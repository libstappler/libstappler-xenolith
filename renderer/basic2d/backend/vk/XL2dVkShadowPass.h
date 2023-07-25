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

namespace stappler::xenolith::basic2d::vk {

class ShadowPass : public VertexPass {
public:
	static constexpr StringView ShadowPipeline = "ShadowPipeline";

	enum class Flags {
		None = 0,
		Render3D = 1 << 0,
	};

	struct RenderQueueInfo {
		MainLoop *target = nullptr;
		Extent2 extent;
		Flags flags = Flags::None;
		Function<void(core::Resource::Builder &)> resourceCallback;
	};

	struct PassCreateInfo {
		MainLoop *target = nullptr;
		Extent2 extent;
		Flags flags;

		const AttachmentData *shadowSdfAttachment = nullptr;
		const AttachmentData *lightsAttachment = nullptr;
		const AttachmentData *sdfPrimitivesAttachment = nullptr;
	};

	static bool makeDefaultRenderQueue(Queue::Builder &, RenderQueueInfo &);

	virtual ~ShadowPass() { }

	virtual bool init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder, const PassCreateInfo &info);

	const AttachmentData *getLightsData() const { return _lightsData; }
	const AttachmentData *getShadowPrimitives() const { return _shadowPrimitives; }
	const AttachmentData *getSdf() const { return _sdf; }

	virtual Rc<QueuePassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	Flags _flags = Flags::None;

	// shadows
	const AttachmentData *_lightsData = nullptr;
	const AttachmentData *_shadowPrimitives = nullptr;
	const AttachmentData *_sdf = nullptr;
};

SP_DEFINE_ENUM_AS_MASK(ShadowPass::Flags)

class ShadowPassHandle : public VertexPassHandle {
public:
	virtual ~ShadowPassHandle() { }

	virtual bool prepare(FrameQueue &q, Function<void(bool)> &&cb) override;

protected:
	virtual void prepareRenderPass(CommandBuffer &) override;
	virtual void prepareMaterialCommands(core::MaterialSet * materials, CommandBuffer &buf) override;

	// shadows
	const ShadowLightDataAttachmentHandle *_shadowData = nullptr;
	const ShadowPrimitivesAttachmentHandle *_shadowPrimitives = nullptr;
	const ImageAttachmentHandle *_sdfImage = nullptr;
};

class ComputeShadowPass : public QueuePass {
public:
	static constexpr StringView SdfTrianglesComp = "SdfTrianglesComp";
	static constexpr StringView SdfCirclesComp = "SdfCirclesComp";
	static constexpr StringView SdfRectsComp = "SdfRectsComp";
	static constexpr StringView SdfRoundedRectsComp = "SdfRoundedRectsComp";
	static constexpr StringView SdfPolygonsComp = "SdfPolygonsComp";
	static constexpr StringView SdfImageComp = "SdfImageComp";

	virtual ~ComputeShadowPass() { }

	virtual bool init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder, Extent2 defaultExtent);

	const AttachmentData *getLights() const { return _lights; }
	const AttachmentData *getVertexes() const { return _vertexes; }
	const AttachmentData *getPrimitives() const { return _primitives; }
	const AttachmentData *getSdf() const { return _sdf; }

	virtual Rc<QueuePassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	const AttachmentData *_lights = nullptr;
	const AttachmentData *_vertexes = nullptr;
	const AttachmentData *_primitives = nullptr;
	const AttachmentData *_sdf = nullptr;
};

class ComputeShadowPassHandle : public QueuePassHandle {
public:
	virtual ~ComputeShadowPassHandle() { }

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual void writeShadowCommands(RenderPass *, CommandBuffer &);
	virtual Vector<const CommandBuffer *> doPrepareCommands(FrameHandle &) override;

	const ShadowLightDataAttachmentHandle *_lightsBuffer = nullptr;
	const ShadowVertexAttachmentHandle *_vertexBuffer = nullptr;
	const ShadowPrimitivesAttachmentHandle *_primitivesBuffer = nullptr;
	const ShadowSdfImageAttachmentHandle *_sdfImage = nullptr;

	uint32_t _gridCellSize = 64;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKSHADOWPASS_H_ */
