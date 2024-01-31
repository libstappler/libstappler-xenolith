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

#include "XLApplication.h"
#include "XLEvent.h"
#include "XLEventHandler.h"
#include "XLResourceCache.h"
#include "XLCoreDevice.h"
#include "XLCoreQueue.h"


#if MODULE_XENOLITH_SCENE

#include "XLView.h"

#endif


#if MODULE_XENOLITH_FONT

#include "XLFontLibrary.h"

#endif


namespace STAPPLER_VERSIONIZED stappler::xenolith {

static Application *s_mainLoop = nullptr;

XL_DECLARE_EVENT_CLASS(Application, onMessageToken)
XL_DECLARE_EVENT_CLASS(Application, onRemoteNotification)

Application *Application::getInstance() {
	return s_mainLoop;
}

Application::~Application() {
	if (_updatePool) {
		memory::pool::destroy(_updatePool);
	}

	_instance = nullptr;

	if (s_mainLoop == this) {
		s_mainLoop = nullptr;
	}
}

bool Application::init(CommonInfo &&info, Rc<core::Instance> &&instance) {
	if (instance == nullptr) {
		return false;
	}

	s_mainLoop = this;

	_info = move(info);
	_info.applicationVersionCode = XL_MAKE_API_VERSION(_info.applicationVersion);
	_name = _info.applicationName;
	_instance = move(instance);
	_updatePool = memory::pool::create(static_cast<memory::pool_t *>(nullptr));
	return true;
}

void Application::run(const CallbackInfo &cb, core::LoopInfo &&loopInfo, uint32_t threadsCount, TimeInterval iv) {
	_shouldQuit.test_and_set();
	_threadId = std::this_thread::get_id();
	_resourceCache = Rc<ResourceCache>::create(this);

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

	auto loop = _instance->makeLoop(move(loopInfo));

	if (!spawnWorkers(thread::TaskQueue::Flags::Waitable, 0, threadsCount, _name)) {
		log::error("MainLoop", "Fail to spawn worker threads");
		return;
	}

	if (cb.initCallback) {
		cb.initCallback(*this);
	}

	for (auto &it : _extensions) {
		it.second->initialize(this);
	}

	nativeInit();

	loop->waitRinning();

	_glLoop = move(loop);

	_resourceCache->initialize(*_glLoop);

	for (auto &it : _glWaitCallback) {
		_glLoop->performOnGlThread(move(it.func), it.target, it.immediate);
	}
	_glWaitCallback.clear();

#if MODULE_XENOLITH_FONT
	auto lib = Rc<font::FontLibrary>::create(this, _instance->makeFontQueue());

	auto builder = lib->makeDefaultControllerBuilder("ApplicationFontController");

	addExtension(lib->acquireController(move(builder)));
	addExtension(move(lib));
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

	log::debug("Application", "started");

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

	for (auto &it : _extensions) {
		it.second->invalidate(this);
	}

	nativeDispose();

	_glLoop->cancel();

	waitForAll();
	TaskQueue::update();

	_resourceCache->invalidate();

#if SP_REF_DEBUG
	if (_glLoop->getReferenceCount() > 1) {
		auto loop = _glLoop.get();
		_glLoop = nullptr;

		loop->foreachBacktrace([] (uint64_t id, Time time, const std::vector<std::string> &vec) {
			StringStream stream;
			stream << "[" << id << ":" << time.toHttp<Interface>() << "]:\n";
			for (auto &it : vec) {
				stream << "\t" << it << "\n";
			}
			log::debug("core::Loop", stream.str());
		});
	} else {
		_glLoop = nullptr;
	}
#else
	_glLoop = nullptr;
#endif
	_resourceCache = nullptr;

	cancelWorkers();
}

void Application::end()  {
	_shouldQuit.clear();
	if (!isOnMainThread()) {
		wakeup();
	}
}

void Application::wakeup() {
	if (isOnMainThread()) {
		_immediateUpdate = true;
#if MODULE_XENOLITH_SCENE
		for (auto &it : _activeViews) {
			it->setReadyForNextFrame();
		}
#endif
	} else {
		performOnMainThread([this] {
			_immediateUpdate = true;
#if MODULE_XENOLITH_SCENE
			for (auto &it : _activeViews) {
				it->setReadyForNextFrame();
			}
#endif
		}, this);
	}
}

bool Application::isOnMainThread() const {
	return _threadId == std::this_thread::get_id();
}

void Application::performOnGlThread(Function<void()> &&func, Ref *target, bool immediate) const {
	if (_glLoop) {
		_glLoop->performOnGlThread(move(func), target, immediate);
	} else {
		_glWaitCallback.emplace_back(WaitCallbackInfo{move(func), target, immediate});
	}
}

void Application::performOnMainThread(Function<void()> &&func, Ref *target, bool onNextFrame) {
	if (isOnMainThread() && !onNextFrame) {
		func();
	} else {
		TaskQueue::onMainThread(Rc<Task>::create([func = move(func)] (const thread::Task &, bool success) {
			if (success) { func(); }
		}, target));
	}
}

void Application::performOnMainThread(Rc<Task> &&task, bool onNextFrame) {
	if (isOnMainThread() && !onNextFrame) {
		task->onComplete();
	} else {
		onMainThread(std::move(task));
	}
}

void Application::perform(ExecuteCallback &&exec, CompleteCallback &&complete, Ref *obj) {
	perform(Rc<Task>::create(move(exec), move(complete), obj));
}

void Application::perform(Rc<Task> &&task) {
	TaskQueue::perform(std::move(task));
}

void Application::perform(Rc<Task> &&task, bool performFirst) {
	TaskQueue::perform(std::move(task), performFirst);
}

void Application::addEventListener(const EventHandlerNode *listener) {
	auto it = _eventListeners.find(listener->getEventID());
	if (it != _eventListeners.end()) {
		it->second.insert(listener);
	} else {
		_eventListeners.emplace(listener->getEventID(), HashSet<const EventHandlerNode *>{listener});
	}
}

void Application::removeEventListner(const EventHandlerNode *listener) {
	auto it = _eventListeners.find(listener->getEventID());
	if (it != _eventListeners.end()) {
		it->second.erase(listener);
	}
}

void Application::removeAllEventListeners() {
	_eventListeners.clear();
}

void Application::dispatchEvent(const Event &ev) {
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

uint64_t Application::getClock() const {
	return platform::clock(core::ClockType::Monotonic);
}

thread::TaskQueue *Application::getQueue() {
	return this;
}

void Application::update(const CallbackInfo &cb, const UpdateTime &t) {
	memory::pool::push(_updatePool);
	if (cb.updateCallback) {
		cb.updateCallback(*this, t);
	}

	for (auto &it : _extensions) {
		it.second->update(this, t);
	}

	memory::pool::pop();
	memory::pool::clear(_updatePool);
}

void Application::handleDeviceStarted(const core::Loop &loop, const core::Device &dev) {
	log::debug("Application", "handleDeviceStarted");

	auto emptyObject = dev.getEmptyImageObject();
	auto solidObject = dev.getSolidImageObject();

	performOnMainThread([this, emptyObject = dev.getEmptyImageObject(), solidObject = dev.getSolidImageObject()] () {
		_resourceCache->addImage(core::EmptyTextureName, emptyObject);
		_resourceCache->addImage(core::SolidTextureName, solidObject);
	});

	_device = &dev;

#if MODULE_XENOLITH_SCENE
	for (auto &it : _tmpViews) {
		addView(move(it));
	}

	_tmpViews.clear();
#endif
}

void Application::handleDeviceFinalized(const core::Loop &loop, const core::Device &dev) {
	_device = nullptr;

	performOnMainThread([cache = _resourceCache] {
		cache->invalidate();
	}, Rc<core::Device>(const_cast<core::Device *>(&dev)));
}

void Application::handleMessageToken(String &&str) {
	_messageToken = move(str);
	onMessageToken(this, _messageToken);
}

void Application::handleRemoteNotification(Value &&val) {
	onRemoteNotification(this, move(val));
}

#if MODULE_XENOLITH_SCENE
bool Application::addView(ViewInfo &&info) {
	performOnGlThread([this, info = move(info)] () mutable {
		if (_device) {
			if (info.onClosed) {
				auto tmp = move(info.onClosed);
				info.onClosed = [this, tmp = move(tmp)] (xenolith::View &view) {
					performOnMainThread([this, view = Rc<xenolith::View>(&view)] {
						_activeViews.erase(view);
					}, this);
					tmp(view);
				};
			} else {
				info.onClosed = [this] (xenolith::View &view) {
					performOnMainThread([this, view = Rc<xenolith::View>(&view)] {
						_activeViews.erase(view);
					}, this);
				};
			}

			auto v = _instance->makeView(*this, *_device, move(info));
			performOnMainThread([this, v] {
				_activeViews.emplace(v.get());
			}, this);
		} else {
			_tmpViews.emplace_back(move(info));
		}
	}, this);

	return true;
}
#endif

}
