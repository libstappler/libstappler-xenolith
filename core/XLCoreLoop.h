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

#ifndef XENOLITH_CORE_XLCORELOOP_H_
#define XENOLITH_CORE_XLCORELOOP_H_

#include "XLCoreQueueData.h"
#include "XLCoreAttachment.h"
#include "XLCoreInstance.h"
#include "XLCoreMaterial.h"

#include "SPThread.h"
#include "SPThreadTaskQueue.h"

namespace stappler::xenolith::core {

class Loop;
class Device;

struct LoopInfo {
	uint32_t deviceIdx = core::Instance::DefaultDevice;
	uint32_t threadsCount = 2;
	Function<void(const Loop &, const Device &)> onDeviceStarted;
	Function<void(const Loop &, const Device &)> onDeviceFinalized;
	Rc<Ref> platformData;
};

class Loop : public thread::ThreadInterface<Interface> {
public:
	using FrameCache = core::FrameCache;
	using FrameRequest = core::FrameRequest;
	using ImageStorage = core::ImageStorage;
	using Queue = core::Queue;
	using FrameHandle = core::FrameHandle;
	using PassData = core::QueuePassData;
	using ImageAttachment = core::ImageAttachment;
	using AttachmentHandle = core::AttachmentHandle;
	using DependencyEvent = core::DependencyEvent;
	using LoopInfo = core::LoopInfo;

	static constexpr uint32_t LoopThreadId = 2;

	virtual ~Loop();

	Loop();

	virtual bool init(Instance *gl, LoopInfo &&);
	virtual void waitRinning() = 0;
	virtual void cancel() = 0;

	virtual bool isRunning() const = 0;

	const Rc<Instance> &getGlInstance() const { return _glInstance; }
	const Rc<FrameCache> &getFrameCache() const { return _frameCache; }

	// in preload mode, resource will be prepared for transfer immediately in caller's thread
	// (device will allocate transfer buffer then fill it with resource data)
	// do not use reload with main thread
	virtual void compileResource(Rc<Resource> &&req, Function<void(bool)> &&, bool preload = false) const = 0;
	virtual void compileQueue(const Rc<Queue> &req, Function<void(bool)> && = nullptr) const = 0;

	virtual void compileMaterials(Rc<MaterialInputData> &&req, const Vector<Rc<DependencyEvent>> & = Vector<Rc<DependencyEvent>>()) const = 0;
	virtual void compileImage(const Rc<DynamicImage> &, Function<void(bool)> && = nullptr) const = 0;

	// run frame with RenderQueue
	virtual void runRenderQueue(Rc<FrameRequest> &&req, uint64_t gen = 0, Function<void(bool)> && = nullptr) = 0;

	// callback should return true to end spinning
	virtual void schedule(Function<bool(Loop &)> &&, StringView) = 0;
	virtual void schedule(Function<bool(Loop &)> &&, uint64_t, StringView) = 0;

	virtual void performInQueue(Rc<thread::Task> &&) const = 0;
	virtual void performInQueue(Function<void()> &&func, Ref *target = nullptr) const = 0;

	virtual void performOnGlThread(Function<void()> &&func, Ref *target = nullptr, bool immediate = false) const = 0;

	virtual bool isOnGlThread() const = 0;

	virtual Rc<FrameHandle> makeFrame(Rc<FrameRequest> &&, uint64_t gen) = 0;

	virtual Rc<Framebuffer> acquireFramebuffer(const PassData *, SpanView<Rc<ImageView>>) = 0;
	virtual void releaseFramebuffer(Rc<Framebuffer> &&) = 0;

	virtual Rc<ImageStorage> acquireImage(const ImageAttachment *, const AttachmentHandle *, const ImageInfoData &) = 0;
	virtual void releaseImage(Rc<ImageStorage> &&) = 0;

	virtual Rc<Semaphore> makeSemaphore() = 0;

	virtual const Vector<ImageFormat> &getSupportedDepthStencilFormat() const = 0;

	virtual void signalDependencies(const Vector<Rc<DependencyEvent>> &, Queue *, bool success) = 0;
	virtual void waitForDependencies(const Vector<Rc<DependencyEvent>> &, Function<void(bool)> &&) = 0;

	virtual void wakeup() = 0;
	virtual void waitIdle() = 0;

	virtual void captureImage(Function<void(const ImageInfoData &info, BytesView view)> &&cb, const Rc<ImageObject> &image, AttachmentLayout l) = 0;

protected:
#if SP_REF_DEBUG
	virtual bool isRetainTrackerEnabled() const override {
		return true;
	}
#endif

	std::atomic_flag _shouldExit;
	Rc<Instance> _glInstance;
	Rc<FrameCache> _frameCache;
	LoopInfo _info;
};

}

#endif /* XENOLITH_CORE_XLCORELOOP_H_ */
