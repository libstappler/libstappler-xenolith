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

#ifndef XENOLITH_APPLICATION_XLCONTEXT_H_
#define XENOLITH_APPLICATION_XLCONTEXT_H_

#include "XLEvent.h"
#include "XLContextInfo.h"
#include "XLCoreTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class ContextController;
class ContextNativeWindow;

} // namespace stappler::xenolith::platform

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Context;
class AppThread;
class AppWindow;

using NativeWindow = platform::ContextNativeWindow;

class ContextComponent : public Ref {
public:
	virtual ~ContextComponent() { }

	virtual bool init() { return true; }

	virtual void handleStart(Context *a) { }
	virtual void handleResume(Context *a) { }
	virtual void handlePause(Context *a) { }
	virtual void handleStop(Context *a) { }
	virtual void handleDestroy(Context *a) { }
	virtual void handleLowMemory(Context *a) { }
};

class Context : public Ref {
public:
	using SwapchainConfig = core::SwapchainConfig;
	using SurfaceInfo = core::SurfaceInfo;

	static EventHeader onMessageToken;
	static EventHeader onRemoteNotification;

	Context();
	virtual ~Context();

	virtual bool init(ContextConfig &&);

	virtual int run();

	const ContextInfo *getInfo() const { return _info; }
	event::Looper *getLooper() const { return _looper; }

	core::Loop *getGlLoop() const { return _loop; }

	BytesView getMessageToken() const { return _messageToken; }

	virtual void performOnThread(Function<void()> &&func, Ref *target = nullptr,
			bool immediate = false, StringView tag = STAPPLER_LOCATION) const;

	template <typename T>
	auto addComponent(Rc<T> &&) -> T *;

	template <typename T>
	T *getComponent() const;

	virtual void readFromClipboard(Function<void(BytesView, StringView)> &&, Ref *);
	virtual void writeToClipboard(BytesView, StringView contentType = StringView());

	virtual void handleConfigurationChanged(Rc<ContextInfo> &&);

	virtual void handleGraphicsLoaded(NotNull<core::Loop>);

	virtual Value saveState();

	virtual void handleAppThreadCreated(NotNull<AppThread>);
	virtual void handleAppThreadDestroyed(NotNull<AppThread>);
	virtual void handleAppThreadUpdate(NotNull<AppThread>, const UpdateTime &time);

	virtual SwapchainConfig handleAppWindowSurfaceUpdate(NotNull<AppWindow>, const SurfaceInfo &);
	virtual void handleAppWindowCreated(NotNull<AppWindow>, const core::FrameConstraints &);
	virtual void handleAppWindowDestroyed(NotNull<AppWindow>);

	virtual void handleNativeWindowCreated(NotNull<NativeWindow>);
	virtual void handleNativeWindowDestroyed(NotNull<NativeWindow>);
	virtual void handleNativeWindowRedrawNeeded(NotNull<NativeWindow>);
	virtual void handleNativeWindowResized(NotNull<NativeWindow>);
	virtual void handleNativeWindowInputEvents(NotNull<NativeWindow>,
			Vector<core::InputEventData> &&);
	virtual void handleNativeWindowTextInput(NotNull<NativeWindow>, const core::TextInputState &);

	virtual void handleLowMemory();

	virtual void handleWillDestroy();
	virtual void handleDidDestroy();

	virtual void handleWillStop();
	virtual void handleDidStop();

	virtual void handleWillPause();
	virtual void handleDidPause();

	virtual void handleWillResume();
	virtual void handleDidResume();

	virtual void handleWillStart();
	virtual void handleDidStart();

	virtual void handleWindowFocusChanged(int focused);

	virtual bool handleBackButton();

	virtual void addNetworkCallback(Ref *key, Function<void(NetworkFlags)> &&);
	virtual void removeNetworkCallback(Ref *key);

	virtual void updateMessageToken(BytesView tok);
	virtual void receiveRemoteNotification(Value &&val);

protected:
	virtual Rc<AppWindow> makeAppWindow(NotNull<NativeWindow>);
	virtual Rc<core::PresentationEngine> makePresentationEngine(NotNull<core::PresentationWindow>);

	virtual void initializeComponent(NotNull<ContextComponent>);

	memory::allocator_t *_alloc = nullptr;
	memory::pool_t *_pool = nullptr;
	memory::pool_t *_tmpPool = nullptr;

	event::Looper *_looper = nullptr;

	bool _isEmulator = false;
	bool _valid = false;
	bool _running = false;

	Rc<ContextInfo> _info;

	Bytes _messageToken;

	Rc<platform::ContextController> _controller;

	Rc<core::Loop> _loop;

	Rc<AppThread> _application;
	Rc<AppWindow> _rootWindow;

	HashMap<std::type_index, Rc<ContextComponent>> _components;
};

template <typename T>
auto Context::addComponent(Rc<T> &&t) -> T * {
	auto it = _components.find(std::type_index(typeid(T)));
	if (it == _components.end()) {
		it = _components.emplace(std::type_index(typeid(*t.get())), move(t)).first;
		initializeComponent(it->second);
	}
	return it->second.get_cast<T>();
}

template <typename T>
auto Context::getComponent() const -> T * {
	auto it = _components.find(std::type_index(typeid(T)));
	if (it != _components.end()) {
		return reinterpret_cast<T *>(it->second.get());
	}
	return nullptr;
}


} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLCONTEXT_H_ */
