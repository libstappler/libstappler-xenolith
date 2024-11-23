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

#ifndef XENOLITH_SCENE_DIRECTOR_XLVIEW_H_
#define XENOLITH_SCENE_DIRECTOR_XLVIEW_H_

#include "XLEventHeader.h"
#include "XLCoreFrameEmitter.h"
#include "XLCoreLoop.h"
#include "XLPlatformViewInterface.h"
#include "XLInput.h"
#include "XLDirector.h"
#include "SPThread.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class View;
class Director;

struct SP_PUBLIC ViewInfo {
	String title;
	String bundleId;
	URect rect = URect(0, 0, 1024, 768);
	Padding decoration;
	uint64_t frameInterval = 0; // in microseconds ( 1'000'000 / 60 for 60 fps)
	float density = 0.0f;
	Function<core::SwapchainConfig (View &, const core::SurfaceInfo &)> selectConfig;
	Function<void(View &, const core::FrameContraints &)> onCreated;
	Function<void(View &)> onClosed;
};

class SP_PUBLIC View : public thread::Thread, public TextInputViewInterface, public platform::ViewInterface {
public:
	static constexpr size_t FrameAverageCount = 20;

	using AttachmentLayout = core::AttachmentLayout;
	using ImageStorage = core::ImageStorage;
	using FrameEmitter = core::FrameEmitter;
	using FrameRequest = core::FrameRequest;
	using RenderQueue = core::Queue;

	static EventHeader onFrameRate;
	static EventHeader onBackground;
	static EventHeader onFocus;

	View();
	virtual ~View();

	virtual bool init(Application &, ViewInfo &&);

	virtual void runWithQueue(const Rc<core::Queue> &) = 0;
	virtual void end() override;

	virtual void update(bool displayLink) override;

	void performOnThread(Function<void()> &&func, Ref *target = nullptr, bool immediate = false);

	// true - if presentation request accepted, false otherwise,
	// frame should not mark image as detached if false is returned
	virtual bool present(Rc<ImageStorage> &&) = 0;

	// present image in place instead of scheduling presentation
	// should be called in view's thread
	virtual bool presentImmediate(Rc<ImageStorage> &&, Function<void(bool)> &&) = 0;

	// invalidate swapchain image target, if drawing process was not successful
	virtual void invalidateTarget(Rc<ImageStorage> &&) = 0;

	virtual Rc<Ref> getSwapchainHandle() const = 0;

	virtual void captureImage(StringView, const Rc<core::ImageObject> &image, AttachmentLayout l) const = 0;
	virtual void captureImage(Function<void(const core::ImageInfoData &info, BytesView view)> &&,
			const Rc<core::ImageObject> &image, AttachmentLayout l) const = 0;

	const Rc<Director> &getDirector() const;
	const Rc<Application> &getMainLoop() const { return _mainLoop; }
	const Rc<core::Loop> &getGlLoop() const { return _glLoop; }

	// update screen extent, non thread-safe
	// only updates field, view is not resized

	// handle and propagate input event
	virtual void handleInputEvent(const InputEventData &) override;
	virtual void handleInputEvents(Vector<InputEventData> &&) override;

	virtual core::ImageInfo getSwapchainImageInfo() const;
	virtual core::ImageInfo getSwapchainImageInfo(const core::SwapchainConfig &cfg) const;
	virtual core::ImageViewInfo getSwapchainImageViewInfo(const core::ImageInfo &image) const;

	// interval between two frame presented
	uint64_t getLastFrameInterval() const;
	uint64_t getAvgFrameInterval() const;

	// time between frame stared and last queue submission completed
	uint64_t getLastFrameTime() const;
	uint64_t getAvgFrameTime() const;

	uint64_t getAvgFenceTime() const;

	const core::FrameContraints & getFrameContraints() const { return _constraints; }
	virtual Extent2 getExtent() const override;

	bool hasFocus() const { return _hasFocus; }
	bool isInBackground() const { return _inBackground; }
	bool isPointerWithinWindow() const { return _pointerInWindow; }

	uint64_t getFrameInterval() const;
	void setFrameInterval(uint64_t);

	virtual void setReadyForNextFrame() override;

	virtual void setRenderOnDemand(bool value);
	virtual bool isRenderOnDemand() const;

	virtual void retainBackButton();
	virtual void releaseBackButton();
	virtual uint64_t getBackButtonCounter() const override;

	virtual void setDecorationTone(float); // 0.0 - 1.0
	virtual void setDecorationVisible(bool);

	virtual uint64_t retainView() override;
	virtual void releaseView(uint64_t) override;

	virtual void setContentPadding(const Padding &) override;

protected:
	virtual void wakeup(std::unique_lock<Mutex> &) = 0;

	core::FrameContraints _constraints;

	bool _inBackground = false;
	bool _hasFocus = true;
	bool _pointerInWindow = false;
	bool _threadStarted = false;
	bool _navigationEmpty = true;

	std::atomic<bool> _init = false;
	std::atomic<bool> _running = false;

	Rc<Director> _director;
	Rc<Application> _mainLoop;
	Rc<core::Loop> _glLoop;
	Rc<core::FrameEmitter> _frameEmitter;

	ViewInfo _info;

	uint64_t _gen = 1;
	core::SwapchainConfig _config;

	Mutex _mutex;
	Vector<Pair<Function<void()>, Rc<Ref>>> _callbacks;

	mutable Mutex _frameIntervalMutex;
	uint64_t _lastFrameStart = 0;
	std::atomic<uint64_t> _lastFrameInterval = 0;
	math::MovingAverage<FrameAverageCount, uint64_t> _avgFrameInterval;
	std::atomic<uint64_t> _avgFrameIntervalValue = 0;
	uint64_t _backButtonCounter = 0;
};

}

#endif /* XENOLITH_SCENE_DIRECTOR_XLVIEW_H_ */
