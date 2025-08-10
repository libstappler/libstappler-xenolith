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

#ifndef XENOLITH_APPLICATION_XLBASICWINDOW_H_
#define XENOLITH_APPLICATION_XLBASICWINDOW_H_

#include "XLContext.h"
#include "XLEvent.h"
#include "XLCoreTextInput.h"
#include "XLCorePresentationEngine.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Director;

enum class AppWindowConfigFlags {
	None = 0,
};

class SP_PUBLIC AppWindow : public Ref, core::PresentationWindow {
public:
	using InputEventData = core::InputEventData;
	using InputEventName = core::InputEventName;
	using TextInputRequest = core::TextInputRequest;
	using TextInputState = core::TextInputState;

	// In most cases, this can be received via InputListener,
	// but for objects without scene binding you can use this events
	static EventHeader onBackground;
	static EventHeader onFocus;
	static EventHeader onFullscreen;

	virtual ~AppWindow();

	virtual bool init(NotNull<Context>, NotNull<AppThread>, NotNull<NativeWindow>);

	virtual void runWithQueue(const Rc<core::Queue> &); // from view thread

	virtual void run(); // from view thread

	virtual void update(core::PresentationUpdateFlags); // from view thread
	virtual void end(); // from view thread

	virtual void close(bool graceful = true);

	virtual void handleInputEvents(Vector<InputEventData> &&);
	virtual void handleTextInput(const TextInputState &);

	Context *getContext() const { return _context; }
	AppThread *getApplication() const { return _application; }

	const WindowInfo *getInfo() const;

	WindowCapabilities getCapabilities() const;

	core::PresentationEngine *getPresentationEngine() const { return _presentationEngine; }

	Director *getDirector() const { return _director; }

	// It's not safe to ask PresentationEngine about current config, use this instead
	const core::SwapchainConfig &getAppSwapchainConfig() const { return _appSwapchainConfig; }

	// Run constraints update process
	void updateConstraints(core::PresentationSwapchainFlags); // from any thread

	void setReadyForNextFrame(); // from any thread

	// Block current thread until next frame
	void waitUntilFrame(); // from any thread

	void setRenderOnDemand(bool value); // from any thread
	bool isRenderOnDemand() const; // from any thread

	void setFrameInterval(uint64_t); // from any thread
	uint64_t getFrameInterval() const; // from any thread

	// exit guard will prevent OS WM to close window on it's side
	void retainExitGuard();
	void releaseExitGuard();

	void setInsetDecoration(const Padding &padding);

	void setInsetDecorationVisible(bool);
	bool isInsetDecorationVisible() const;

	void setInsetDecorationTone(float);
	float getInsetDecorationTone() const;

	void acquireTextInput(TextInputRequest &&);
	void releaseTextInput();

	void updateLayers(Vector<WindowLayer> &&); // from app thread

	// native window interface for app thread
	void acquireScreenInfo(Function<void(NotNull<ScreenInfo>)> &&, Ref * = nullptr);

	bool setFullscreen(FullscreenInfo &&, Function<void(Status)> &&, Ref * = nullptr);
	bool isFullscreen() const { return _isFullscreen; }

	void captureScreenshot(Function<void(const core::ImageInfoData &info, BytesView view)> &&cb);

protected:
	virtual core::ImageInfo getSwapchainImageInfo(const core::SwapchainConfig &cfg) const override;
	virtual core::ImageViewInfo getSwapchainImageViewInfo(
			const core::ImageInfo &image) const override;
	virtual core::SurfaceInfo getSurfaceOptions(core::SurfaceInfo &&) const override;

	virtual core::SwapchainConfig selectConfig(const core::SurfaceInfo &, bool fastMode) override;

	virtual void acquireFrameData(NotNull<core::PresentationFrame>,
			Function<void(NotNull<core::PresentationFrame>)> &&) override;

	virtual void handleFramePresented(NotNull<core::PresentationFrame>) override;

	virtual Rc<core::Surface> makeSurface(NotNull<core::Instance>) override;
	virtual core::FrameConstraints exportFrameConstraints() const override;

	virtual void setFrameOrder(uint64_t) override;

	virtual void propagateInputEvent(InputEventData &); // from app thread
	virtual void propagateTextInput(TextInputState &); // from app thread

	Rc<Context> _context;
	Rc<AppThread> _application;
	Rc<Director> _director;
	NativeWindow *_window = nullptr;
	Rc<core::PresentationEngine> _presentationEngine;

	bool _inBackground = false;
	bool _hasFocus = true;
	bool _pointerInWindow = false;
	bool _isFullscreen = false;

	uint32_t _exitGuard = 0;
	bool _insetDecorationVisible = true;
	float _insetDecorationTone = true;

	core::SwapchainConfig _appSwapchainConfig;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLBASICWINDOW_H_ */
