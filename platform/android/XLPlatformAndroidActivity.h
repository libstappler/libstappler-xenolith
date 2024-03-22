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

#ifndef XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROIDACTIVITY_H_
#define XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROIDACTIVITY_H_

#include "XLPlatformAndroidClassLoader.h"
#include "XLPlatformAndroidNetworkConnectivity.h"
#include "XLPlatformViewInterface.h"
#include "XLCoreInput.h"

#if ANDROID

#include <jni.h>

#include <android/native_activity.h>
#include <android/configuration.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

enum class ActivityFlags {
	None = 0,
	CaptureInput = 1 << 0,
};

struct ActivityInfo {
	String bundleName;
	String applicationName;
	String applicationVersion;
	String userAgent;
	String systemAgent;
	String locale;

	float density = 1.0f;

	Extent2 sizeInPixels;
	Size2 sizeInDp;
};

class Activity;
class EngineMainThread;

struct ActivityTextInputWrapper : public Ref {
	Rc<Ref> target;
	Function<void(Ref *, WideStringView, core::TextCursor)> textChanged;
	Function<void(Ref *, bool)> inputEnabled;
	Function<void(Ref *)> cancelInput;
};

class ActivityComponent : public Ref {
public:
	virtual ~ActivityComponent() { }

	virtual bool init() {
		return true;
	}

	virtual void handleAdded(Activity *a) { }
	virtual void handlePause(Activity *a) { }
	virtual void handleStart(Activity *a) { }
	virtual void handleResume(Activity *a) { }
	virtual void handleStop(Activity *a) { }
	virtual void handleDestroy(Activity *a) { }

	virtual void handleConfigurationChanged(Activity *a, AConfiguration *) { }

	virtual void handleLowMemory(Activity *a) { }

	// return true if result was processed
	virtual bool handleActivityResult(Activity *a, int request_code, int result_code, jobject data) { return false; }
};

class Activity : public Ref {
public:
	static constexpr StringView NetworkConnectivityClassName = "org.stappler.xenolith.appsupport.NetworkConnectivity";
	static constexpr StringView NetworkConnectivityClassPath = "org/stappler/xenolith/appsupport/NetworkConnectivity";

	static constexpr int FLAG_ACTIVITY_NEW_TASK =  268435456;

	Activity();
	virtual ~Activity();

	virtual bool init(ANativeActivity *activity, ActivityFlags flags);

	virtual NetworkCapabilities getNetworkCapabilities() const;
	virtual void setNetworkCapabilities(NetworkCapabilities);

	virtual void addNetworkCallback(void *key, Function<void(NetworkCapabilities)> &&);
	virtual void removeNetworkCallback(void *key);

	virtual void addRemoteNotificationCallback(void *key, Function<void(const Value &)> &&);
	virtual void removeRemoteNotificationCallback(void *key);

	virtual void addTokenCallback(void *key, Function<void(StringView)> &&);
	virtual void removeTokenCallback(void *key);

	virtual void wakeup();
	virtual void setView(platform::ViewInterface *);

	virtual void run(Function<void(Activity *, Function<void()> &&)> &&);

	String getMessageToken() const;
	void setMessageToken(StringView);

	void handleRemoteNotification(const Value &);

	ClassLoader *getClassLoader() const { return _classLoader; }
	ANativeActivity *getNativeActivity() const { return _activity; }
	const NativeBufferFormatSupport &getFormatSupport() const { return _formatSupport; }

	const ActivityInfo &getActivityInfo() const { return _info; }

	virtual void addComponent(Rc<ActivityComponent> &&);

	void handleActivityResult(jint request_code, jint result_code, jobject data);
	void handleCancelInput();
	void handleTextChanged(jstring text, jint cursor_start, jint cursor_len);
	void handleInputEnabled(jboolean);

	virtual void openUrl(StringView url);

	virtual void updateTextCursor(uint32_t pos, uint32_t len);
	virtual void updateTextInput(WideStringView str, uint32_t pos, uint32_t len, core::TextInputType);
	virtual void runTextInput(Rc<ActivityTextInputWrapper> &&, WideStringView str, uint32_t pos, uint32_t len, core::TextInputType);
	virtual void cancelTextInput();

	jobject getClipboardService() const { return _clipboardServce; }

protected:
	struct InputLooperData {
		Activity *activity;
		AInputQueue *queue;
	};

	jclass getClass() const {
		return _activity->env->GetObjectClass(_activity->clazz);
	}

	jmethodID getMethodID(jclass cl, StringView method, StringView params) const {
		return _activity->env->GetMethodID(cl, method.data(), params.data());
	}

	void handleConfigurationChanged();
	void handleContentRectChanged(const ARect *);

	void handleInputQueueCreated(AInputQueue *);
	void handleInputQueueDestroyed(AInputQueue *);
	void handleLowMemory();
	void* handleSaveInstanceState(size_t* outLen);

	void handleNativeWindowCreated(ANativeWindow *);
	void handleNativeWindowDestroyed(ANativeWindow *);
	void handleNativeWindowRedrawNeeded(ANativeWindow *);
	void handleNativeWindowResized(ANativeWindow *);

	void handlePause();
	void handleStart();
	void handleResume();
	void handleStop();
	void handleDestroy();

	void handleWindowFocusChanged(int focused);

	int handleLooperEvent(int fd, int events);
	int handleInputEventQueue(int fd, int events, AInputQueue *);
	int handleInputEvent(AInputEvent *);
	int handleKeyEvent(AInputEvent *);
	int handleMotionEvent(AInputEvent *);

	ActivityInfo getActivityInfo(AConfiguration *);

	platform::ViewInterface *waitForView();

	virtual void transferInputEvent(const core::InputEventData &);
	virtual void transferInputEvents(Vector<core::InputEventData> &&);
	virtual void updateView();
	virtual bool handleBackButton();

	jmethodID _startActivityMethod;
	jmethodID _intentInitMethod;
	jmethodID _uriParseMethod;
	jmethodID _intentAddFlagsMethod;
	jfieldID _intentActionView;

	jmethodID _runInputMethod;
	jmethodID _updateInputMethod;
	jmethodID _updateCursorMethod;
	jmethodID _cancelInputMethod;

	jobject _clipboardServce;

	ActivityFlags _flags = ActivityFlags::None;
	ANativeActivity *_activity = nullptr;
	AConfiguration *_config = nullptr;
	ALooper *_looper = nullptr;
	Rc<ClassLoader> _classLoader;
	Rc<NetworkConnectivity> _networkConnectivity;

	ViewInterfaceRef _rootViewTmp = nullptr;
	ViewInterfaceRef _rootView = nullptr;
	std::mutex _rootViewMutex;
	std::condition_variable _rootViewVar;

	int _eventfd = -1;
	int _timerfd = -1;

	Map<AInputQueue *, InputLooperData> _input;

	float _density = 1.0f;
	core::InputModifier _activeModifiers = core::InputModifier::None;
	int32_t _sdkVersion = 0;

	Size2 _windowSize;
	Vec2 _hoverLocation;
	NativeBufferFormatSupport _formatSupport;
	bool isEmulator = false;

	ActivityInfo _info;
	Rc<EngineMainThread> _thread;
	uint64_t _refId = 0;

	mutable std::mutex _callbackMutex;
	String _messageToken;
	NetworkCapabilities _capabilities = NetworkCapabilities::None;
	Map<void *, Pair<Rc<Ref>, Function<void(NetworkCapabilities)>>> _networkCallbacks;
	Map<void *, Pair<Rc<Ref>, Function<void(const Value &)>>> _notificationCallbacks;
	Map<void *, Pair<Rc<Ref>, Function<void(StringView)>>> _tokenCallbacks;

	Value _drawables;
	Vector<Rc<ActivityComponent>> _components;
	Rc<ActivityTextInputWrapper> _textInputWrapper;
};

SP_DEFINE_ENUM_AS_MASK(ActivityFlags)

}

#endif

#endif /* XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROIDACTIVITY_H_ */
