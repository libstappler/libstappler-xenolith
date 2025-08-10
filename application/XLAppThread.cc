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
#include "XLEvent.h"

#if MODULE_XENOLITH_FONT

#include "XLFontComponent.h"
#include "XLFontLocale.h"

#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

XL_DECLARE_EVENT_CLASS(AppThread, onNetworkState)
XL_DECLARE_EVENT_CLASS(AppThread, onThemeInfo)

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
		data->performUpdate(false);
	}),
		.interval = _context->getInfo()->appUpdateInterval,
		.count = event::TimerInfo::Infinite,
	});

	loadExtensions();

	_context->handleAppThreadCreated(this);

	initializeExtensions();

	_time.delta = 0;
	_time.global = sp::platform::clock(ClockType::Monotonic);
	_time.app = 0;
	_time.dt = 0.0f;

	performUpdate(true);

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
	performOnAppThread([this] { performUpdate(true); }, this, true);
}

void AppThread::handleNetworkStateChanged(NetworkFlags flags) {
	performOnAppThread([this, flags] {
		if (flags != _networkFlags) {
			_networkFlags = flags;
			for (auto &it : _extensions) { it.second->handleNetworkStateChanged(flags); }
			onNetworkState(this, int64_t(toInt(flags)));
		}
	}, this);
}

void AppThread::handleThemeInfoChanged(const ThemeInfo &theme) {
	performOnAppThread([this, theme] {
		if (theme != _themeInfo) {
			_themeInfo = theme;
			for (auto &it : _extensions) { it.second->handleThemeInfoChanged(theme); }
			onThemeInfo(this, _themeInfo.encode());
		}
	}, this);
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

void AppThread::readFromClipboard(Function<void(BytesView, StringView)> &&cb,
		Function<StringView(SpanView<StringView>)> &&tcb, Ref *ref) {
	_context->performOnThread(
			[this, cb = sp::move(cb), tcb = sp::move(tcb), ref = Rc<Ref>(ref)]() mutable {
		_context->readFromClipboard(
				[this, cb = sp::move(cb), ref = sp::move(ref)](BytesView data,
						StringView type) mutable {
			performOnAppThread(
					[data = data.bytes<Interface>(), type = type.str<Interface>(),
							cb = sp::move(cb), ref = move(ref)]() mutable {
				cb(data, type);
				ref = nullptr;
			},
					this);
		},
				sp::move(tcb), this);
	}, this);
}

void AppThread::writeToClipboard(BytesView data, StringView contentType, Ref *ref) {
	_context->performOnThread(
			[this, data = data.bytes<Interface>(), type = contentType.str<Interface>(),
					ref = Rc<Ref>(ref)]() mutable {
		_context->writeToClipboard([data = sp::move(data), t = type](StringView type) -> Bytes {
			if (t == type) {
				return data;
			}
			return Bytes();
		}, makeSpanView(&type, 1), ref);
	},
			this);
}

void AppThread::writeToClipboard(Function<Bytes(StringView)> &&cb, SpanView<StringView> types,
		Ref *ref) {
	Vector<String> vtypes;
	vtypes.reserve(types.size());
	for (auto &it : types) { vtypes.emplace_back(it.str<Interface>()); }
	_context->performOnThread(
			[this, cb = sp::move(cb), vtypes = sp::move(vtypes), ref = Rc<Ref>(ref)]() mutable {
		_context->writeToClipboard(sp::move(cb), vtypes, ref);
	}, this);
}

void AppThread::acquireScreenInfo(Function<void(NotNull<ScreenInfo>)> &&cb, Ref *ref) {
	_context->performOnThread([this, cb = sp::move(cb), ref = Rc<Ref>(ref)]() mutable {
		auto info = _context->getScreenInfo();
		performOnAppThread([cb = sp::move(cb), ref = move(ref), info = move(info)]() mutable {
			cb(info);
			ref = nullptr;
			info = nullptr;
		}, this);
	}, this);
}

bool AppThread::addListener(NotNull<Ref> ref, Function<void(const UpdateTime &, bool)> &&cb) {
	auto it = _listeners.find(ref);
	if (it == _listeners.end()) {
		_listeners.emplace(ref, sp::move(cb));
		return true;
	}
	return false;
}

bool AppThread::removeListener(NotNull<Ref> ref) {
	auto it = _listeners.find(ref);
	if (it != _listeners.end()) {
		_listeners.erase(it);
		return true;
	}
	return false;
}

void AppThread::performAppUpdate(const UpdateTime &time, bool wakeup) {
	_context->handleAppThreadUpdate(this, time);
	for (auto &it : _extensions) { it.second->update(this, time, wakeup); }

	auto listeners = _listeners;
	for (auto &it : listeners) { it.second(time, wakeup); }
}

void AppThread::performUpdate(bool wakeup) {
	_clock = sp::platform::clock(ClockType::Monotonic);

	_time.delta = _clock - _lastUpdate;
	_time.global = _clock;
	_time.app = _startTime - _clock;
	_time.dt = float(_time.delta) / 1'000'000;

	performAppUpdate(_time, wakeup);

	_lastUpdate = _clock;
}

void AppThread::loadExtensions() {
	_resourceCache = addExtension(Rc<ResourceCache>::create(this));

#if MODULE_XENOLITH_FONT
	auto createFontController = SharedModule::acquireTypedSymbol<
			decltype(&font::FontComponent::createDefaultController)>(
			buildconfig::MODULE_XENOLITH_FONT_NAME, "FontComponent::createDefaultController");

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
