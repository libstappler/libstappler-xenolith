/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "XLAppThread.h"
#include "XLContext.h"
#include "SPSharedModule.h"
#include "SPEventTimerHandle.h"

#if MODULE_XENOLITH_FONT

#include "XLFontComponent.h"
#include "XLFontLocale.h"

#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

bool AppThread::init(NotNull<Context> ctx) {
	_context = ctx;
	return true;
}

void AppThread::run() { Thread::run(); }

void AppThread::threadInit() {
	_thisThreadId = std::this_thread::get_id();

	_appLooper = event::Looper::acquire(event::LooperInfo{.name = StringView("Application"),
		.workersCount = _context->getInfo()->appThreadsCount,

		// Disable ALooper for internal queue, it can not be stopped gracefully
		.engineMask = event::QueueEngine::Any & ~event::QueueEngine::ALooper});

	_timer = _appLooper->scheduleTimer(event::TimerInfo{
		.completion = event::TimerInfo::Completion::create<AppThread>(this,
				[](AppThread *data, event::TimerHandle *self, uint32_t value, Status status) {
		data->performUpdate();
	}),
		.interval = _context->getInfo()->appUpdateInterval,
		.count = event::TimerInfo::Infinite,
	});

	_context->handleAppThreadCreated(this);

	_time.delta = 0;
	_time.global = sp::platform::clock(ClockType::Monotonic);
	_time.app = 0;
	_time.dt = 0.0f;

	performUpdate();

	Thread::threadInit();
}

void AppThread::threadDispose() {
	_context->handleAppThreadDestroyed(this);

	_timer->cancel();
	_timer = nullptr;

	finalizeExtensions();

	_appLooper = nullptr;

	Thread::threadDispose();
}

bool AppThread::worker() {
	_startTime = _lastUpdate = _clock = _time.global;

	_appLooper->run();

	return _continueExecution.test_and_set();
}

void AppThread::stop() {
	Thread::stop();

	_appLooper->wakeup(event::WakeupFlags::Graceful | event::WakeupFlags::SuspendThreads);
}

void AppThread::wakeup() {
	performOnAppThread([this] { performUpdate(); }, this, true);
}

void AppThread::performOnAppThread(Function<void()> &&func, Ref *target, bool onNextFrame,
		StringView tag) {
	if (isOnThisThread() && !onNextFrame) {
		func();
	} else {
		waitRunning();
		_appLooper->performOnThread(sp::move(func), target, !onNextFrame, tag);
	}
}

void AppThread::performOnAppThread(Rc<Task> &&task, bool onNextFrame) {
	if (isOnThisThread() && !onNextFrame) {
		task->handleCompleted();
	} else {
		waitRunning();
		_appLooper->performOnThread(sp::move(task));
	}
}

void AppThread::perform(ExecuteCallback &&exec, CompleteCallback &&complete, Ref *obj) const {
	perform(Rc<Task>::create(sp::move(exec), sp::move(complete), obj));
}

void AppThread::perform(Rc<Task> &&task) const { _appLooper->performAsync(sp::move(task)); }

void AppThread::perform(Rc<Task> &&task, bool performFirst) const {
	_appLooper->performAsync(sp::move(task), performFirst);
}

void AppThread::performAppUpdate(const UpdateTime &time) {
	_context->handleAppThreadUpdate(this, time);
	for (auto &it : _extensions) { it.second->update(this, time); }
}

void AppThread::performUpdate() {
	_clock = sp::platform::clock(ClockType::Monotonic);

	_time.delta = _clock - _lastUpdate;
	_time.global = _clock;
	_time.app = _startTime - _clock;
	_time.dt = float(_time.delta) / 1'000'000;

	performAppUpdate(_time);

	_lastUpdate = _clock;
}

void AppThread::loadExtensions() {
	_resourceCache = addExtension(Rc<ResourceCache>::create(this));

#if MODULE_XENOLITH_FONT
	auto createFontController = SharedModule::acquireTypedSymbol<
			decltype(&font::FontComponent::createDefaultController)>(
			buildconfig::MODULE_XENOLITH_FONT_NAME, "FontExtension::createDefaultController");

	if (createFontController) {
		auto comp = _context->getComponent<font::FontComponent>();
		if (comp) {
			if (auto controller =
							createFontController(comp, _appLooper, "ApplicationFontController")) {
				addExtension(move(controller));
			}
		}
	}
#endif
}

void AppThread::initializeExtensions() {
	for (auto &it : _extensions) { it.second->initialize(this); }
	_extensionsInitialized = true;
}

void AppThread::finalizeExtensions() {
	for (auto &it : _extensions) { it.second->invalidate(this); }
}

} // namespace stappler::xenolith
