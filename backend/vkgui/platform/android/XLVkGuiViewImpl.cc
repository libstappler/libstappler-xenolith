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

#include "XLVkGuiViewImpl.h"
#include "XLTextInputManager.h"

#if ANDROID

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

static int flag_SYSTEM_UI_FLAG_LAYOUT_STABLE;
static int flag_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
static int flag_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
static int flag_SYSTEM_UI_FLAG_HIDE_NAVIGATION;
static int flag_SYSTEM_UI_FLAG_FULLSCREEN;
static int flag_SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
static int flag_SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
static int flag_SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;

static int flag_FLAG_TRANSLUCENT_STATUS;
static int flag_FLAG_TRANSLUCENT_NAVIGATION;
static int flag_FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS;
static int flag_FLAG_FULLSCREEN;
static int flag_FLAG_LAYOUT_INSET_DECOR;
static int flag_FLAG_LAYOUT_IN_SCREEN;

ViewImpl::ViewImpl() { }

ViewImpl::~ViewImpl() { }

bool ViewImpl::init(Application &loop, core::Device &dev, ViewInfo &&info) {
	if (!View::init(loop, static_cast<vk::Device &>(dev), move(info))) {
		return false;
	}

	_options.presentImmediate = false;
	_options.acquireImageImmediately = true;
	_options.renderOnDemand = true;

	return true;
}

void ViewImpl::run() {
	reinterpret_cast<xenolith::platform::Activity *>(_mainLoop->getInfo().nativeHandle)->setView(this);
}

void ViewImpl::threadInit() {
	_started = true;

	auto activity = static_cast<xenolith::platform::Activity *>(_mainLoop->getInfo().nativeHandle);
	setActivity(activity);

	View::threadInit();
}

void ViewImpl::mapWindow() {
	View::mapWindow();
}

void ViewImpl::threadDispose() {
	View::threadDispose();
	if (_nativeWindow) {
		ANativeWindow_release(_nativeWindow);
		_nativeWindow = nullptr;
	}
	_surface = nullptr;
	_started = false;
}

bool ViewImpl::worker() {
	return false;
}

void ViewImpl::update(bool displayLink) {
	if (displayLink) {
		if (_initImage) {
			presentImmediate(move(_initImage), nullptr);
			_initImage = nullptr;
			View::update(false);
			//scheduleNextImage(_frameInterval, false);
			return;
		}

		/*while (_scheduledPresent.empty()) {
			View::update(false);
		}*/
		View::update(true);
	} else {
		View::update(displayLink);
	}
}

void ViewImpl::end() {
	performOnThread([this] {
		threadDispose();
		vk::View::end();
	}, this, true);
}

void ViewImpl::wakeup(std::unique_lock<Mutex> &) {
	reinterpret_cast<xenolith::platform::Activity *>(_mainLoop->getInfo().nativeHandle)->wakeup();
}

void ViewImpl::updateTextCursor(uint32_t pos, uint32_t len) {
	performOnThread([this, pos, len] {
		if (_activity) {
			_activity->updateTextCursor(pos, len);
		}
	}, this, true);
}

void ViewImpl::updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType type) {
	performOnThread([this, str = str.str<Interface>(), pos, len, type] {
		if (_activity) {
			_activity->updateTextInput(str, pos, len, type);
		}
	}, this, true);
}

void ViewImpl::runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType type) {
	performOnThread([this, str = str.str<Interface>(), pos, len, type] {
		if (_activity) {
			auto wrapper = Rc<xenolith::platform::ActivityTextInputWrapper>::alloc();
			wrapper->target = this;
			wrapper->textChanged = [this] (Ref *ref, WideStringView text, core::TextCursor cursor) {
				_mainLoop->performOnMainThread([dir = _director, view = Rc<ViewImpl>(this), text = text.str<Interface>(), cursor] {
					dir->getTextInputManager()->textChanged(text, cursor, core::TextCursor());
					view->setReadyForNextFrame();
				}, ref);
			};
			wrapper->inputEnabled = [this] (Ref *ref, bool value) {
				_mainLoop->performOnMainThread([dir = _director, view = Rc<ViewImpl>(this), value] {
					dir->getTextInputManager()->setInputEnabled(value);
					view->setReadyForNextFrame();
				}, ref);
			};
			wrapper->cancelInput = [this] (Ref *ref) {
				_mainLoop->performOnMainThread([dir = _director, view = Rc<ViewImpl>(this)] {
					dir->getTextInputManager()->cancel();
					view->setReadyForNextFrame();
				}, ref);
			};

			_activity->runTextInput(move(wrapper), str, pos, len, type);
		}
	}, this, true);
}

void ViewImpl::cancelTextInput() {
	performOnThread([this] {
		if (_activity) {
			_activity->cancelTextInput();
		}
	}, this, true);
}

void ViewImpl::runWithWindow(ANativeWindow *window) {
	auto instance = _instance.cast<vk::Instance>();

	VkSurfaceKHR targetSurface;
	VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo;
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.pNext = nullptr;
	surfaceCreateInfo.flags = 0;
	surfaceCreateInfo.window = window;

	_nativeWindow = window;
	_constraints.extent = Extent2(ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));

	if (instance->vkCreateAndroidSurfaceKHR(instance->getInstance(), &surfaceCreateInfo, nullptr, &targetSurface) != VK_SUCCESS) {
		log::error("ViewImpl", "fail to create surface");
		return;
	}

	_surface = Rc<vk::Surface>::create(instance, targetSurface);
	ANativeWindow_acquire(_nativeWindow);

	auto info = View::getSurfaceOptions();
	if ((info.currentTransform & core::SurfaceTransformFlags::Rotate90) != core::SurfaceTransformFlags::None ||
		(info.currentTransform & core::SurfaceTransformFlags::Rotate270) != core::SurfaceTransformFlags::None) {
		_identityExtent = Extent2(info.currentExtent.height, info.currentExtent.width);
	} else {
		_identityExtent = info.currentExtent;
	}

	if (!_started) {
		_options.followDisplayLink = true;
		threadInit();
		_options.followDisplayLink = false;
	} else {
		initWindow();
	}
}

void ViewImpl::initWindow() {
	auto info = getSurfaceOptions();
	core::SwapchainConfig cfg = _info.selectConfig(*this, info);

	createSwapchain(info, move(cfg), cfg.presentMode);

	if (_initImage && !_options.followDisplayLink) {
		presentImmediate(move(_initImage), nullptr);
		_initImage = nullptr;
	}

	mapWindow();
}

void ViewImpl::stopWindow() {
	_surface = nullptr;

	_swapchain->deprecate(false);
	recreateSwapchain(core::PresentMode::Unsupported);

	std::unique_lock lock(_windowMutex);
	_glLoop->performOnGlThread([this] {
		std::unique_lock windowLock(_windowMutex);
		_glLoop->waitIdle();
		_windowCond.notify_all();
	});

	_windowCond.wait(lock);

	clearImages();

	for (auto &it : _scheduledPresent) {
		invalidateSwapchainImage(it);
	}

	_scheduledPresent.clear();

	if (_swapchain) {
		_swapchain = nullptr;
	}
	_surface = nullptr;

	if (_nativeWindow) {
		ANativeWindow_release(_nativeWindow);
		_nativeWindow = nullptr;
	}

	_framesInProgress = 0;
}

void ViewImpl::setActivity(xenolith::platform::Activity *activity) {
	_activity = activity;

	auto jactivity = _activity->getNativeActivity()->clazz;
	auto env = _activity->getNativeActivity()->env;

	jclass activityClass = env->FindClass("android/app/NativeActivity");
	jclass windowClass = env->FindClass("android/view/Window");
	jclass viewClass = env->FindClass("android/view/View");
	jclass layoutClass = env->FindClass("android/view/WindowManager$LayoutParams");
	jmethodID getWindow = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");
	jmethodID clearFlags = env->GetMethodID(windowClass, "clearFlags", "(I)V");
	jmethodID addFlags = env->GetMethodID(windowClass, "addFlags", "(I)V");

	jobject windowObj = env->CallObjectMethod(jactivity, getWindow);

	jfieldID id_SYSTEM_UI_FLAG_LAYOUT_STABLE = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LAYOUT_STABLE", "I");
	jfieldID id_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION", "I");
	jfieldID id_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN", "I");
	jfieldID id_SYSTEM_UI_FLAG_HIDE_NAVIGATION = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_HIDE_NAVIGATION", "I");
	jfieldID id_SYSTEM_UI_FLAG_FULLSCREEN = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_FULLSCREEN", "I");
	jfieldID id_SYSTEM_UI_FLAG_IMMERSIVE_STICKY = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_IMMERSIVE_STICKY", "I");
	jfieldID id_SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR", "I");
	jfieldID id_SYSTEM_UI_FLAG_LIGHT_STATUS_BAR = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LIGHT_STATUS_BAR", "I");

	flag_SYSTEM_UI_FLAG_LAYOUT_STABLE = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_LAYOUT_STABLE);
	flag_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
	flag_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
	flag_SYSTEM_UI_FLAG_HIDE_NAVIGATION = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_HIDE_NAVIGATION);
	flag_SYSTEM_UI_FLAG_FULLSCREEN = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_FULLSCREEN);
	flag_SYSTEM_UI_FLAG_IMMERSIVE_STICKY = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
	flag_SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR);
	flag_SYSTEM_UI_FLAG_LIGHT_STATUS_BAR = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_LIGHT_STATUS_BAR);

	jfieldID id_FLAG_TRANSLUCENT_STATUS = env->GetStaticFieldID(layoutClass, "FLAG_TRANSLUCENT_STATUS", "I");
	jfieldID id_FLAG_TRANSLUCENT_NAVIGATION = env->GetStaticFieldID(layoutClass, "FLAG_TRANSLUCENT_NAVIGATION", "I");
	jfieldID id_FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS = env->GetStaticFieldID(layoutClass, "FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS", "I");
	jfieldID id_FLAG_FULLSCREEN = env->GetStaticFieldID(layoutClass, "FLAG_FULLSCREEN", "I");
	jfieldID id_FLAG_LAYOUT_INSET_DECOR = env->GetStaticFieldID(layoutClass, "FLAG_LAYOUT_INSET_DECOR", "I");
	jfieldID id_FLAG_LAYOUT_IN_SCREEN = env->GetStaticFieldID(layoutClass, "FLAG_LAYOUT_IN_SCREEN", "I");

	flag_FLAG_TRANSLUCENT_STATUS = env->GetStaticIntField(layoutClass, id_FLAG_TRANSLUCENT_STATUS);
	flag_FLAG_TRANSLUCENT_NAVIGATION = env->GetStaticIntField(layoutClass, id_FLAG_TRANSLUCENT_NAVIGATION);
	flag_FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS = env->GetStaticIntField(layoutClass, id_FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
	flag_FLAG_FULLSCREEN = env->GetStaticIntField(layoutClass, id_FLAG_FULLSCREEN);
	flag_FLAG_LAYOUT_INSET_DECOR = env->GetStaticIntField(layoutClass, id_FLAG_LAYOUT_INSET_DECOR);
	flag_FLAG_LAYOUT_IN_SCREEN = env->GetStaticIntField(layoutClass, id_FLAG_LAYOUT_IN_SCREEN);

	env->CallVoidMethod(windowObj, clearFlags, flag_FLAG_TRANSLUCENT_NAVIGATION | flag_FLAG_TRANSLUCENT_STATUS);
	env->CallVoidMethod(windowObj, addFlags, flag_FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS
		| flag_FLAG_LAYOUT_INSET_DECOR | flag_FLAG_LAYOUT_IN_SCREEN);

	updateDecorations();
}

bool ViewImpl::pollInput(bool frameReady) {
	if (!_nativeWindow) {
		return false;
	}

	return true;
}

core::SurfaceInfo ViewImpl::getSurfaceOptions() const {
	auto info = View::getSurfaceOptions();

	if (_usePreRotation) {
		// always use identity extent for pre-rotation;
		info.currentExtent = _identityExtent;
		info.currentTransform |= core::SurfaceTransformFlags::PreRotated;
	} else {
		if (info.currentExtent != _identityExtent && (info.currentTransform == core::SurfaceTransformFlags::Identity
													  || info.currentTransform == core::SurfaceTransformFlags::Rotate180)) {
			info.currentExtent = _identityExtent;
			log::warn("ViewImpl", "Fixed:", info.currentExtent,
					   " Rotation: ", core::getSurfaceTransformFlagsDescription(info.currentTransform));
		}

		if (info.currentExtent == _identityExtent && (info.currentTransform == core::SurfaceTransformFlags::Rotate270
													  || info.currentTransform == core::SurfaceTransformFlags::Rotate90)) {
			info.currentExtent = Extent2(_identityExtent.height, _identityExtent.width);
			log::warn("ViewImpl", "Fixed:", info.currentExtent,
					   " Rotation: ", core::getSurfaceTransformFlagsDescription(info.currentTransform));
		}
	}

	auto it = info.formats.begin();
	while (it != info.formats.end()) {
		switch (it->first) {
		case core::ImageFormat::R8G8B8A8_UNORM:
		case core::ImageFormat::R8G8B8A8_SRGB:
			if (!_activity->getFormatSupport().R8G8B8A8_UNORM) {
				it = info.formats.erase(it);
			} else {
				++ it;
			}
			break;
		case core::ImageFormat::R8G8B8_UNORM:
			if (!_activity->getFormatSupport().R8G8B8_UNORM) {
				it = info.formats.erase(it);
			} else {
				++ it;
			}
			break;
		case core::ImageFormat::R5G6B5_UNORM_PACK16:
			if (!_activity->getFormatSupport().R5G6B5_UNORM) {
				it = info.formats.erase(it);
			} else {
				++ it;
			}
			break;
		case core::ImageFormat::R16G16B16A16_SFLOAT:
			if (!_activity->getFormatSupport().R16G16B16A16_FLOAT) {
				it = info.formats.erase(it);
			} else {
				++ it;
			}
			break;
		default:
			++ it;
		}
	}

	return info;
}

void ViewImpl::setDecorationTone(float value) {
	performOnThread([this, value] {
		doSetDecorationTone(value);
	});
}

void ViewImpl::setDecorationVisible(bool value) {
	performOnThread([this, value] {
		doSetDecorationVisible(value);
	});
}

bool ViewImpl::isInputEnabled() const {
	return false;
}

void ViewImpl::linkWithNativeWindow(void *window) {
	runWithWindow(static_cast<ANativeWindow *>(window));
}

void ViewImpl::stopNativeWindow() {
	stopWindow();
}

void ViewImpl::doSetDecorationTone(float value) {
	_decorationTone = value;
	updateDecorations();
}

void ViewImpl::doSetDecorationVisible(bool value) {
	_decorationVisible = value;
	updateDecorations();
}

void ViewImpl::updateDecorations() {
	if (!_activity) {
		return;
	}

	auto env = _activity->getNativeActivity()->env;
	auto jactivity = _activity->getNativeActivity()->clazz;

	jclass activityClass = env->FindClass("android/app/NativeActivity");
	jclass windowClass = env->FindClass("android/view/Window");
	jclass viewClass = env->FindClass("android/view/View");
	jmethodID getWindow = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");
	jmethodID setNavigationBarColor = env->GetMethodID(windowClass, "setNavigationBarColor", "(I)V");
	jmethodID setStatusBarColor = env->GetMethodID(windowClass, "setStatusBarColor", "(I)V");
	jmethodID clearFlags = env->GetMethodID(windowClass, "clearFlags", "(I)V");
	jmethodID addFlags = env->GetMethodID(windowClass, "addFlags", "(I)V");
	jmethodID getDecorView = env->GetMethodID(windowClass, "getDecorView", "()Landroid/view/View;");
	jmethodID setSystemUiVisibility = env->GetMethodID(viewClass, "setSystemUiVisibility", "(I)V");
	jmethodID getSystemUiVisibility = env->GetMethodID(viewClass, "getSystemUiVisibility", "()I");

	jobject windowObj = env->CallObjectMethod(jactivity, getWindow);
	jobject decorViewObj = env->CallObjectMethod(windowObj, getDecorView);

	const int currentVisibility = env->CallIntMethod(decorViewObj, getSystemUiVisibility);
	int updatedVisibility = currentVisibility;
	updatedVisibility |= flag_SYSTEM_UI_FLAG_LAYOUT_STABLE;

	if (_decorationTone < 0.5f) {
		env->CallVoidMethod(windowObj, setNavigationBarColor, jint(0xFFFFFFFF));
		env->CallVoidMethod(windowObj, setStatusBarColor, jint(0xFFFFFFFF));
		updatedVisibility = updatedVisibility | flag_SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
		updatedVisibility = updatedVisibility | flag_SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
	} else {
		env->CallVoidMethod(windowObj, setNavigationBarColor, jint(0xFF000000));
		env->CallVoidMethod(windowObj, setStatusBarColor, jint(0xFF000000));
		updatedVisibility = updatedVisibility & ~flag_SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
		updatedVisibility = updatedVisibility & ~flag_SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
	}

	if (_activity->getSdkVersion() < 30) {
		// Get current immersiveness
		if (_decorationVisible) {
			updatedVisibility = updatedVisibility & ~flag_SYSTEM_UI_FLAG_FULLSCREEN;
			env->CallVoidMethod(windowObj, clearFlags, flag_FLAG_FULLSCREEN);
		} else {
			updatedVisibility = updatedVisibility | flag_SYSTEM_UI_FLAG_FULLSCREEN;
			//env->CallVoidMethod(windowObj, addFlags, flag_FLAG_FULLSCREEN);
		}

		if (_decorationTone < 0.5f) {
			updatedVisibility = updatedVisibility | flag_SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
			updatedVisibility = updatedVisibility | flag_SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
		} else {
			updatedVisibility = updatedVisibility & ~flag_SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
			updatedVisibility = updatedVisibility & ~flag_SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
		}
	} else {
		static int APPEARANCE_LIGHT_STATUS_BARS = 8;
		static int APPEARANCE_LIGHT_NAVIGATION_BARS = 16;

		jclass controllerClass = env->FindClass("android/view/WindowInsetsController");
		jclass typeClass = env->FindClass("android/view/WindowInsets$Type");

		jmethodID getInsetsController = env->GetMethodID(windowClass, "getInsetsController", "()Landroid/view/WindowInsetsController;");
		jmethodID show = env->GetMethodID(controllerClass, "show", "(I)V");
		jmethodID hide = env->GetMethodID(controllerClass, "hide", "(I)V");
		jmethodID setSystemBarsAppearance = env->GetMethodID(controllerClass, "setSystemBarsAppearance", "(II)V");
		jmethodID statusBars = env->GetStaticMethodID(typeClass, "statusBars", "()I");
		jmethodID navBars = env->GetStaticMethodID(typeClass, "navigationBars", "()I");

		jobject windowObj = env->CallObjectMethod(jactivity, getWindow);
		jobject insetsControllerObj = env->CallObjectMethod(windowObj, getInsetsController);

		auto bars = env->CallStaticIntMethod(typeClass, statusBars);
		auto navs = env->CallStaticIntMethod(typeClass, navBars);

		env->CallVoidMethod(windowObj, clearFlags, flag_FLAG_FULLSCREEN);

		if (_decorationTone < 0.5f) {
			env->CallVoidMethod(insetsControllerObj, setSystemBarsAppearance,
				APPEARANCE_LIGHT_NAVIGATION_BARS | APPEARANCE_LIGHT_STATUS_BARS,
				APPEARANCE_LIGHT_NAVIGATION_BARS | APPEARANCE_LIGHT_STATUS_BARS);
		} else {
			env->CallVoidMethod(insetsControllerObj, setSystemBarsAppearance,
								0,
								APPEARANCE_LIGHT_NAVIGATION_BARS | APPEARANCE_LIGHT_STATUS_BARS);
		}

		if (_decorationVisible) {
			if (!_decorationShown) {
				env->CallVoidMethod(insetsControllerObj, show, bars);
				_decorationShown = true;
			}
		} else {
			if (_decorationShown) {
				env->CallVoidMethod(insetsControllerObj, hide, bars);
				_decorationShown = false;
			}
		}

		env->CallVoidMethod(insetsControllerObj, show, navs);
	}

	// log::debug("ViewImpl", "setDecorationVisible ", _decorationVisible);
	if (currentVisibility != updatedVisibility) {
		// log::debug("ViewImpl", "updatedVisibility ", updatedVisibility);
		env->CallVoidMethod(decorViewObj, setSystemUiVisibility, updatedVisibility);
	}

	xenolith::platform::checkJniError(env);

	env->DeleteLocalRef(windowObj);
	env->DeleteLocalRef(decorViewObj);
}

void ViewImpl::readFromClipboard(Function<void(BytesView, StringView)> &&cb, Ref *ref) {
	performOnThread([this, cb = move(cb), ref = Rc<Ref>(ref)] () mutable {
		doReadFromClipboard([this, cb = move(cb)] (BytesView view, StringView ct) mutable {
			_mainLoop->performOnMainThread([this, cb = move(cb), view = view.bytes<Interface>(), ct = ct.str<Interface>()] () {
				cb(view, ct);
			}, this);
		}, ref);
	}, this);
}

void ViewImpl::writeToClipboard(BytesView data, StringView contentType) {
	performOnThread([this, data = data.bytes<Interface>(), contentType = contentType.str<Interface>()] {
		doWriteToClipboard(data, contentType);
	}, this);
}

void ViewImpl::doReadFromClipboard(Function<void(BytesView, StringView)> &&cb, Ref *ref) {
	auto a = _activity->getNativeActivity();
	auto clipboard = _activity->getClipboardService();
	jclass clipboardClass = a->env->FindClass("android/content/ClipboardManager");
	jclass clipDataClass = a->env->FindClass("android/content/ClipData");
	jclass clipDescriptionClass = a->env->FindClass("android/content/ClipDescription");
	jclass clipItemClass = a->env->FindClass("android/content/ClipData$Item");
	jclass charSequenceClass = a->env->FindClass("java/lang/CharSequence");

	auto getPrimaryClipFn = a->env->GetMethodID(clipboardClass, "getPrimaryClip", "()Landroid/content/ClipData;");
	auto getClipDescriptionFn = a->env->GetMethodID(clipDataClass, "getDescription", "()Landroid/content/ClipDescription;");
	auto getItemCountFn = a->env->GetMethodID(clipDataClass, "getItemCount", "()I");
	auto getItemAtFn = a->env->GetMethodID(clipDataClass, "getItemAt", "(I)Landroid/content/ClipData$Item;");
	auto getMimeTypeFn = a->env->GetMethodID(clipDescriptionClass, "getMimeType", "(I)Ljava/lang/String;");
	auto coerceToTextFn = a->env->GetMethodID(clipItemClass, "coerceToText", "(Landroid/content/Context;)Ljava/lang/CharSequence;");
	auto coerceToHtmlTextFn = a->env->GetMethodID(clipItemClass, "coerceToHtmlText", "(Landroid/content/Context;)Ljava/lang/String;");
	auto toStringFn = a->env->GetMethodID(charSequenceClass, "toString", "()Ljava/lang/String;");

	auto primClip = a->env->CallObjectMethod(clipboard, getPrimaryClipFn);
	if (primClip) {
		auto count = a->env->CallIntMethod(primClip, getItemCountFn);
		if (count > 0) {
			auto desc = a->env->CallObjectMethod(primClip, getClipDescriptionFn);
			auto mime = (jstring)a->env->CallObjectMethod(desc, getMimeTypeFn);
			if (mime) {
				auto item = a->env->CallObjectMethod(primClip, getItemAtFn, 0);
				auto chars = a->env->GetStringChars(mime, nullptr);
				auto len = a->env->GetStringLength(mime);

				WideStringView mimeStr((const char16_t *)chars, len);
				auto mimeCh = string::toUtf8<memory::StandartInterface>(mimeStr);
				if (mimeStr.equals(u"text/html")) {
					auto str = (jstring)a->env->CallObjectMethod(item, coerceToHtmlTextFn);
					if (str) {
						auto clipChars = a->env->GetStringUTFChars(str, nullptr);
						auto clipLen = a->env->GetStringUTFLength(str);

						cb(BytesView((const uint8_t *)clipChars, clipLen), mimeCh);

						a->env->ReleaseStringUTFChars(str, clipChars);
						a->env->DeleteLocalRef(str);
					} else {
						cb(BytesView(), StringView());
					}
				} else {
					auto seq = a->env->CallObjectMethod(item, coerceToTextFn);
					auto str = (jstring)a->env->CallObjectMethod(seq, toStringFn);
					if (str) {
						auto clipChars = a->env->GetStringUTFChars(str, nullptr);
						auto clipLen = a->env->GetStringUTFLength(str);

						cb(BytesView((const uint8_t *)clipChars, clipLen), mimeCh);

						a->env->ReleaseStringUTFChars(str, clipChars);
						a->env->DeleteLocalRef(str);
					} else {
						cb(BytesView(), StringView());
					}

					a->env->DeleteLocalRef(seq);
				}

				a->env->ReleaseStringChars(mime, chars);
				a->env->DeleteLocalRef(mime);
				a->env->DeleteLocalRef(item);
			} else {
				cb(BytesView(), StringView());
			}

			a->env->DeleteLocalRef(desc);
		} else {
			cb(BytesView(), StringView());
		}
		a->env->DeleteLocalRef(primClip);
	} else {
		cb(BytesView(), StringView());
	}

	a->env->DeleteLocalRef(clipboardClass);
	a->env->DeleteLocalRef(clipDataClass);
	a->env->DeleteLocalRef(clipDescriptionClass);
	a->env->DeleteLocalRef(clipItemClass);
	a->env->DeleteLocalRef(charSequenceClass);
}

void ViewImpl::doWriteToClipboard(BytesView data, StringView contentType) {
	auto a = _activity->getNativeActivity();
	auto clipboard = _activity->getClipboardService();
	jclass clipboardClass = a->env->FindClass("android/content/ClipboardManager");
	jclass clipDataClass = a->env->FindClass("android/content/ClipData");

	auto newPlainTextFn = a->env->GetStaticMethodID(clipDataClass, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
	auto setPrimaryClipFn = a->env->GetMethodID(clipboardClass, "setPrimaryClip", "(Landroid/content/ClipData;)V");

	auto label = a->env->NewStringUTF(contentType.str<Interface>().data());
	auto content = a->env->NewStringUTF(data.readString().str<Interface>().data());

	auto clip = a->env->CallStaticObjectMethod(clipDataClass, newPlainTextFn, label, content);
	if (clip) {
		a->env->CallVoidMethod(clipboard, setPrimaryClipFn, clip);
		a->env->DeleteLocalRef(clip);
	}

	a->env->DeleteLocalRef(label);
	a->env->DeleteLocalRef(content);

	a->env->DeleteLocalRef(clipboardClass);
	a->env->DeleteLocalRef(clipDataClass);
}

Rc<vk::View> createView(Application &loop, const core::Device &dev, ViewInfo &&info) {
	return Rc<ViewImpl>::create(loop, const_cast<core::Device &>(dev), move(info));
}

bool initInstance(vk::platform::VulkanInstanceData &data, const vk::platform::VulkanInstanceInfo &info) {
	const char *surfaceExt = nullptr;
	const char *androidExt = nullptr;
	for (auto &extension : info.availableExtensions) {
		if (strcmp(VK_KHR_SURFACE_EXTENSION_NAME, extension.extensionName) == 0) {
			surfaceExt = extension.extensionName;
			data.extensionsToEnable.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		} else if (strcmp(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, extension.extensionName) == 0) {
			androidExt = extension.extensionName;
			data.extensionsToEnable.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
		}
	}

	if (surfaceExt && androidExt) {
		return true;
	}

	return false;
}

uint32_t checkPresentationSupport(const vk::Instance *instance, VkPhysicalDevice device, uint32_t queueIdx) {
	return 1;
}

}

#endif
