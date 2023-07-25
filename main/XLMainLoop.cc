/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLMainLoop.h"
#include "XLEvent.h"
#include "XLEventHandler.h"
#include "XLResourceCache.h"
#include "XLCoreDevice.h"
#include "XLCoreQueue.h"

#if MODULE_XENOLITH_FONT

#include "XLFontLibrary.h"

#endif

namespace stappler::xenolith {

static MainLoop *s_mainLoop = nullptr;

MainLoop *MainLoop::getInstance() {
	return s_mainLoop;
}

MainLoop::~MainLoop() {
	if (_updatePool) {
		memory::pool::destroy(_updatePool);
	}

	_instance = nullptr;
}

bool MainLoop::init(StringView name, Rc<core::Instance> &&instance) {
	if (instance == nullptr) {
		return false;
	}

	_name = name;
	_instance = move(instance);
	_updatePool = memory::pool::create((memory::pool_t *)nullptr);
	return true;
}

void MainLoop::run(const CallbackInfo &cb, core::LoopInfo &&loopInfo, uint32_t threadsCount, TimeInterval iv) {
	s_mainLoop = this;

	_shouldQuit.test_and_set();
	_threadId = std::this_thread::get_id();
	_resourceCache = Rc<ResourceCache>::create();

	auto tmpStarted = move(loopInfo.onDeviceStarted);
	auto tmpFinalized = move(loopInfo.onDeviceFinalized);

	loopInfo.onDeviceStarted = [this, tmpStarted = move(tmpStarted)] (const core::Loop &loop, const core::Device &dev) {
		if (tmpStarted) {
			tmpStarted(loop, dev);
		}

		handleDeviceStarted(loop, dev);
	};
	loopInfo.onDeviceFinalized = [this, tmpFinalized = move(tmpFinalized)] (const core::Loop &loop, const core::Device &dev) {
		handleDeviceFinalized(loop, dev);

		if (tmpFinalized) {
			tmpFinalized(loop, dev);
		}
	};

	_glLoop = _instance->makeLoop(move(loopInfo));

	if (!spawnWorkers(thread::TaskQueue::Flags::Waitable, 0, threadsCount, _name)) {
		log::text("MainLoop", "Fail to spawn worker threads");
		return;
	}

	if (cb.initCallback) {
		cb.initCallback(*this);
	}

	_glLoop->waitRinning();

#if MODULE_XENOLITH_FONT
	_fontLibrary = Rc<font::FontLibrary>::create(this, _instance->makeFontQueue());

	auto builder = _fontLibrary->makeDefaultControllerBuilder("ApplicationFontController");

	updateDefaultFontController(builder);

	_fontController = _fontLibrary->acquireController(move(builder));
#endif

	uint32_t count = 0;
	uint64_t clock = platform::clock(core::ClockType::Monotonic);
	uint64_t lastUpdate = clock;
	uint64_t startTime = clock;

	_time.delta = 0;
	_time.global = clock;
	_time.app = 0;
	_time.dt = 0.0f;

	update(cb, _time);

	do {
		count = 0;
		if (!_immediateUpdate) {
			wait(iv - TimeInterval::microseconds(clock - lastUpdate), &count);
		}
		if (count > 0) {
			memory::pool::push(_updatePool);
			TaskQueue::update();
			memory::pool::pop();
			memory::pool::clear(_updatePool);
		}
		clock = platform::clock(core::ClockType::Monotonic);

		auto dt = TimeInterval::microseconds(clock - lastUpdate);
		if (dt >= iv || _immediateUpdate) {
			_time.delta = dt.toMicros();
			_time.global = clock;
			_time.app = startTime - clock;
			_time.dt = float(_time.delta) / 1'000'000;

			update(cb, _time);
			lastUpdate = clock;
			_immediateUpdate = false;
		}
	} while (_shouldQuit.test_and_set());

	if (cb.finalizeCallback) {
		cb.finalizeCallback(*this);
	}

#if MODULE_XENOLITH_FONT
	_fontController->invalidate();
	_fontController = nullptr;

	_fontLibrary->invalidate();
	_fontLibrary = nullptr;
#endif

	_glLoop->cancel();

	waitForAll();
	TaskQueue::update();

	_glLoop = nullptr;
	_resourceCache = nullptr;

	if (s_mainLoop == this) {
		s_mainLoop = nullptr;
	}

	cancelWorkers();
}

void MainLoop::end() const {
	_shouldQuit.clear();
}

void MainLoop::scheduleUpdate() {
	if (isOnMainThread()) {
		_immediateUpdate = true;
	} else {
		onMainThread([this] {
			_immediateUpdate = true;
		}, this);
	}
}

bool MainLoop::isOnMainThread() const {
	return _threadId == std::this_thread::get_id();
}

void MainLoop::performOnGlThread(Function<void()> &&func, Ref *target, bool immediate) const {
	_glLoop->performOnGlThread(move(func), target, immediate);
}

void MainLoop::performOnMainThread(Function<void()> &&func, Ref *target, bool onNextFrame) {
	if (isOnMainThread() && !onNextFrame) {
		func();
	} else {
		TaskQueue::onMainThread(Rc<Task>::create([func = move(func)] (const thread::Task &, bool success) {
			if (success) { func(); }
		}, target));
	}
}

void MainLoop::performOnMainThread(Rc<Task> &&task, bool onNextFrame) {
	if (isOnMainThread() && !onNextFrame) {
		task->onComplete();
	} else {
		onMainThread(std::move(task));
	}
}

void MainLoop::perform(ExecuteCallback &&exec, CompleteCallback &&complete, Ref *obj) {
	perform(Rc<Task>::create(move(exec), move(complete), obj));
}

void MainLoop::perform(Rc<Task> &&task) {
	TaskQueue::perform(std::move(task));
}

void MainLoop::perform(Rc<Task> &&task, bool performFirst) {
	TaskQueue::perform(std::move(task), performFirst);
}

void MainLoop::addEventListener(const EventHandlerNode *listener) {
	auto it = _eventListeners.find(listener->getEventID());
	if (it != _eventListeners.end()) {
		it->second.insert(listener);
	} else {
		_eventListeners.emplace(listener->getEventID(), HashSet<const EventHandlerNode *>{listener});
	}
}

void MainLoop::removeEventListner(const EventHandlerNode *listener) {
	auto it = _eventListeners.find(listener->getEventID());
	if (it != _eventListeners.end()) {
		it->second.erase(listener);
	}
}

void MainLoop::removeAllEventListeners() {
	_eventListeners.clear();
}

void MainLoop::dispatchEvent(const Event &ev) {
	if (_eventListeners.size() > 0) {
		auto it = _eventListeners.find(ev.getHeader().getEventID());
		if (it != _eventListeners.end() && it->second.size() != 0) {
			Vector<const EventHandlerNode *> listenersToExecute;
			auto &listeners = it->second;
			for (auto l : listeners) {
				if (l->shouldRecieveEventWithObject(ev.getEventID(), ev.getObject())) {
					listenersToExecute.push_back(l);
				}
			}

			for (auto l : listenersToExecute) {
				l->onEventRecieved(ev);
			}
		}
	}
}

uint64_t MainLoop::getClock() const {
	return platform::clock(core::ClockType::Monotonic);
}

thread::TaskQueue *MainLoop::getQueue() {
	return this;
}

void MainLoop::update(const CallbackInfo &cb, const UpdateTime &t) {
	memory::pool::push(_updatePool);
	if (cb.updateCallback) {
		cb.updateCallback(*this, t);
	}
	memory::pool::pop();
	memory::pool::clear(_updatePool);
}

void MainLoop::handleDeviceStarted(const core::Loop &loop, const core::Device &dev) {
	auto emptyObject = dev.getEmptyImageObject();
	auto solidObject = dev.getSolidImageObject();

	performOnMainThread([this, emptyObject = dev.getEmptyImageObject(), solidObject = dev.getSolidImageObject()] () {
		_resourceCache->addImage(core::EmptyTextureName, emptyObject);
		_resourceCache->addImage(core::SolidTextureName, solidObject);
	});
}

void MainLoop::handleDeviceFinalized(const core::Loop &loop, const core::Device &dev) {
	performOnMainThread([cache = _resourceCache] {
		cache->invalidate();
	}, Rc<core::Device>(const_cast<core::Device *>(&dev)));
}

#if MODULE_XENOLITH_FONT
void MainLoop::updateDefaultFontController(font::FontController::Builder &builder) {

}
#endif

}
