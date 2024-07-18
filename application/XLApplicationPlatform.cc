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

#if ANDROID

#include "android/XLPlatformAndroidActivity.h"

#define XL_APPLICATION_MAIN_LOOP_DEFAULT 1

namespace STAPPLER_VERSIONIZED stappler::xenolith {

void Application::nativeInit() {
	auto activity = static_cast<platform::Activity *>(_info.nativeHandle);

	auto tok = activity->getMessageToken();
	if (!tok.empty()) {
		handleMessageToken(move(tok));
	}
	activity->addTokenCallback(this, [this] (StringView str) {
		performOnMainThread([this, str = str.str<Interface>()] () mutable {
			handleMessageToken(move(str));
		}, this);
	});

	activity->addRemoteNotificationCallback(this, [this] (const Value &val) {
		performOnMainThread([this, val = val] () mutable {
			handleRemoteNotification(move(val));
		}, this);
	});
}

void Application::nativeDispose() {
	auto activity = static_cast<platform::Activity *>(_info.nativeHandle);

	activity->removeTokenCallback(this);
	activity->removeRemoteNotificationCallback(this);
}

void Application::openUrl(StringView url) const {
	auto activity = static_cast<platform::Activity *>(_info.nativeHandle);

	activity->openUrl(url);
}

}

#endif


#if LINUX

#include <stdlib.h>

#define XL_APPLICATION_MAIN_LOOP_DEFAULT 1

namespace STAPPLER_VERSIONIZED stappler::xenolith {

void Application::nativeInit() { }

void Application::nativeDispose() { }

void Application::openUrl(StringView url) const {
	::system(toString("xdg-open ", url).data());
}

}

#endif


#if WIN32

#include "SPPlatformUnistd.h"
#include <shellapi.h>

#define XL_APPLICATION_MAIN_LOOP_DEFAULT 1

namespace STAPPLER_VERSIONIZED stappler::xenolith {

void Application::nativeInit() {
	platform::clock(core::ClockType::Monotonic);
}

void Application::nativeDispose() { }

void Application::openUrl(StringView url) const {
	ShellExecute(0, 0, (wchar_t *)string::toUtf16<Interface>(url).data(), 0, 0 , SW_SHOW );
}

}

#endif

#if MACOS

#include "macos/XLPlatformMacos.h"
#include <CoreFoundation/CFRunLoop.h>

#define XL_APPLICATION_MAIN_LOOP_DEFAULT 0

namespace STAPPLER_VERSIONIZED stappler::xenolith {

struct MacosApplicationData {
	CFRunLoopRef loop = nullptr;
	memory::pool_t *updatePool = nullptr;
	CFRunLoopSourceRef updateSource = nullptr;
	CFRunLoopTimerRef updateTimer = nullptr;
	thread::TaskQueue *queue = nullptr;
	Application *app = nullptr;

	uint64_t clock = 0;
	uint64_t lastUpdate = 0;
	uint64_t startTime = 0;
	const Application::CallbackInfo *cb = nullptr;
};

void Application::nativeInit() {
	platform::clock(core::ClockType::Monotonic);

	platform::initMacApplication();

	auto data = new MacosApplicationData;
	data->updatePool = _updatePool;
	data->loop = CFRunLoopGetCurrent();
	data->queue = this;
	data->app = this;

	CFRunLoopSourceContext ctx = CFRunLoopSourceContext{
		.version = 0,
	   .info = data,
	   .retain = nullptr,
	   .release = nullptr,
	   .copyDescription = nullptr,
	   .equal = nullptr,
	   .hash = nullptr,
	   .schedule = nullptr,
	   .cancel = nullptr,
	   .perform = [] (void *info) {
			auto data = (MacosApplicationData *)info;

			memory::pool::push(data->updatePool);
			data->queue->update(nullptr);
			memory::pool::pop();
			memory::pool::clear(data->updatePool);

			// forced timing update
			if (data->app && data->cb) {
				data->app->performTimersUpdate(*data->cb, true);
			}

			if (data->app && !data->app->_shouldQuit.test_and_set()) {
				data->app->nativeStop();
			}
		}
	};

	data->updateSource = CFRunLoopSourceCreate(nullptr, 0, &ctx);

	CFRunLoopAddSource(data->loop, data->updateSource, kCFRunLoopDefaultMode);

	lock();

	_info.nativeHandle = data;

	unlock();
}

void Application::nativeDispose() {
	lock();
	auto data = (MacosApplicationData *)_info.nativeHandle;
	_info.nativeHandle = nullptr;
	unlock();

	CFRelease(data->updateSource);
	delete data;

}

void Application::nativeWakeup() {
	lock();
	auto data = (MacosApplicationData *)_info.nativeHandle;
	unlock();

	if (data) {
		CFRunLoopSourceSignal(data->updateSource);
	}
}

void Application::nativeRunMainLoop(const CallbackInfo &cb) {
	auto data = (MacosApplicationData *)_info.nativeHandle;
	data->startTime = data->lastUpdate = data->clock = _time.global;
	data->cb = &cb;

	CFRunLoopTimerContext tctx = CFRunLoopTimerContext{
		.version = 0,
		.info = data,
		.retain = nullptr,
		.release = nullptr,
		.copyDescription = nullptr,
	};

	data->updateTimer = CFRunLoopTimerCreate(nullptr, 0, double(_updateInterval.toMicros()) / 1'000'000.0, 0, 0, [] (CFRunLoopTimerRef timer, void *info) {
		auto data = (MacosApplicationData *)info;
		// timing already adjusted by OS
		data->app->performTimersUpdate(*data->cb, true);
	}, &tctx);

	CFRunLoopAddTimer(data->loop, data->updateTimer, kCFRunLoopDefaultMode);

	platform::runMacApplication();

	CFRunLoopRemoveTimer(data->loop, data->updateTimer, kCFRunLoopDefaultMode);
	CFRelease(data->updateTimer);

	data->cb = nullptr;
	data->updateTimer = nullptr;
}

void Application::nativeStop() {
	platform::stopMacApplication();
}

void Application::openUrl(StringView url) const {
	//ShellExecute(0, 0, (wchar_t *)string::toUtf16<Interface>(url).data(), 0, 0 , SW_SHOW );
}

}

#endif

#if XL_APPLICATION_MAIN_LOOP_DEFAULT

namespace STAPPLER_VERSIONIZED stappler::xenolith {

void Application::nativeWakeup() { }

void Application::nativeWait(TimeInterval iv, uint32_t *count) {
	wait(iv, count);
}

void Application::nativeRunMainLoop(const CallbackInfo &cb, TimeInterval iv) {
	uint32_t count = 0;
	uint64_t clock = _time.global;
	uint64_t lastUpdate = clock;
	uint64_t startTime = clock;

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

		performTimersUpdate(cb, _immediateUpdate);
	} while (_shouldQuit.test_and_set());
}

}
#endif
