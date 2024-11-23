/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_BACKEND_VK_XLVKLOOP_H_
#define XENOLITH_BACKEND_VK_XLVKLOOP_H_

#include "XLVk.h"
#include "XLVkInstance.h"
#include "XLVkSync.h"
#include "XLCoreLoop.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class Device;
class View;

class SP_PUBLIC Loop : public core::Loop {
public:
	struct Timer;
	struct Internal;

	virtual ~Loop();

	Loop();

	bool init(Rc<Instance> &&, LoopInfo &&);

	virtual void threadInit() override;
	virtual void threadDispose() override;
	virtual bool worker() override;

	virtual bool isRunning() const override { return _running.load(); }

	virtual void compileResource(Rc<core::Resource> &&req, Function<void(bool)> && = nullptr, bool preload = false) const override;
	virtual void compileQueue(const Rc<Queue> &req, Function<void(bool)> && = nullptr) const override;

	virtual void compileMaterials(Rc<core::MaterialInputData> &&req, const Vector<Rc<DependencyEvent>> & = Vector<Rc<DependencyEvent>>()) const override;
	virtual void compileImage(const Rc<core::DynamicImage> &, Function<void(bool)> && = nullptr) const override;

	// run frame with RenderQueue
	virtual void runRenderQueue(Rc<FrameRequest> &&req, uint64_t gen = 0, Function<void(bool)> && = nullptr) override;

	// callback should return true to end spinning
	virtual void schedule(Function<bool(core::Loop &)> &&, StringView) override;
	virtual void schedule(Function<bool(core::Loop &)> &&, uint64_t, StringView) override;

	virtual void performInQueue(Rc<thread::Task> &&) const override;
	virtual void performInQueue(Function<void()> &&func, Ref *target = nullptr) const override;

	virtual void performOnGlThread(Function<void()> &&func, Ref *target = nullptr, bool immediate = false) const override;

	virtual Rc<FrameHandle> makeFrame(Rc<FrameRequest> &&, uint64_t gen) override;

	virtual Rc<core::Framebuffer> acquireFramebuffer(const PassData *, SpanView<Rc<core::ImageView>>) override;
	virtual void releaseFramebuffer(Rc<core::Framebuffer> &&) override;

	virtual Rc<ImageStorage> acquireImage(const ImageAttachment *, const AttachmentHandle *, const core::ImageInfoData &) override;
	virtual void releaseImage(Rc<ImageStorage> &&) override;

	virtual Rc<core::Semaphore> makeSemaphore() override;

	virtual const Vector<core::ImageFormat> &getSupportedDepthStencilFormat() const override;

	Rc<Fence> acquireFence(uint64_t, bool init = true);

	virtual void signalDependencies(const Vector<Rc<DependencyEvent>> &, Queue *, bool success) override;
	virtual void waitForDependencies(const Vector<Rc<DependencyEvent>> &, Function<void(bool)> &&) override;

	virtual void wakeup() override;
	virtual void waitIdle() override;

	virtual void captureImage(Function<void(const ImageInfoData &info, BytesView view)> &&cb,
			const Rc<core::ImageObject> &image, core::AttachmentLayout l) override;

	virtual void captureBuffer(Function<void(const BufferInfo &info, BytesView view)> &&cb, const Rc<core::BufferObject> &) override;

protected:
	using core::Loop::init;

	Internal *_internal = nullptr;

	Rc<Instance> _vkInstance;

	uint64_t _clock = 0;
};

}

#endif /* XENOLITH_BACKEND_VK_XLVKLOOP_H_ */
