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

#ifndef XENOLITH_APPLICATION_PLATFORM_XLCONTEXTNATIVEWINDOW_H_
#define XENOLITH_APPLICATION_PLATFORM_XLCONTEXTNATIVEWINDOW_H_

#include "SPEnum.h"
#include "SPStatus.h"
#include "XLContextInfo.h"
#include "XLCoreTextInput.h"
#include "XLCorePresentationFrame.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class ContextController;

// Cross-OS state flags
enum class NativeWindowStateFlags {
	None = 0,
	Modal = 1 << 0,
	Sticky = 1 << 1,
	MaximizedVert = 1 << 2,
	MaximizedHorz = 1 << 3,
	Maximized = MaximizedVert | MaximizedHorz,

	Shaded = 1 << 4,
	SkipTaskbar = 1 << 5,
	SkipPager = 1 << 6,
	Hidden = 1 << 7,
	Fullscreen = 1 << 8,
	Above = 1 << 9,
	Below = 1 << 10,
	DemandsAttention = 1 << 11,
	Focused = 1 << 12,

	Resizing = 1 << 13,

	TiledLeft = 1 << 20,
	TiledRight = 1 << 21,
	TiledTop = 1 << 22,
	TiledBottom = 1 << 23,

	TiledTopLeft = TiledLeft | TiledTop,
	TiledTopRight = TiledRight | TiledTop,
	TiledBottomLeft = TiledLeft | TiledBottom,
	TiledBottomRight = TiledRight | TiledBottom,

	ConstrainedLeft = 1 << 24,
	ConstrainedRight = 1 << 25,
	ConstrainedTop = 1 << 26,
	ConstrainedBottom = 1 << 27,
};

SP_DEFINE_ENUM_AS_MASK(NativeWindowStateFlags)

class NativeWindow : public Ref {
public:
	using TextInputProcessor = core::TextInputProcessor;
	using InputEventData = core::InputEventData;
	using InputEventName = core::InputEventName;
	using TextInputRequest = core::TextInputRequest;
	using TextInputState = core::TextInputState;
	using TextInputFlags = core::TextInputFlags;

	virtual ~NativeWindow();

	virtual bool init(NotNull<ContextController>, Rc<WindowInfo> &&, WindowCapabilities);

	virtual void mapWindow() = 0;
	virtual void unmapWindow() = 0;
	virtual bool close() = 0;

	virtual void handleFramePresented(NotNull<core::PresentationFrame>) { }
	virtual void handleLayerUpdate(const WindowLayer &) { }

	virtual core::SurfaceInfo getSurfaceOptions(core::SurfaceInfo &&info) const {
		return move(info);
	}

	virtual core::FrameConstraints exportConstraints(core::FrameConstraints &&) const = 0;

	virtual Extent2 getExtent() const = 0;

	virtual Rc<core::Surface> makeSurface(NotNull<core::Instance>) = 0;

	virtual void setExitGuard(bool value) { _hasExitGuard = value; }
	virtual bool hasExitGuard() const { return _hasExitGuard; }

	virtual void setInsetDecorationVisible(bool) { }
	virtual void setInsetDecorationTone(float) { }

	virtual core::PresentationOptions getPreferredOptions() const {
		return core::PresentationOptions();
	}

	bool isRootWindow() const { return _isRootWindow; }

	void setFrameOrder(uint64_t v) { _frameOrder = v; }
	uint64_t getFrameOrder() const { return _frameOrder; }

	bool isTextInputEnabled() const { return _textInput->isRunning(); }

	const WindowInfo *getInfo() const { return _info; }

	// application requests
	void acquireTextInput(const TextInputRequest &);
	void releaseTextInput();

	// returns true if some layer were found
	// callback: return false to stop iterating
	bool findLayers(Vec2, const Callback<bool(const WindowLayer &)> &) const;

	void setAppWindow(Rc<AppWindow> &&);
	AppWindow *getAppWindow() const;

	virtual void updateLayers(Vector<WindowLayer> &&);

	virtual void setFullscreen(FullscreenInfo &&, Function<void(Status)> &&cb, Ref *ref);

protected:
	// Run text input mode or update text input buffer
	//
	// should be forwarded to OS input method
	virtual bool updateTextInput(const TextInputRequest &,
			TextInputFlags flags = TextInputFlags::RunIfDisabled) = 0;

	// Disable text input, if it was enabled
	//
	// should be forwarded to OS input method
	virtual void cancelTextInput() = 0; // from view thread

	virtual void handleInputEvents(Vector<InputEventData> &&events);

	virtual void handleMotionEvent(const InputEventData &);

	virtual Status setFullscreenState(FullscreenInfo &&) { return Status::ErrorNotImplemented; }

	uint64_t _frameOrder = 0;

	Rc<ContextController> _controller;
	Rc<WindowInfo> _info;
	Rc<core::TextInputProcessor> _textInput;

	Rc<AppWindow> _appWindow;

	// usually, text input can be captured from keyboard, but on some systems text input separated from keyboard input
	bool _handleTextInputFromKeyboard = true;

	// intercept pointer motion event to track current top layer
	bool _handleLayerForMotion = true;

	bool _isRootWindow = true;
	bool _hasExitGuard = false;

	Vec2 _layerLocation;
	Vector<WindowLayer> _layers;
	WindowLayer _currentLayer;

	NativeWindowStateFlags _state = NativeWindowStateFlags::None;
};

const CallbackStream &operator<<(const CallbackStream &, NativeWindowStateFlags);

} // namespace stappler::xenolith::platform

#endif /* XENOLITH_APPLICATION_PLATFORM_XLCONTEXTNATIVEWINDOW_H_ */
