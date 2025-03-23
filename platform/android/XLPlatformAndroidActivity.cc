/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "XLPlatformAndroidActivity.h"
#include "SPPlatformUnistd.h"
#include "SPDso.h"

#include "XLPlatformAndroidKeyCodes.cc"

#include <sys/timerfd.h>
#include <sys/eventfd.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static thread_local AppEnv tl_interface;
static JavaVM *s_vm = nullptr;

static JNIEnv *getEnv() {
	void *ret = nullptr;

	s_vm->GetEnv(&ret, JNI_VERSION_1_6);
	return reinterpret_cast<JNIEnv*>(ret);
}

static jni::Local Activity_getDisplay(ANativeActivity *activity, int sdk) {
	jni::Env env(activity->env);
	jni::Ref thiz(activity->clazz, activity->env);

	auto cl = thiz.getClass();

	if (sdk >= 30) {
		auto getDisplayMethod = cl.getMethodID("getDisplay", "()Landroid/view/Display;");
		auto display = thiz.callMethod<jobject>(getDisplayMethod);
		if (display) {
			return display;
		}
	}

	auto getServiceMethod = cl.getMethodID("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
	auto windowManagerFieldID = cl.getStaticFieldID("WINDOW_SERVICE", "Ljava/lang/String;");
	auto windowManagerField = cl.getStaticField<jstring>(windowManagerFieldID);
	jni::Local windowManager = thiz.callMethod<jobject>(getServiceMethod, windowManagerField);
	if (windowManager) {
		jni::LocalClass windowManagerClass = windowManager.getClass();
		auto getDefaultDisplayMethod = windowManagerClass.getMethodID("getDefaultDisplay", "()Landroid/view/Display;");
		auto display = windowManager.callMethod<jobject>(getDefaultDisplayMethod);
		if (display) {
			return display;
		}
	}
	return nullptr;
}

AppEnv *AppEnv::getInerface() {
	if (!tl_interface.env) {
		auto env = getEnv();
		if (env) {
			tl_interface.init(s_vm, env, false);
		} else {
			s_vm->AttachCurrentThread(&env, nullptr);
			if (env) {
				tl_interface.init(s_vm, env, true);
			}
		}
	}
	return &tl_interface;
};

AppEnv::~AppEnv() {
	if (attached) {
		vm->DetachCurrentThread();
	}
}

void AppEnv::init(JavaVM *v, JNIEnv *e, bool a) {
	vm = v;
	env = e;
	attached = a;
}

static core::ImageFormat s_getCommonFormat = core::ImageFormat::R8G8B8A8_UNORM;

core::ImageFormat getCommonFormat() {
	return s_getCommonFormat;
}

Activity::Activity() { }

Activity::~Activity() {
	if (_clipboardServce) {
		_activity->env->DeleteGlobalRef(_clipboardServce);
		_clipboardServce = nullptr;
	}

	if (_rootView) {
		_rootView = nullptr;
	}

	if (_networkConnectivity) {
		_networkConnectivity->finalize();
		_networkConnectivity = nullptr;
	}

	if (_classLoader) {
		_classLoader->finalize();
		_classLoader = nullptr;
	}

	stappler::jni::Env::finalizeJava();
	filesystem::platform::Android_terminateFilesystem();

	if (_config) {
		AConfiguration_delete(_config);
		_config = nullptr;
	}
}

bool Activity::init(ANativeActivity *activity, ActivityFlags flags) {
	s_vm = activity->vm;
	_flags = flags;
	_activity = activity;
	_config = AConfiguration_new();
	AConfiguration_fromAssetManager(_config, _activity->assetManager);
	_sdkVersion = AConfiguration_getSdkVersion(_config);

	stappler::jni::Env::loadJava(activity->vm, _sdkVersion);

	if (_sdkVersion >= 29) {
		// check for available surface formats
		auto handle = Dso(StringView(), DsoFlags::Self);
		if (handle) {
			auto _AHardwareBuffer_isSupported = handle.sym<int (*) (const AHardwareBuffer_Desc *)>("AHardwareBuffer_isSupported");
			if (_AHardwareBuffer_isSupported) {

				// check for common buffer formats
				auto checkSupported = [&] (int format) -> bool {
					AHardwareBuffer_Desc desc;
					desc.width = 1024;
					desc.height = 1024;
					desc.layers = 1;
					desc.format = format;
					desc.usage = AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
					desc.stride = 0;
					desc.rfu0 = 0;
					desc.rfu1 = 0;

					return _AHardwareBuffer_isSupported(&desc) != 0;
				};

				_formatSupport = NativeBufferFormatSupport{
					checkSupported(AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM),
					checkSupported(AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM),
					checkSupported(AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM),
					checkSupported(AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM),
					checkSupported(AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT),
					checkSupported(AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM)
				};
			}
		}
	}

	_activity->callbacks->onConfigurationChanged = [] (ANativeActivity* a) {
		reinterpret_cast<Activity *>(a->instance)->handleConfigurationChanged();
	};

	_activity->callbacks->onContentRectChanged = [] (ANativeActivity* a, const ARect* r) {
		reinterpret_cast<Activity *>(a->instance)->handleContentRectChanged(r);
	};

	_activity->callbacks->onDestroy = [] (ANativeActivity* a) {
		auto ref = reinterpret_cast<Activity *>(a->instance);
		ref->handleDestroy();
	};
	_activity->callbacks->onInputQueueCreated = [] (ANativeActivity* a, AInputQueue* queue) {
		reinterpret_cast<Activity *>(a->instance)->handleInputQueueCreated(queue);
	};
	_activity->callbacks->onInputQueueDestroyed = [] (ANativeActivity* a, AInputQueue* queue) {
		reinterpret_cast<Activity *>(a->instance)->handleInputQueueDestroyed(queue);
	};
	_activity->callbacks->onLowMemory = [] (ANativeActivity* a) {
		reinterpret_cast<Activity *>(a->instance)->handleLowMemory();
	};

	_activity->callbacks->onNativeWindowCreated = [] (ANativeActivity* a, ANativeWindow* window) {
		reinterpret_cast<Activity *>(a->instance)->handleNativeWindowCreated(window);
	};

	_activity->callbacks->onNativeWindowDestroyed = [] (ANativeActivity* a, ANativeWindow* window) {
		reinterpret_cast<Activity *>(a->instance)->handleNativeWindowDestroyed(window);
	};

	_activity->callbacks->onNativeWindowRedrawNeeded = [] (ANativeActivity* a, ANativeWindow* window) {
		reinterpret_cast<Activity *>(a->instance)->handleNativeWindowRedrawNeeded(window);
	};

	_activity->callbacks->onNativeWindowResized = [] (ANativeActivity* a, ANativeWindow* window) {
		reinterpret_cast<Activity *>(a->instance)->handleNativeWindowResized(window);
	};
	_activity->callbacks->onPause = [] (ANativeActivity* a) {
		reinterpret_cast<Activity *>(a->instance)->handlePause();
	};
	_activity->callbacks->onResume = [] (ANativeActivity* a) {
		reinterpret_cast<Activity *>(a->instance)->handleResume();
	};
	_activity->callbacks->onSaveInstanceState = [] (ANativeActivity* a, size_t* outLen) {
		return reinterpret_cast<Activity *>(a->instance)->handleSaveInstanceState(outLen);
	};
	_activity->callbacks->onStart = [] (ANativeActivity* a) {
		reinterpret_cast<Activity *>(a->instance)->handleStart();
	};
	_activity->callbacks->onStop = [] (ANativeActivity* a) {
		reinterpret_cast<Activity *>(a->instance)->handleStop();
	};
	_activity->callbacks->onWindowFocusChanged = [] (ANativeActivity *a, int focused) {
		reinterpret_cast<Activity *>(a->instance)->handleWindowFocusChanged(focused);
	};

	auto env = jni::Env(_activity->env);
	jni::Ref thiz(_activity->clazz, _activity->env);

	auto activityClass = thiz.getClass();
	auto setNativePointerMethod = activityClass.getMethodID("setNativePointer", "(J)V");
	if (setNativePointerMethod) {
		thiz.callMethod<void>(setNativePointerMethod, jlong(this));
	}

	env.checkErrors();

	do {
		auto isEmulatorMethod = activityClass.getMethodID("isEmulator", "()Z");
		if (isEmulatorMethod) {
			isEmulator = thiz.callMethod<jboolean>(isEmulatorMethod);
			if (isEmulator) {
				// emulators often do not support this format for swapchains
				_formatSupport.R8G8B8A8_UNORM = false;
				if (_formatSupport.R5G6B5_UNORM) {
					s_getCommonFormat = core::ImageFormat::R5G6B5_UNORM_PACK16;
				} else if (_formatSupport.R8G8B8_UNORM) {
					s_getCommonFormat = core::ImageFormat::R8G8B8_UNORM;
				}
			}
		}

		_startActivityMethod = activityClass.getMethodID("startActivity", "(Landroid/content/Intent;)V");
		_runInputMethod = activityClass.getMethodID("runInput", "(Ljava/lang/String;III)V");
		_updateInputMethod = activityClass.getMethodID("updateInput", "(Ljava/lang/String;III)V");
		_updateCursorMethod = activityClass.getMethodID("updateCursor", "(II)V");
		_cancelInputMethod = activityClass.getMethodID("cancelInput", "()V");

		auto intentClass = env.findClass("android/content/Intent");
		auto uriClass = env.findClass("android/net/Uri");
		auto contextClass = env.findClass("android/content/Context");

		_intentInitMethod = intentClass.getMethodID("<init>", "(Ljava/lang/String;Landroid/net/Uri;)V");
		_intentAddFlagsMethod = intentClass.getMethodID("addFlags", "(I)Landroid/content/Intent;");
		_intentActionView = intentClass.getStaticFieldID("ACTION_VIEW", "Ljava/lang/String;");
		_uriParseMethod = uriClass.getStaticMethodID("parse", "(Ljava/lang/String;)Landroid/net/Uri;");

		auto getServiceMethod = contextClass.getMethodID("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
		auto clipboardNameField = contextClass.getStaticField<jstring>("CLIPBOARD_SERVICE");

		_clipboardServce = thiz.callMethod<jobject>(getServiceMethod, clipboardNameField);
	} while (0);

	env.checkErrors();

	_classLoader = Rc<ClassLoader>::create(_activity, _sdkVersion);

	filesystem::platform::Android_initializeFilesystem(_activity->assetManager,
			_activity->internalDataPath, _activity->externalDataPath, _classLoader ? _classLoader->apkPath : StringView());

	_info = ActivityInfo::get(_config, env, activityClass, thiz);
	_activity->instance = this;

	if (_classLoader) {
		_networkConnectivity = Rc<NetworkConnectivity>::create(_classLoader, thiz, [this] (NetworkCapabilities flags) {
			setNetworkCapabilities(flags);
		});

		if (_networkConnectivity) {
			setNetworkCapabilities(_networkConnectivity->capabilities);
		}

		auto drawableClassName = toString(_info.bundleName, ".R$drawable");
		auto cl = _classLoader->findClass(env, drawableClassName);
		if (cl) {
			_classLoader->foreachField(cl, [&] (StringView type, StringView name, const jni::Ref &obj) {
				if (type == "int") {
					_drawables.setInteger(_classLoader->getIntField(cl, obj), name);
				}
			});
		}
	}

	Value info;
	info.setValue(_drawables, "drawables");
	info.setString(_info.bundleName, "bundleName");
	info.setString(_info.applicationName, "applicationName");
	info.setString(_info.applicationVersion, "applicationVersion");
	info.setString(_info.userAgent, "userAgent");
	info.setString(_info.systemAgent, "systemAgent");
	info.setString(_info.locale, "locale");
	info.setDouble(_info.density, "density");
	info.setValue(Value({Value(_info.sizeInPixels.width), Value(_info.sizeInPixels.height)}), "size");
	info.setInteger(_sdkVersion, "sdk");

	saveApplicationInfo(info);
	_messageToken = loadMessageToken();

	env.checkErrors();

	_refId = retain();
	return true;
}

bool Activity::runApplication(PlatformApplication *app) {
	if (_application) {
		return false;
	}

	_application = app;

	auto tok = getMessageToken();
	if (!tok.empty()) {
		_application->updateMessageToken(BytesView(StringView(tok)));
	}

	addTokenCallback(this, [this] (StringView str) {
		_application->performOnAppThread([this, str = str.str<Interface>()] () mutable {
			_application->updateMessageToken(BytesView(StringView(str)));
		}, this);
	});

	addRemoteNotificationCallback(this, [this] (const Value &val) {
		_application->performOnAppThread([this, val = val] () mutable {
			_application->receiveRemoteNotification(move(val));
		}, this);
	});

	_application->run();

	return true;
}

NetworkCapabilities Activity::getNetworkCapabilities() const {
	std::unique_lock lock(_callbackMutex);
	return _capabilities;
}

void Activity::setNetworkCapabilities(NetworkCapabilities cap) {
	std::unique_lock lock(_callbackMutex);
	_capabilities = cap;
	for (auto &it : _networkCallbacks) {
		it.second.second(_capabilities);
	}
}

void Activity::addNetworkCallback(void *key, Function<void(NetworkCapabilities)> &&cb) {
	std::unique_lock lock(_callbackMutex);
	_networkCallbacks.emplace(key, pair(Rc<Ref>(this), sp::move(cb)));
}

void Activity::removeNetworkCallback(void *key) {
	std::unique_lock lock(_callbackMutex);
	_networkCallbacks.erase(key);
}

void Activity::addRemoteNotificationCallback(void *key, Function<void(const Value &)> &&cb) {
	std::unique_lock lock(_callbackMutex);
	_notificationCallbacks.emplace(key, pair(Rc<Ref>(this), sp::move(cb)));
}

void Activity::removeRemoteNotificationCallback(void *key) {
	std::unique_lock lock(_callbackMutex);
	_notificationCallbacks.erase(key);
}

void Activity::addTokenCallback(void *key, Function<void(StringView)> &&cb) {
	std::unique_lock lock(_callbackMutex);
	_tokenCallbacks.emplace(key, pair(Rc<Ref>(this), sp::move(cb)));
}

void Activity::removeTokenCallback(void *key) {
	std::unique_lock lock(_callbackMutex);
	_tokenCallbacks.erase(key);
}

void Activity::setView(platform::ViewInterface *view) {
	_rootView = view;
}

String Activity::getMessageToken() const {
	std::unique_lock lock(_callbackMutex);
	return _messageToken;
}

void Activity::setMessageToken(StringView str) {
	std::unique_lock lock(_callbackMutex);
	if (_messageToken != str) {
		_messageToken = str.str<Interface>();
		for (auto &it : _tokenCallbacks) {
			it.second.second(_messageToken);
		}
	}
}

void Activity::handleRemoteNotification(const Value &val) {
	std::unique_lock lock(_callbackMutex);
	for (auto &it : _notificationCallbacks) {
		it.second.second(val);
	}
}

ApplicationInfo Activity::makeApplicationInfo() const {
	ApplicationInfo ret;
	ret.bundleName = _info.bundleName;
	ret.applicationName = _info.applicationName;
	ret.applicationVersion = _info.applicationVersion;
	ret.userLanguage = _info.locale;
	ret.userAgent = _info.systemAgent;
	ret.density = _info.density;
	ret.screenSize = _info.sizeInDp;
	ret.platformHandle = const_cast<Activity *>(this);
	return ret;
}

void Activity::addComponent(Rc<ActivityComponent> &&c) {
	if (c) {
		auto &it = _components.emplace_back(move(c));
		it->handleAdded(this);
	}
}

void Activity::handleConfigurationChanged() {
	log::info("NativeActivity", "onConfigurationChanged");
	if (_config) {
		AConfiguration_delete(_config);
	}
	_config = AConfiguration_new();
	AConfiguration_fromAssetManager(_config, _activity->assetManager);
	_sdkVersion = AConfiguration_getSdkVersion(_config);

	_info = ActivityInfo::get(_config, _activity->env, getClass(),
			jni::Ref(_activity->clazz, _activity->env), &_info);

	for (auto &it : _components) {
		it->handleConfigurationChanged(this, _config);
	}
}

void Activity::handleContentRectChanged(const ARect *r) {
	log::format(log::Info, "NativeActivity", "ContentRectChanged: l=%d,t=%d,r=%d,b=%d", r->left, r->top, r->right, r->bottom);

	if (_rootView) {
		auto verticalSpace = _windowSize.height - r->top;
		_rootView->setContentPadding(Padding(r->top, _windowSize.width - r->right, _windowSize.height - r->bottom, r->left));
	}
}

void Activity::handleInputQueueCreated(AInputQueue *queue) {
	log::info("NativeActivity", "onInputQueueCreated");
	if ((_flags & ActivityFlags::CaptureInput) != ActivityFlags::None) {
		auto it = _input.emplace(queue, InputLooperData { this, queue }).first;
		AInputQueue_attachLooper(queue, ALooper_forThread(), 0, [](int fd, int events, void *data) {
			auto d = reinterpret_cast<InputLooperData*>(data);
			return d->activity->handleInputEventQueue(fd, events, d->queue);
		}, &it->second);
	}
}

void Activity::handleInputQueueDestroyed(AInputQueue *queue) {
	log::info("NativeActivity", "onInputQueueDestroyed");
	if ((_flags & ActivityFlags::CaptureInput) != ActivityFlags::None) {
		AInputQueue_detachLooper(queue);
		_input.erase(queue);
	}
}

void Activity::handleLowMemory() {
	log::info("NativeActivity", "onLowMemory");

	for (auto &it : _components) {
		it->handleLowMemory(this);
	}
}

void *Activity::handleSaveInstanceState(size_t* outLen) {
	log::info("NativeActivity", "onSaveInstanceState");
	return nullptr;
}

void Activity::handleNativeWindowCreated(ANativeWindow *window) {
    log::format(log::Info, "NativeActivity", "NativeWindowCreated: %p -- %p -- %d x %d", _activity, window,
			ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));

	_application->waitRunning();

	if (_rootView) {
		_rootView->linkWithNativeWindow(window);
	} else {
		abort();
	}

	_windowSize = Size2(ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));
	_recreateSwapchain = true;
}

void Activity::handleNativeWindowDestroyed(ANativeWindow *window) {
	log::format(log::Info, "NativeActivity", "NativeWindowDestroyed: %p -- %p", _activity, window);
	if (_rootView) {
		_rootView->end();
		_recreateSwapchain = true;
	}
}

void Activity::handleNativeWindowRedrawNeeded(ANativeWindow *window) {
	log::format(log::Info, "NativeActivity", "NativeWindowRedrawNeeded: %p -- %p", _activity, window);
	if (_rootView) {
		_rootView->waitUntilFrame();
	}
}

void Activity::handleNativeWindowResized(ANativeWindow *window) {
	log::format(log::Info, "NativeActivity", "NativeWindowResized: %p -- %p -- %d x %d", _activity, window,
			ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));

	auto newSize = Size2(ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));
	if (_rootView && (_windowSize != newSize || _recreateSwapchain)) {
		_recreateSwapchain = false;
		_windowSize = newSize;
		_rootView->getPresentationEngine()->deprecateSwapchain(false);
	}
}

void Activity::handlePause() {
	log::info("NativeActivity", "onPause");
	core::InputEventData event = core::InputEventData::BoolEvent(core::InputEventName::Background, true);
	transferInputEvent(event);

	for (auto &it : _components) {
		it->handlePause(this);
	}
}

void Activity::handleStart() {
	log::info("NativeActivity", "onStart");

	for (auto &it : _components) {
		it->handleStart(this);
	}
}

void Activity::handleResume() {
	log::info("NativeActivity", "onResume");
	core::InputEventData event = core::InputEventData::BoolEvent(core::InputEventName::Background, false);
	transferInputEvent(event);

	for (auto &it : _components) {
		it->handleResume(this);
	}
}

void Activity::handleStop() {
	stappler::log::info("NativeActivity", "onStop");
}

void Activity::handleDestroy() {
	log::format(log::Info, "NativeActivity", "Destroy: %p", _activity);

	for (auto &it : _components) {
		it->handleDestroy(this);
	}

	if (_rootView) {
		_rootView->end();
	}
	if (_application) {
		_application->end();
		_application->waitStopped();
	}
	release(_refId);
}
void Activity::handleWindowFocusChanged(int focused) {
	stappler::log::info("NativeActivity", "onWindowFocusChanged");
	core::InputEventData event = core::InputEventData::BoolEvent(core::InputEventName::FocusGain, focused != 0);
	transferInputEvent(event);
}

int Activity::handleInputEventQueue(int fd, int events, AInputQueue *queue) {
    AInputEvent* event = NULL;
    while (AInputQueue_getEvent(queue, &event) >= 0) {
        if (AInputQueue_preDispatchEvent(queue, event)) {
            continue;
        }
        int32_t handled = handleInputEvent(event);
        AInputQueue_finishEvent(queue, event, handled);
    }
    return 1;
}

int Activity::handleInputEvent(AInputEvent *event) {
	auto type = AInputEvent_getType(event);
	auto source = AInputEvent_getSource(event);
	switch (type) {
	case AINPUT_EVENT_TYPE_KEY:
		return handleKeyEvent(event);
		break;
	case AINPUT_EVENT_TYPE_MOTION:
		return handleMotionEvent(event);
		break;
	}
	return 0;
}

int Activity::handleKeyEvent(AInputEvent *event) {
	auto action = AKeyEvent_getAction(event);
	auto flags = AKeyEvent_getFlags(event);
	auto modsFlags = AKeyEvent_getMetaState(event);

	auto keyCode = AKeyEvent_getKeyCode(event);
	auto scanCode = AKeyEvent_getKeyCode(event);

	if (keyCode == AKEYCODE_BACK) {
		if (handleBackButton()) {
			return 0;
		}
	}

	core::InputModifier mods = core::InputModifier::None;
	if (modsFlags != AMETA_NONE) {
		if ((modsFlags & AMETA_ALT_ON) != AMETA_NONE) { mods |= core::InputModifier::Alt; }
		if ((modsFlags & AMETA_ALT_LEFT_ON) != AMETA_NONE) { mods |= core::InputModifier::AltL; }
		if ((modsFlags & AMETA_ALT_RIGHT_ON) != AMETA_NONE) { mods |= core::InputModifier::AltR; }
		if ((modsFlags & AMETA_SHIFT_ON) != AMETA_NONE) { mods |= core::InputModifier::Shift; }
		if ((modsFlags & AMETA_SHIFT_LEFT_ON) != AMETA_NONE) { mods |= core::InputModifier::ShiftL; }
		if ((modsFlags & AMETA_SHIFT_RIGHT_ON) != AMETA_NONE) { mods |= core::InputModifier::ShiftR; }
		if ((modsFlags & AMETA_CTRL_ON) != AMETA_NONE) { mods |= core::InputModifier::Ctrl; }
		if ((modsFlags & AMETA_CTRL_LEFT_ON) != AMETA_NONE) { mods |= core::InputModifier::CtrlL; }
		if ((modsFlags & AMETA_CTRL_RIGHT_ON) != AMETA_NONE) { mods |= core::InputModifier::CtrlR; }
		if ((modsFlags & AMETA_META_ON) != AMETA_NONE) { mods |= core::InputModifier::Mod3; }
		if ((modsFlags & AMETA_META_LEFT_ON) != AMETA_NONE) { mods |= core::InputModifier::Mod3L; }
		if ((modsFlags & AMETA_META_RIGHT_ON) != AMETA_NONE) { mods |= core::InputModifier::Mod3R; }

		if ((modsFlags & AMETA_CAPS_LOCK_ON) != AMETA_NONE) { mods |= core::InputModifier::CapsLock; }
		if ((modsFlags & AMETA_NUM_LOCK_ON) != AMETA_NONE) { mods |= core::InputModifier::NumLock; }

		if ((modsFlags & AMETA_SCROLL_LOCK_ON) != AMETA_NONE) { mods |= core::InputModifier::ScrollLock; }
		if ((modsFlags & AMETA_SYM_ON) != AMETA_NONE) { mods |= core::InputModifier::Sym; }
		if ((modsFlags & AMETA_FUNCTION_ON) != AMETA_NONE) { mods |= core::InputModifier::Function; }
	}

	_activeModifiers = mods;

	Vector<core::InputEventData> events;

	bool isCanceled = (flags & AKEY_EVENT_FLAG_CANCELED) != 0 | (flags & AKEY_EVENT_FLAG_CANCELED_LONG_PRESS) != 0;

	switch (action) {
	case AKEY_EVENT_ACTION_DOWN: {
		auto &ev = events.emplace_back(core::InputEventData{uint32_t(keyCode),
			core::InputEventName::KeyPressed, core::InputMouseButton::Touch, _activeModifiers,
			_hoverLocation.x, _hoverLocation.y});
		ev.key.keycode = s_keycodes[keyCode];
		ev.key.compose = core::InputKeyComposeState::Nothing;
		ev.key.keysym = keyCode;
		ev.key.keychar = 0;
		break;
	}
	case AKEY_EVENT_ACTION_UP: {
		auto &ev = events.emplace_back(core::InputEventData{uint32_t(keyCode),
			isCanceled ? core::InputEventName::KeyCanceled : core::InputEventName::KeyReleased, core::InputMouseButton::Touch, _activeModifiers,
			_hoverLocation.x, _hoverLocation.y});
		ev.key.keycode = s_keycodes[keyCode];
		ev.key.compose = core::InputKeyComposeState::Nothing;
		ev.key.keysym = keyCode;
		ev.key.keychar = 0;
		break;
	}
	case AKEY_EVENT_ACTION_MULTIPLE: {
		auto &ev = events.emplace_back(core::InputEventData{uint32_t(keyCode),
			core::InputEventName::KeyRepeated, core::InputMouseButton::Touch, _activeModifiers,
			_hoverLocation.x, _hoverLocation.y});
		ev.key.keycode = s_keycodes[keyCode];
		ev.key.compose = core::InputKeyComposeState::Nothing;
		ev.key.keysym = keyCode;
		ev.key.keychar = 0;
		break;
	}
	}
	if (!events.empty()) {
		transferInputEvents(sp::move(events));
		return 1;
	}
	return 0;
}

int Activity::handleMotionEvent(AInputEvent *event) {
	Vector<core::InputEventData> events;
	auto action = AMotionEvent_getAction(event);
	auto count = AMotionEvent_getPointerCount(event);
	switch (action & AMOTION_EVENT_ACTION_MASK) {
	case AMOTION_EVENT_ACTION_DOWN:
		stappler::log::info("NativeActivity", "Motion AMOTION_EVENT_ACTION_DOWN ", count,
							 " ", AMotionEvent_getPointerId(event, 0), " ", 0);
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(core::InputEventData{uint32_t(AMotionEvent_getPointerId(event, i)),
				core::InputEventName::Begin, core::InputMouseButton::Touch, _activeModifiers,
				AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i)});
			ev.point.density = _density;
		}
		break;
	case AMOTION_EVENT_ACTION_UP:
		stappler::log::info("NativeActivity", "Motion AMOTION_EVENT_ACTION_UP ", count,
							 " ", AMotionEvent_getPointerId(event, 0), " ", 0);
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(core::InputEventData{uint32_t(AMotionEvent_getPointerId(event, i)),
				core::InputEventName::End, core::InputMouseButton::Touch, _activeModifiers,
				AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i)});
			ev.point.density = _density;
		}
		break;
	case AMOTION_EVENT_ACTION_MOVE:
		for (size_t i = 0; i < count; ++ i) {
			if (AMotionEvent_getHistorySize(event) == 0
					|| AMotionEvent_getX(event, i) - AMotionEvent_getHistoricalX(event, i, AMotionEvent_getHistorySize(event) - 1) != 0.0f
					|| AMotionEvent_getY(event, i) - AMotionEvent_getHistoricalY(event, i, AMotionEvent_getHistorySize(event) - 1) != 0.0f) {
				auto &ev = events.emplace_back(core::InputEventData{uint32_t(AMotionEvent_getPointerId(event, i)),
					core::InputEventName::Move, core::InputMouseButton::Touch, _activeModifiers,
					AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i)});
				ev.point.density = _density;
			}
		}
		break;
	case AMOTION_EVENT_ACTION_CANCEL:
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(core::InputEventData{uint32_t(AMotionEvent_getPointerId(event, i)),
				core::InputEventName::Cancel, core::InputMouseButton::Touch, _activeModifiers,
				AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i)});
			ev.point.density = _density;
		}
		break;
	case AMOTION_EVENT_ACTION_OUTSIDE:
		stappler::log::info("NativeActivity", "Motion AMOTION_EVENT_ACTION_OUTSIDE ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_POINTER_DOWN: {
		auto pointer = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> 8;
		stappler::log::info("NativeActivity", "Motion AMOTION_EVENT_ACTION_POINTER_DOWN ", count,
							 " ", AMotionEvent_getPointerId(event, pointer), " ", pointer);
		auto &ev = events.emplace_back(core::InputEventData{uint32_t(AMotionEvent_getPointerId(event, pointer)),
			core::InputEventName::Begin, core::InputMouseButton::Touch, _activeModifiers,
			AMotionEvent_getX(event, pointer), _windowSize.height - AMotionEvent_getY(event, pointer)});
		ev.point.density = _density;
		break;
	}
	case AMOTION_EVENT_ACTION_POINTER_UP: {
		auto pointer = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> 8;
		stappler::log::info("NativeActivity", "Motion AMOTION_EVENT_ACTION_POINTER_UP ", count,
							 " ", AMotionEvent_getPointerId(event, pointer), " ", pointer);
		auto &ev = events.emplace_back(core::InputEventData{uint32_t(AMotionEvent_getPointerId(event, pointer)),
			core::InputEventName::End, core::InputMouseButton::Touch, _activeModifiers,
			AMotionEvent_getX(event, pointer), _windowSize.height - AMotionEvent_getY(event, pointer)});
		ev.point.density = _density;
		break;
	}
	case AMOTION_EVENT_ACTION_HOVER_MOVE:
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(core::InputEventData{uint32_t(AMotionEvent_getPointerId(event, i)),
				core::InputEventName::MouseMove, core::InputMouseButton::Touch, _activeModifiers,
				AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i)});
			ev.point.density = _density;
			_hoverLocation = Vec2(AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i));
		}
		break;
	case AMOTION_EVENT_ACTION_SCROLL:
		stappler::log::info("NativeActivity", "Motion AMOTION_EVENT_ACTION_SCROLL ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_HOVER_ENTER:
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::PointerEnter, true,
					Vec2(AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i))));
			ev.id = uint32_t(AMotionEvent_getPointerId(event, i));
			ev.point.density = _density;
		}
		stappler::log::info("NativeActivity", "Motion AMOTION_EVENT_ACTION_HOVER_ENTER ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_HOVER_EXIT:
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::PointerEnter, false,
					Vec2(AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i))));
			ev.id = uint32_t(AMotionEvent_getPointerId(event, i));
			ev.point.density = _density;
		}
		stappler::log::info("NativeActivity", "Motion AMOTION_EVENT_ACTION_HOVER_EXIT ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_BUTTON_PRESS:
		stappler::log::info("NativeActivity", "Motion AMOTION_EVENT_ACTION_BUTTON_PRESS ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_BUTTON_RELEASE:
		stappler::log::info("NativeActivity", "Motion AMOTION_EVENT_ACTION_BUTTON_RELEASE ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	}
	if (!events.empty()) {
		transferInputEvents(sp::move(events));
		return 1;
	}
	return 0;
}

void Activity::handleActivityResult(jint request_code, jint result_code, jobject data) {
	for (auto &it : _components) {
		if (it->handleActivityResult(this, request_code, result_code, data)) {
			return;
		}
	}
}

void Activity::handleCancelInput() {
	if (_textInputWrapper) {
		_textInputWrapper->cancelInput(_textInputWrapper->target);
		_textInputWrapper = nullptr;
	}
}

void Activity::handleTextChanged(jstring text, jint cursor_start, jint cursor_len) {
	if (_textInputWrapper) {
		jboolean isCopy = 0;
		auto jchars = _activity->env->GetStringChars(text, &isCopy);
		auto len = _activity->env->GetStringLength(text);

		_textInputWrapper->textChanged(_textInputWrapper->target,
				WideStringView(reinterpret_cast<const char16_t *>(jchars), len), core::TextCursor(cursor_start, cursor_len));

		_activity->env->ReleaseStringChars(text, jchars);
	}
}

void Activity::handleInputEnabled(jboolean value) {
	if (_textInputWrapper) {
		_textInputWrapper->inputEnabled(_textInputWrapper->target, (value == 0) ? false : true);
	}
}

void Activity::openUrl(StringView url) {
	auto env = AppEnv::getInerface();
	auto intentClass = env->env->FindClass("android/content/Intent");
	auto uriClass = env->env->FindClass("android/net/Uri");

	auto j_str = env->env->NewStringUTF(url.data());
	auto j_uri = env->env->CallStaticObjectMethod(uriClass, _uriParseMethod, j_str);
	auto j_ACTION_VIEW = env->env->GetStaticObjectField(intentClass, _intentActionView);

	auto j_intent = env->env->NewObject(intentClass, _intentInitMethod, j_ACTION_VIEW, j_uri);
	env->env->CallObjectMethod(j_intent, _intentAddFlagsMethod, FLAG_ACTIVITY_NEW_TASK);
	env->env->CallVoidMethod(_activity->clazz, _startActivityMethod, j_intent);

	env->env->DeleteLocalRef(j_intent);
	env->env->DeleteLocalRef(j_ACTION_VIEW);
	env->env->DeleteLocalRef(j_str);
	env->env->DeleteLocalRef(uriClass);
	env->env->DeleteLocalRef(intentClass);
}

void Activity::updateTextCursor(uint32_t pos, uint32_t len) {
	_activity->env->CallVoidMethod(_activity->clazz, _updateCursorMethod, jint(pos), jint(len));
}

void Activity::updateTextInput(WideStringView str, uint32_t pos, uint32_t len, core::TextInputType type) {
	auto jstr = _activity->env->NewString(reinterpret_cast<const jchar*>(str.data()), str.size());
	_activity->env->CallVoidMethod(_activity->clazz, _updateInputMethod, jstr, jint(pos), jint(len), jint(type));
	_activity->env->DeleteLocalRef(jstr);
}

void Activity::runTextInput(Rc<ActivityTextInputWrapper> &&wrapper, WideStringView str, uint32_t pos, uint32_t len, core::TextInputType type) {
	_textInputWrapper = move(wrapper);

	auto jstr = _activity->env->NewString(reinterpret_cast<const jchar*>(str.data()), str.size());
	_activity->env->CallVoidMethod(_activity->clazz, _runInputMethod, jstr, jint(pos), jint(len), jint(type));
	_activity->env->DeleteLocalRef(jstr);
}

void Activity::cancelTextInput() {
	_activity->env->CallVoidMethod(_activity->clazz, _cancelInputMethod);
}

jni::Local Activity::getDisplay() const {
	return Activity_getDisplay(_activity, _sdkVersion);
}

void Activity::transferInputEvent(const core::InputEventData &event) {
	if (_rootView) {
		_rootView->handleInputEvent(event);
	}
}

void Activity::transferInputEvents(Vector<core::InputEventData> &&events) {
	if (_rootView) {
		_rootView->handleInputEvents(sp::move(events));
	}
}

void Activity::updateView() {
	if (_rootView) {
		_rootView->update(false);
	}
}

bool Activity::handleBackButton() {
	auto counter = _rootView->getBackButtonCounter();
	if (counter != 0) {
		return false;
	}
	return true;
}

extern "C"
JNIEXPORT void JNICALL
Java_org_stappler_xenolith_appsupport_AppSupportActivity_handleActivityResult(JNIEnv *env,
		jobject thiz, jlong native_pointer, jint request_code, jint result_code, jobject data) {
	auto activity = reinterpret_cast<Activity *>(native_pointer);
	if (activity) {
		activity->handleActivityResult(request_code, result_code, data);
	}
}

extern "C"
JNIEXPORT void JNICALL
Java_org_stappler_xenolith_appsupport_TextInputWrapper_nativeHandleCancelInput(JNIEnv *env,
		jobject thiz, jlong native_pointer) {
	auto activity = reinterpret_cast<Activity *>(native_pointer);
	if (activity) {
		activity->handleCancelInput();
	}
}

extern "C"
JNIEXPORT void JNICALL
Java_org_stappler_xenolith_appsupport_TextInputWrapper_nativeHandleTextChanged(JNIEnv *env,
		jobject thiz, jlong native_pointer, jstring text, jint cursor_start, jint cursor_len) {
	auto activity = reinterpret_cast<Activity *>(native_pointer);
	if (activity) {
		activity->handleTextChanged(text, cursor_start, cursor_len);
	}
}

extern "C"
JNIEXPORT void JNICALL
Java_org_stappler_xenolith_appsupport_TextInputWrapper_nativeHandleInputEnabled(JNIEnv *env,
		jobject thiz, jlong native_pointer, jboolean value) {
	auto activity = reinterpret_cast<Activity *>(native_pointer);
	if (activity) {
		activity->handleInputEnabled(value);
	}
}

}
