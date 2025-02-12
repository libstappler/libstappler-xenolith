/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "XLPlatformApplication.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

Value ApplicationInfo::encode() const {
	Value ret;
	ret.setString(bundleName, "bundleName");
	ret.setString(applicationName, "applicationName");
	ret.setString(applicationVersion, "applicationVersion");
	ret.setString(userLanguage, "userLanguage");
	if (!launchUrl.empty()) {
		ret.setString(userLanguage, "userLanguage");
	}
	ret.setValue(Value({Value(screenSize.width), Value(screenSize.height)}), "screenSize");
	ret.setValue(Value({Value(viewDecoration.top), Value(viewDecoration.right),
		Value(viewDecoration.bottom), Value(viewDecoration.left)}), "viewDecoration");
	ret.setDouble(density, "density");
	if (isPhone) {
		ret.setBool(isPhone, "isPhone");
	}
	if (isFixed) {
		ret.setBool(isFixed, "isFixed");
	}
	if (renderdoc) {
		ret.setBool(renderdoc, "renderdoc");
	}
	if (!validation) {
		ret.setBool(validation, "validation");
	}
	if (verbose) {
		ret.setBool(verbose, "verbose");
	}
	if (help) {
		ret.setBool(help, "help");
	}
	return ret;
}

int ApplicationInfo::parseCmdSwitch(ApplicationInfo &ret, char c, const char *str) {
	if (c == 'h') {
		ret.help = true;
	} else if (c == 'v') {
		ret.verbose = true;
	}
	return 1;
}

int ApplicationInfo::parseCmdString(ApplicationInfo &ret, const StringView &str, int argc, const char * argv[]) {
	if (str == "help") {
		ret.help = true;
	} else if (str == "verbose") {
		ret.verbose = true;
	} else if (str.starts_with("w=")) {
		auto s = str.sub(2).readInteger().get(0);
		if (s > 0) {
			ret.screenSize.width = uint32_t(s);
		}
	} else if (str.starts_with("h=")) {
		auto s = str.sub(2).readInteger().get(0);
		if (s > 0) {
			ret.screenSize.height = uint32_t(s);
		}
	} else if (str.starts_with("d=")) {
		auto s = str.sub(2).readDouble().get(0.0);
		if (s > 0) {
			ret.density = s;
		}
	} else if (str.starts_with("l=")) {
		ret.userLanguage = str.sub(2).str<Interface>();
	} else if (str.starts_with("locale=")) {
		ret.userLanguage = str.sub("locale="_len).str<Interface>();
	} else if (str == "phone") {
		ret.isPhone = true;
	} else if (str.starts_with("bundle=")) {
		ret.bundleName = str.sub("bundle="_len).str<Interface>();
	} else if (str == "fixed") {
		ret.isFixed = true;
	} else if (str == "renderdoc") {
		ret.renderdoc = true;
	} else if (str == "novalidation") {
		ret.validation = false;
	} else if (str.starts_with("decor=")) {
		auto values = str.sub(6);
		float f[4] = { nan(), nan(), nan(), nan() };
		uint32_t i = 0;
		values.split<StringView::Chars<','>>([&] (StringView val) {
			if (i < 4) {
				f[i] = val.readFloat().get(nan());
			}
			++ i;
		});
		if (!isnan(f[0])) {
			if (isnan(f[1])) {
				f[1] = f[0];
			}
			if (isnan(f[2])) {
				f[2] = f[0];
			}
			if (isnan(f[3])) {
				f[3] = f[1];
			}
			ret.viewDecoration = Padding(f[0], f[1], f[2], f[3]);
		}
	}
	return 1;
}

PlatformApplication::~PlatformApplication() {
	_instance = nullptr;
}


PlatformApplication::PlatformApplication() { }

bool PlatformApplication::init(ApplicationInfo &&info, Rc<core::Instance> &&instance) {
	if (instance == nullptr) {
		return false;
	}

	_info = move(info);
	_info.applicationVersionCode = XL_MAKE_API_VERSION(_info.applicationVersion);

	_queue = Rc<thread::TaskQueue>::alloc("Application::Queue", [] () {
		//wakeup();
	});

	_instance = move(instance);

	return true;
}

void PlatformApplication::run() {
	platformInitialize();
	Thread::run();
}

void PlatformApplication::waitRunning() {
	Thread::waitRunning();
}

void PlatformApplication::waitStopped() {
	platformWaitExit();
}

void PlatformApplication::threadInit() {
	_thisThreadId = std::this_thread::get_id();

	thread::ThreadInfo::setThreadInfo("Application");

	if (_info.loopInfo.onDeviceStarted) {
		auto tmpStarted = sp::move(_info.loopInfo.onDeviceStarted);
		_info.loopInfo.onDeviceStarted = [this, tmpStarted = sp::move(tmpStarted)] (const core::Loop &loop, const core::Device &dev) {
			if (tmpStarted) {
				tmpStarted(loop, dev);
			}

			handleDeviceStarted(loop, dev);
		};
	} else {
		_info.loopInfo.onDeviceStarted = [this] (const core::Loop &loop, const core::Device &dev) {
			handleDeviceStarted(loop, dev);
		};
	}

	if (_info.loopInfo.onDeviceFinalized) {
		auto tmpFinalized = sp::move(_info.loopInfo.onDeviceFinalized);
		_info.loopInfo.onDeviceFinalized = [this, tmpFinalized = sp::move(tmpFinalized)] (const core::Loop &loop, const core::Device &dev) {
			handleDeviceFinalized(loop, dev);

			if (tmpFinalized) {
				tmpFinalized(loop, dev);
			}
		};
	} else {
		_info.loopInfo.onDeviceFinalized = [this] (const core::Loop &loop, const core::Device &dev) {
			handleDeviceFinalized(loop, dev);
		};
	}

	auto loop = _instance->makeLoop(move(_info.loopInfo));

	if (!_queue->spawnWorkers(0, _info.threadsCount, "Application::Queue")) {
		log::error("MainLoop", "Fail to spawn worker threads");
		return;
	}

	if (_info.initCallback) {
		_info.initCallback(*this);
	}

	loadExtensions();

	loop->waitRunning();

	_glLoop = move(loop);

	initializeExtensions();

	_extensionsInitialized = true;

	for (auto &it : _glWaitCallback) {
		_glLoop->performOnGlThread(sp::move(it.func), it.target, it.immediate);
	}
	_glWaitCallback.clear();

	_time.delta = 0;
	_time.global = platform::clock(core::ClockType::Monotonic);
	_time.app = 0;
	_time.dt = 0.0f;

	performAppUpdate(_time);

	Thread::threadInit();
}

void PlatformApplication::threadDispose() {
	if (_info.finalizeCallback) {
		_info.finalizeCallback(*this);
	}

	_glLoop->stop();
	_glLoop->waitStopped();

	finalizeExtensions();

	_queue->waitForAll();
	_queue->update();

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
	_queue->cancelWorkers();

	platformSignalExit();

	Thread::threadDispose();
}

bool PlatformApplication::worker() {
	uint32_t count = 0;
	_startTime = _lastUpdate = _clock = _time.global;

	count = 0;
	if (!_immediateUpdate) {
		_queue->wait(_info.updateInterval - TimeInterval::microseconds(_clock - _lastUpdate), &count);
	}
	if (count > 0) {
		_queue->update();
	}

	performTimersUpdate(_immediateUpdate);

	return _continueExecution.test_and_set();
}

void PlatformApplication::end()  {
	stop();
	if (!isOnThisThread()) {
		wakeup();
	}
}

void PlatformApplication::wakeup() {
	if (isOnThisThread()) {
		_immediateUpdate = true;
	} else {
		performOnMainThread([this] {
			_immediateUpdate = true;
		}, this);
	}
}

void PlatformApplication::performOnGlThread(Function<void()> &&func, Ref *target, bool immediate) const {
	if (_glLoop) {
		_glLoop->performOnGlThread(sp::move(func), target, immediate);
	} else {
		_glWaitCallback.emplace_back(WaitCallbackInfo{sp::move(func), target, immediate});
	}
}

void PlatformApplication::performOnMainThread(Function<void()> &&func, Ref *target, bool onNextFrame) const {
	if (isOnThisThread() && !onNextFrame) {
		func();
	} else {
		_queue->onMainThread(Rc<Task>::create([func = sp::move(func)] (const thread::Task &, bool success) {
			if (success) { func(); }
		}, target));
	}
}

void PlatformApplication::performOnMainThread(Rc<Task> &&task, bool onNextFrame) const {
	if (isOnThisThread() && !onNextFrame) {
		task->onComplete();
	} else {
		_queue->onMainThread(sp::move(task));
	}
}

void PlatformApplication::perform(ExecuteCallback &&exec, CompleteCallback &&complete, Ref *obj) const {
	perform(Rc<Task>::create(sp::move(exec), sp::move(complete), obj));
}

void PlatformApplication::perform(Rc<Task> &&task) const {
	_queue->perform(sp::move(task));
}

void PlatformApplication::perform(Rc<Task> &&task, bool performFirst) const {
	_queue->perform(sp::move(task), performFirst);
}

void PlatformApplication::updateMessageToken(BytesView tok) {
	if (_messageToken != tok) {
		_messageToken = tok.bytes<Interface>();
	}
}

void PlatformApplication::receiveRemoteNotification(Value &&val) {

}

void PlatformApplication::handleDeviceStarted(const core::Loop &loop, const core::Device &dev) {
	_device = &dev;
}

void PlatformApplication::handleDeviceFinalized(const core::Loop &loop, const core::Device &dev) {
	_device = nullptr;
}

void PlatformApplication::loadExtensions() {

}

void PlatformApplication::initializeExtensions() {

}

void PlatformApplication::finalizeExtensions() {

}

void PlatformApplication::performAppUpdate(const UpdateTime &t) {
	if (_info.updateCallback) {
		_info.updateCallback(*this, t);
	}
}

void PlatformApplication::performTimersUpdate(bool forced) {
	_clock = platform::clock(core::ClockType::Monotonic);

	auto dt = TimeInterval::microseconds(_clock - _lastUpdate);
	if (dt >= _info.updateInterval || forced) {
		_time.delta = dt.toMicros();
		_time.global = _clock;
		_time.app = _startTime - _clock;
		_time.dt = float(_time.delta) / 1'000'000;

		performAppUpdate(_time);
		_lastUpdate = _clock;
		_immediateUpdate = false;
	}
}

}

//
// PlatformApplication
//

#if LINUX

#include <stdlib.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

void PlatformApplication::openUrl(StringView url) const {
	if (::system(toString("xdg-open ", url).data()) != 0) {
		log::error("xenolith::Application", "Fail to open external url: ", url);
	}
}

void PlatformApplication::platformInitialize() { }

void PlatformApplication::platformWaitExit() {
	Thread::waitStopped();
}

void PlatformApplication::platformSignalExit() { }

}

#endif


#if ANDROID

#include "android/XLPlatformAndroidActivity.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

void PlatformApplication::openUrl(StringView url) const {
	auto activity = static_cast<platform::Activity *>(_info.platformHandle);

	activity->openUrl(url);
}

void PlatformApplication::platformInitialize() { }

void PlatformApplication::platformWaitExit() {
	Thread::waitStopped();
}

void PlatformApplication::platformSignalExit() { }

}

#endif


#if MACOS

#include "XLPlatformMacos.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

void PlatformApplication::openUrl(StringView url) const {
	platform::openUrl(url);
}

void PlatformApplication::platformInitialize() {
	_shouldSignalOnExit = platform::isOnMainThread();
}

void PlatformApplication::platformWaitExit() {
	if (_shouldSignalOnExit) {
		if (platform::isOnMainThread()) {
			platform::runApplication();
		} else {
			log::error("xenolith::PlatformApplication", "If application was runned from main thread, waitFinalized should be also called in main thread");
		}
	} else {
		Thread::waitStopped();
	}
}

void PlatformApplication::platformSignalExit() {
	if (_shouldSignalOnExit) {
		platform::performOnMainThread([this] () {
			_thread.join();
			platform::stopApplication();
		}, this);
	}
}

}

#endif
