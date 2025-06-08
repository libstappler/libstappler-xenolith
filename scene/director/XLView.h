/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_SCENE_DIRECTOR_XLVIEW_H_
#define XENOLITH_SCENE_DIRECTOR_XLVIEW_H_

#include "XLCoreLoop.h"
#include "XLCorePresentationEngine.h"
#include "XLPlatformViewInterface.h"
#include "XLInput.h"
#include "XLDirector.h"
#include "XLEvent.h"
#include "SPThread.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class View;
class Director;

using WindowInfo = platform::WindowInfo;

struct SP_PUBLIC ViewInfo {
	WindowInfo window;
	Function<core::SwapchainConfig(const View &, const core::SurfaceInfo &)> selectConfig;
	Function<void(View &, const core::FrameConstraints &)> onCreated;
	Function<void(View &)> onClosed;

	core::FrameConstraints exportConstraints() {
		return core::FrameConstraints{.extent = Extent2(window.rect.width, window.rect.height),
			.contentPadding = window.decoration,
			.transform = core::SurfaceTransformFlags::Identity,
			.density = window.density};
	}
};

class SP_PUBLIC View : public platform::ViewInterface {
public:
	static constexpr size_t FrameAverageCount = 20;

	using AttachmentLayout = core::AttachmentLayout;
	using ImageStorage = core::ImageStorage;
	using FrameEmitter = core::FrameEmitter;
	using FrameRequest = core::FrameRequest;
	using RenderQueue = core::Queue;

	static EventHeader onFrameRate;

	virtual ~View();

	virtual bool init(Application &, ViewInfo &&);

	virtual void runWithQueue(const Rc<core::Queue> &);

	virtual void run();

	virtual void end() override;

	void setPresentationEngine(Rc<core::PresentationEngine> &&);

	virtual void captureImage(const FileInfo &, const Rc<core::ImageObject> &image,
			AttachmentLayout l) const = 0;
	virtual void captureImage(Function<void(const core::ImageInfoData &info, BytesView view)> &&,
			const Rc<core::ImageObject> &image, AttachmentLayout l) const = 0;

	Director *getDirector() const;
	core::PresentationEngine *getPresentationEngine() const { return _presentationEngine; }

	virtual core::ImageInfo getSwapchainImageInfo(const core::SwapchainConfig &cfg) const;
	virtual core::ImageViewInfo getSwapchainImageViewInfo(const core::ImageInfo &image) const;

	virtual Extent2 getExtent() const override;

	virtual const WindowInfo &getWindowInfo() const override;

	core::SwapchainConfig selectConfig(const core::SurfaceInfo &) const;

	bool hasFocus() const { return _hasFocus; }
	bool isInBackground() const { return _inBackground; }
	bool isPointerWithinWindow() const { return _pointerInWindow; }

	virtual void retainBackButton();
	virtual void releaseBackButton();
	virtual uint64_t getBackButtonCounter() const override;

	virtual void setDecorationTone(float); // 0.0 - 1.0
	virtual void setDecorationVisible(bool);

	virtual void mapWindow() { }

	virtual void deprecateSwapchain();

protected:
	virtual void propagateInputEvent(core::InputEventData &) override;
	virtual void propagateTextInput(TextInputState &) override;

	bool _navigationEmpty = true;

	Rc<Director> _director;

	ViewInfo _info;
	uint64_t _backButtonCounter = 0;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_SCENE_DIRECTOR_XLVIEW_H_ */
