/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_CORE_XLCOREPRESENTATIONFRAME_H_
#define XENOLITH_CORE_XLCOREPRESENTATIONFRAME_H_

#include "XLCoreInfo.h"
#include "XLCoreSwapchain.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class FrameRequest;
class FrameHandle;
class ImageStorage;
class PresentationEngine;

class Object;
class ImageObject;
class ImageView;
class Semaphore;
class Swapchain;

struct AttachmentData;

struct SwapchainImageData {
	Rc<ImageObject> image;
	Map<ImageViewInfo, Rc<ImageView>> views;
};

struct SwapchainAcquiredImage : public Ref {
	uint32_t imageIndex;
	const SwapchainImageData *data;
	Rc<Semaphore> sem;
	Rc<Swapchain> swapchain;

	SwapchainAcquiredImage(uint32_t idx, const SwapchainImageData *d, Rc<Semaphore> &&s, Rc<Swapchain> &&sw)
	: imageIndex(idx), data(d), sem(move(s)), swapchain(move(sw)) { }
};

class PresentationFrame : public Ref {
public:
	enum Flags {
		None,
		OffscreenTarget = 1 << 0,
		SwapchainImageAcquired = 1 << 1,

		// stage flags
		ImageAcquired = 1 << 2,
		InputAcquired = 1 << 3,
		FrameSubmitted = 1 << 4,
		QueueSubmitted = 1 << 5,
		ImageRendered = 1 << 6,
		ImagePresented = 1 << 7,
		Invalidated = 1 << 8,

		// Frame scheduled in context, that allows presentation interval correction
		// On-demand frame can not be CorrectableFrame
		CorrectableFrame = 1 << 9,
	};

	virtual ~PresentationFrame() = default;

	bool init(PresentationEngine *, FrameConstraints,
			uint64_t frameOrder, Flags flags,
			Function<void(PresentationFrame *, bool)> &&completeCallback = nullptr);

	bool hasFlag(Flags f) const { return sp::hasFlag(_flags, f); }

	bool isValid() const;

	ImageStorage *getTarget() const { return _target; }
	FrameRequest *getRequest() const { return _frameRequest; }
	FrameHandle *getHandle() const { return _frameHandle; }
	Swapchain *getSwapchain() const { return _swapchain; }

	uint64_t getFrameOrder() const { return _frameOrder; }

	SwapchainImage *getSwapchainImage() const;

	AttachmentData *setupOutputAttachment();

	FrameHandle *submitFrame();

	bool assignSwapchainImage(Swapchain::SwapchainAcquiredImage *);
	bool assignResult(ImageStorage *);

	void setSubmitted();
	void setPresented();

	void invalidate(bool invalidateSwapchain = false);

	void cancelFrameHandle();

protected:
	uint64_t _frameOrder = 0;
	bool _active = true;
	Flags _flags = None;
	Rc<ImageStorage> _target;
	Rc<FrameRequest> _frameRequest;
	Rc<FrameHandle> _frameHandle;
	Rc<Swapchain> _swapchain;
	Rc<PresentationEngine> _engine;
	Function<void(PresentationFrame *, bool)> _completeCallback;
};

SP_DEFINE_ENUM_AS_MASK(PresentationFrame::Flags)

}

#endif /* XENOLITH_CORE_XLCOREPRESENTATIONFRAME_H_ */
