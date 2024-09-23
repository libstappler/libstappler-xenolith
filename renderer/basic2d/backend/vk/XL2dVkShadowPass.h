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

#ifndef XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKSHADOWPASS_H_
#define XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKSHADOWPASS_H_

#include "XL2dVkVertexPass.h"
#include "XL2dVkShadow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

class SP_PUBLIC ShadowPass : public VertexPass {
public:
	static constexpr StringView ShadowPipeline = "ShadowPipeline";
	static constexpr StringView PseudoSdfPipeline = "PseudoSdfPipeline";
	static constexpr StringView PseudoSdfSolidPipeline = "PseudoSdfSolidPipeline";
	static constexpr StringView PseudoSdfBackreadPipeline = "PseudoSdfBackreadPipeline";

	enum class Flags {
		None = 0,
		UsePseudoSdf = 1 << 0,
	};

	struct RenderQueueInfo {
		Application *target = nullptr;
		Extent2 extent;
		Flags flags = Flags::None;
	};

	struct PassCreateInfo {
		Application *target = nullptr;
		Extent2 extent;
		Flags flags;

		const AttachmentData *shadowSdfAttachment = nullptr;
		const AttachmentData *lightsAttachment = nullptr;
	};

	static bool makeRenderQueue(Queue::Builder &, RenderQueueInfo &);

	virtual ~ShadowPass() { }

	virtual bool init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder, const PassCreateInfo &info);

	const AttachmentData *getLightsData() const { return _lightsData; }
	const AttachmentData *getSdf() const { return _sdf; }

	Flags getFlags() const { return _flags; }

	virtual Rc<QueuePassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	Flags _flags = Flags::None;

	virtual bool initAsPseudoSdf(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder, const PassCreateInfo &info);

	void makeMaterialSubpass(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder, core::SubpassBuilder &subpassBuilder,
			const core::PipelineLayoutData *layout2d, ResourceCache *cache,
			const core::AttachmentPassData *colorAttachment, const core::AttachmentPassData *shadowAttachment,
			const core::AttachmentPassData *depth2dAttachment);

	// shadows
	const AttachmentData *_lightsData = nullptr;
	const AttachmentData *_sdf = nullptr;
};

SP_DEFINE_ENUM_AS_MASK(ShadowPass::Flags)

class SP_PUBLIC ShadowPassHandle : public VertexPassHandle {
public:
	virtual ~ShadowPassHandle() { }

	virtual bool prepare(FrameQueue &q, Function<void(bool)> &&cb) override;

protected:
	virtual void prepareRenderPass(CommandBuffer &) override;
	virtual void prepareMaterialCommands(core::MaterialSet * materials, CommandBuffer &buf) override;

	// shadows
	const ShadowLightDataAttachmentHandle *_shadowData = nullptr;
	const ImageAttachmentHandle *_sdfImage = nullptr;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKSHADOWPASS_H_ */
