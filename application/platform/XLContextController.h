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

#ifndef XENOLITH_APPLICATION_PLATFORM_XLCONTEXTCONTROLLER_H_
#define XENOLITH_APPLICATION_PLATFORM_XLCONTEXTCONTROLLER_H_

#include "XLContextInfo.h"
#include "XLCoreTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class ContextComponent;

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class ContextNativeWindow;

enum class ContextState {
	None,
	Created,
	Started,
	Active,
};

struct ClipboardRequest : public Ref {
	// receives data from clipboard
	Function<void(BytesView, StringView)> dataCallback;

	// Select one of suggested types to receive
	// You should not assume on what thread this function would be called,
	// so, assume the worst case
	Function<StringView(SpanView<StringView>)> typeCallback;

	// Target to keep
	Rc<Ref> target;
};

struct ClipboardData : public Ref {
	// Supported types for the data
	Vector<String> types;

	// Function to convert clipboard's data into target format
	// You should not assume on what thread this function would be called,
	// so, assume the worst case
	Function<Bytes(StringView)> encodeCallback;

	// Data owner
	Rc<Ref> owner;
};

class ContextController : public Ref {
public:
	static Rc<ContextController> create(NotNull<Context>, ContextConfig &&info);

	static void acquireDefaultConfig(ContextConfig &, NativeContextHandle *);

	virtual ~ContextController() = default;

	virtual bool init(NotNull<Context>);

	virtual int run();

	event::Looper *getLooper() const { return _looper; }

	virtual void notifyWindowConstraintsChanged(NotNull<ContextNativeWindow>, bool liveResize);
	virtual void notifyWindowInputEvents(NotNull<ContextNativeWindow>,
			Vector<core::InputEventData> &&);
	virtual void notifyWindowTextInput(NotNull<ContextNativeWindow>, const core::TextInputState &);

	// true if window should be closed, false otherwise (e.g. ExitGuard)
	virtual bool notifyWindowClosed(NotNull<ContextNativeWindow>);

	virtual Rc<AppWindow> makeAppWindow(NotNull<AppThread>, NotNull<ContextNativeWindow>);

	virtual void initializeComponent(NotNull<ContextComponent>);

	virtual Status readFromClipboard(Rc<ClipboardRequest> &&);
	virtual Status writeToClipboard(Rc<ClipboardData> &&);

	virtual Rc<ScreenInfo> getScreenInfo() const;

	virtual void handleNetworkStateChanged(NetworkFlags);
	virtual void handleThemeInfoChanged(const ThemeInfo &);

protected:
	virtual void handleStateChanged(ContextState prevState, ContextState newState);

	virtual void handleContextWillDestroy();
	virtual void handleContextDidDestroy();

	virtual void handleContextWillStop();
	virtual void handleContextDidStop();

	virtual void handleContextWillPause();
	virtual void handleContextDidPause();

	virtual void handleContextWillResume();
	virtual void handleContextDidResume();

	virtual void handleContextWillStart();
	virtual void handleContextDidStart();

	virtual bool start();
	virtual bool resume();
	virtual bool pause();
	virtual bool stop();
	virtual bool destroy();

	virtual Value saveContextState();

	virtual Rc<core::Loop> makeLoop(NotNull<core::Instance>);

	int _resultCode = 0;
	ContextState _state = ContextState::Created;
	Context *_context = nullptr;
	event::Looper *_looper = nullptr;

	Rc<ContextInfo> _contextInfo;
	Rc<WindowInfo> _windowInfo;
	Rc<core::InstanceInfo> _instanceInfo;
	Rc<core::LoopInfo> _loopInfo;

	NetworkFlags _networkFlags = NetworkFlags::None;
	ThemeInfo _themeInfo;
};

} // namespace stappler::xenolith::platform

#endif /* XENOLITH_APPLICATION_PLATFORM_XLCONTEXTCONTROLLER_H_ */
