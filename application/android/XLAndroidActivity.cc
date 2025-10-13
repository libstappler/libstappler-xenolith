/**
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

#include "XLAndroidActivity.h"
#include "XLAndroidContextController.h"
#include "XLAndroidInput.h"
#include "XLAppWindow.h"
#include "SPThread.h"

#include <android/native_activity.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static void AppSupportActivity_handleActivityResult(JNIEnv *env, jobject thiz, jlong native_pointer,
		jint request_code, jint result_code, jobject data) {
	auto activity = reinterpret_cast<AndroidActivity *>(native_pointer);
	if (activity) {
		activity->handleActivityResult(request_code, result_code, data);
	}
}

static void AppSupportActivity_handleContentInsets(JNIEnv *env, jobject thiz, jlong native_pointer,
		jint top, jint right, jint bottom, jint left) {
	auto activity = reinterpret_cast<AndroidActivity *>(native_pointer);
	if (activity) {
		activity->handleContentInsets(Padding(top, right, bottom, left));
	}
}

static void AppSupportActivity_handleImeInsets(JNIEnv *env, jobject thiz, jlong native_pointer,
		jint top, jint right, jint bottom, jint left) {
	auto activity = reinterpret_cast<AndroidActivity *>(native_pointer);
	if (activity) {
		activity->handleImeInsets(Padding(top, right, bottom, left));
	}
}

static JNINativeMethod s_activityMethods[] = {
	{"handleActivityResult", "(JIILandroid/content/Intent;)V",
		reinterpret_cast<void *>(&AppSupportActivity_handleActivityResult)},
	{"handleContentInsets", "(JIIII)V",
		reinterpret_cast<void *>(&AppSupportActivity_handleContentInsets)},
	{"handleImeInsets", "(JIIII)V", reinterpret_cast<void *>(&AppSupportActivity_handleImeInsets)},
};

static void registerActivityMethods(const jni::RefClass &cl) {
	static std::mutex s_mutex;
	static std::set<std::string> s_classes;

	s_mutex.lock();
	auto className = cl.getName();
	auto classNameStr = className.getString().str<memory::StandartInterface>();

	auto it = s_classes.find(classNameStr);
	if (it == s_classes.end()) {
		cl.registerNatives(s_activityMethods);
		s_classes.emplace(classNameStr);
	}
	s_mutex.unlock();
}

bool AndroidActivity::init(AndroidContextController *controller, ANativeActivity *a,
		BytesView data) {
	_controller = controller;
	_activity = a;
	_proxy = Rc<ActivityProxy>::create(a);

	auto thiz = jni::Ref(a->clazz, a->env);
	_proxy->Activity.setNative(thiz, reinterpret_cast<jlong>(this));

	registerActivityMethods(thiz.getClass());

	return true;
}

bool AndroidActivity::run() {
	memory::pool::context ctx(thread::ThreadInfo::getThreadInfo()->threadPool);

	_activity->instance = this;

	_activity->callbacks->onContentRectChanged = [](ANativeActivity *a, const ARect *r) {
		reinterpret_cast<AndroidActivity *>(a->instance)
				->handleContentRectChanged(Padding(r->top, r->right, r->bottom, r->left));
	};

	_activity->callbacks->onDestroy = [](ANativeActivity *a) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleDestroy();
	};
	_activity->callbacks->onInputQueueCreated = [](ANativeActivity *a, AInputQueue *queue) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleInputQueueCreated(queue);
	};
	_activity->callbacks->onInputQueueDestroyed = [](ANativeActivity *a, AInputQueue *queue) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleInputQueueDestroyed(queue);
	};

	_activity->callbacks->onNativeWindowCreated = [](ANativeActivity *a, ANativeWindow *window) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleNativeWindowCreated(window);
	};

	_activity->callbacks->onNativeWindowDestroyed = [](ANativeActivity *a, ANativeWindow *window) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleNativeWindowDestroyed(window);
	};

	_activity->callbacks->onNativeWindowRedrawNeeded = [](ANativeActivity *a,
															   ANativeWindow *window) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleNativeWindowRedrawNeeded(window);
	};

	_activity->callbacks->onNativeWindowResized = [](ANativeActivity *a, ANativeWindow *window) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleNativeWindowResized(window);
	};
	_activity->callbacks->onPause = [](ANativeActivity *a) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handlePause();
	};
	_activity->callbacks->onResume = [](ANativeActivity *a) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleResume();
	};
	_activity->callbacks->onSaveInstanceState = [](ANativeActivity *a, size_t *outLen) {
		return reinterpret_cast<AndroidActivity *>(a->instance)->handleSaveInstanceState(outLen);
	};
	_activity->callbacks->onStart = [](ANativeActivity *a) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleStart();
	};
	_activity->callbacks->onStop = [](ANativeActivity *a) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleStop();
	};
	_activity->callbacks->onWindowFocusChanged = [](ANativeActivity *a, int focused) {
		reinterpret_cast<AndroidActivity *>(a->instance)->handleWindowFocusChanged(focused);
	};

	return true;
}

void AndroidActivity::notifyWindowInputEvents(Vector<core::InputEventData> &&vec) {
	for (auto &it : vec) {
		if (it.hasLocation()) {
			it.input.y = ANativeWindow_getHeight(_window->getWindow()) - it.input.y;
		}
	}
	_window->handleInputEvents(sp::move(vec));
}

void AndroidActivity::notifyEnableState(WindowState state) {
	_window->updateState(0, _window->getInfo()->state | state);
}

void AndroidActivity::notifyDisableState(WindowState state) {
	_window->updateState(0, _window->getInfo()->state & ~state);
}

ActivityProxy *AndroidActivity::getProxy() const { return _proxy; }

Context *AndroidActivity::getContext() const { return _controller->getContext(); }

void AndroidActivity::handleActivityResult(jint request, jint result, jobject data) {
	log::source().info("AndroidContextController", "handleActivityResult: ", request, " ", result);
}

void AndroidActivity::handleContentInsets(const Padding &p) {
	log::source().info("AndroidContextController", "handleContentInsets: ", p.top, ", ", p.right,
			", ", p.bottom, ", ", p.left);
}

void AndroidActivity::handleImeInsets(const Padding &p) {
	log::source().info("AndroidContextController", "handleImeInsets: ", p.top, ", ", p.right, ", ",
			p.bottom, ", ", p.left);
}

void AndroidActivity::handleDisplayChanged() {
	if (_window) {
		_window->updateWindow();
	}
}

void AndroidActivity::handleContentRectChanged(const Padding &p) {
	getContext()->performTemporary([&] {
		log::format(log::Info, "AndroidContextController", SP_LOCATION,
				"ContentRectChanged: l=%f,t=%f,r=%f,b=%f", p.left, p.top, p.right, p.bottom);

		Padding wp = p;
		wp.bottom = _window->getExtent().height - p.bottom;
		wp.right = _window->getExtent().width - p.right;
		_window->setContentPadding(wp);
	});
}

void AndroidActivity::handleInputQueueCreated(AInputQueue *queue) {
	getContext()->performTemporary([&] {
		log::source().info("AndroidContextController", "onInputQueueCreated");

		_input.emplace(queue, Rc<InputQueue>::create(this, queue));
	});
}

void AndroidActivity::handleInputQueueDestroyed(AInputQueue *queue) {
	getContext()->performTemporary([&] {
		log::source().info("AndroidContextController", "onInputQueueDestroyed");
		_input.erase(queue);
	});
}

void *AndroidActivity::handleSaveInstanceState(size_t *outLen) {
	void *ret = nullptr;
	getContext()->performTemporary([&] {
		log::source().info("AndroidContextController", "onSaveInstanceState ", this);

		auto stateData = _controller->saveContextState();
		if (stateData) {
			auto newData = data::write(stateData, EncodeFormat::CborCompressed);

			*outLen = newData.size();
			ret = ::malloc(newData.size());
			::memcpy(ret, newData.data(), newData.size());
		}
	});

	return ret;
}

void AndroidActivity::handleNativeWindowCreated(ANativeWindow *window) {
	getContext()->performTemporary([&] {
		log::format(log::Info, "AndroidContextController", SP_LOCATION,
				"NativeWindowCreated: %p -- %p -- %d x %d", _activity, window,
				ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));

		Rc<AndroidWindow> win;
		auto wInfo = _controller->makeWindowInfo(window);

		if (_controller->configureWindow(wInfo)) {
			win = Rc<AndroidWindow>::create(this, move(wInfo), window);
			if (win) {
				_window = sp::move(win);
				_controller->notifyWindowCreated(_window);
				return true;
			}
		}

		return false;
	});
}

void AndroidActivity::handleNativeWindowDestroyed(ANativeWindow *window) {
	getContext()->performTemporary([&] {
		log::format(log::Info, "AndroidContextController", SP_LOCATION,
				"NativeWindowDestroyed: %p -- %p", _activity, window);

		if (_window->getWindow() == window) {
			_window->close();
			_controller->notifyWindowClosed(_window,
					WindowCloseOptions::CloseInPlace | WindowCloseOptions::IgnoreExitGuard);
			_window = nullptr;
		}
	});
}

void AndroidActivity::handleNativeWindowRedrawNeeded(ANativeWindow *window) {
	getContext()->performTemporary([&] {
		log::format(log::Info, "AndroidContextController", SP_LOCATION,
				"NativeWindowRedrawNeeded: %p -- %p", _activity, window);
		if (_window->getWindow() == window) {
			_window->getAppWindow()->waitUntilFrame();
		}
	});
}

void AndroidActivity::handleNativeWindowResized(ANativeWindow *window) {
	getContext()->performTemporary([&] {
		log::format(log::Info, "AndroidContextController", SP_LOCATION,
				"NativeWindowResized: %p -- %p -- %d x %d", _activity, window,
				ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));

		if (_window->getWindow() == window) {
			_window->updateWindow();
		}
	});
}

void AndroidActivity::handlePause() {
	getContext()->performTemporary([&] {
		log::source().info("AndroidContextController", "onPause");
		/*core::InputEventData event =
				core::InputEventData::BoolEvent(core::InputEventName::Background, true);
		transferInputEvent(event);*/

		//pause();
	});
}

void AndroidActivity::handleStart() {
	getContext()->performTemporary(
			[&] { log::source().info("AndroidContextController", "onStart"); });
}

void AndroidActivity::handleResume() {
	getContext()->performTemporary([&] {
		log::source().info("AndroidContextController", "onResume");
		/*core::InputEventData event =
				core::InputEventData::BoolEvent(core::InputEventName::Background, false);
		transferInputEvent(event);*/

		//resume();
	});
}

void AndroidActivity::handleStop() {
	getContext()->performTemporary([&] {
		stappler::log::source().info("AndroidContextController", "onStop");

		/*if (_running) {
			for (auto &it : _components) { it->handleStop(this); }

			removeRemoteNotificationCallback(this);
			removeTokenCallback(this);

			_application->stop();

			_running = false;
		}*/
		//stop();
	});
}

void AndroidActivity::handleDestroy() {
	auto id = retain();
	getContext()->performTemporary([&] {
		log::format(log::Info, "AndroidContextController", SP_LOCATION, "Destroy: %p", _activity);

		_controller->destroyActivity(this);
	});
	release(id);
}

void AndroidActivity::handleWindowFocusChanged(int focused) {
	getContext()->performTemporary([&] {
		stappler::log::source().info("AndroidContextController", "onWindowFocusChanged: ", focused);
		/*core::InputEventData event =
			core::InputEventData::BoolEvent(core::InputEventName::FocusGain, focused != 0);
	transferInputEvent(event);*/
	});
}

} // namespace stappler::xenolith::platform
