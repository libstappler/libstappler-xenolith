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

#ifndef XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKVERTEXPASS_H_
#define XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKVERTEXPASS_H_

#include "XL2dVkMaterial.h"
#include "XL2dCommandList.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

class SP_PUBLIC VertexAttachment : public BufferAttachment {
public:
	virtual ~VertexAttachment();

	virtual bool init(AttachmentBuilder &builder, const BufferInfo &, const AttachmentData *);

	const AttachmentData *getMaterials() const { return _materials; }

protected:
	using BufferAttachment::init;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;

	const AttachmentData *_materials = nullptr;
};

class SP_PUBLIC VertexAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~VertexAttachmentHandle();

	virtual bool setup(FrameQueue &, Function<void(bool)> &&) override;

	virtual void submitInput(FrameQueue &, Rc<core::AttachmentInputData> &&, Function<void(bool)> &&) override;

	const Vector<VertexSpan> &getVertexData() const { return _spans; }
	const Vector<VertexSpan> &getShadowSolidData() const { return _shadowSolidSpans; }
	const Vector<VertexSpan> &getShadowSdfData() const { return _shadowSdfSpans; }
	const Rc<Buffer> &getVertexes() const { return _vertexes; }
	const Rc<Buffer> &getIndexes() const { return _indexes; }

	float getMaxShadowValue() const { return _maxShadowValue; }

	const core::MaterialSet *getMaterialSet() const { return _materialSet; }

	const Rc<FrameContextHandle2d> &getCommands() const;

	bool empty() const;

	void loadData(Rc<FrameContextHandle2d> &&data,
			Rc<Buffer> && indexes, Rc<Buffer> &&vertexes, Rc<Buffer> &&transforms,
			Vector<VertexSpan> &&spans, Vector<VertexSpan> &&shadowSolidSpans, Vector<VertexSpan> &&shadowSdfSpans,
			float maxShadowValue);

protected:
	virtual bool isGpuTransform() const { return false; }

	Rc<FrameContextHandle2d> _commands;
	Rc<Buffer> _indexes;
	Rc<Buffer> _vertexes;
	Rc<Buffer> _transforms;
	Vector<VertexSpan> _spans;
	Vector<VertexSpan> _shadowSolidSpans;
	Vector<VertexSpan> _shadowSdfSpans;

	Rc<core::MaterialSet> _materialSet;
	const MaterialAttachmentHandle *_materials = nullptr;
	float _maxShadowValue = 0.0f;
};

class SP_PUBLIC VertexPass : public QueuePass {
public:
	using AttachmentHandle = core::AttachmentHandle;

	static core::ImageFormat selectDepthFormat(SpanView<core::ImageFormat> formats);

	virtual ~VertexPass() { }

	const AttachmentData *getVertexes() const { return _vertexes; }
	const AttachmentData *getMaterials() const { return _materials; }

	virtual Rc<QueuePassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	const AttachmentData *_output = nullptr;
	const AttachmentData *_shadow = nullptr;
	const AttachmentData *_depth2d = nullptr;
	const AttachmentData *_depthSdf = nullptr;

	const AttachmentData *_vertexes = nullptr;
	const AttachmentData *_materials = nullptr;
};

class SP_PUBLIC VertexPassHandle : public QueuePassHandle {
public:
	virtual ~VertexPassHandle() { }

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual Vector<const core::CommandBuffer *> doPrepareCommands(FrameHandle &) override;

	virtual void prepareRenderPass(CommandBuffer &);
	virtual void prepareMaterialCommands(core::MaterialSet * materials, CommandBuffer &);
	virtual void finalizeRenderPass(CommandBuffer &);

	void clearDynamicState(CommandBuffer &buf);
	void applyDynamicState(const FrameContextHandle2d *commands, CommandBuffer &buf, uint32_t stateId);

	const VertexAttachmentHandle *_vertexBuffer = nullptr;
	const MaterialAttachmentHandle *_materialBuffer = nullptr;

	StateId _dynamicStateId = maxOf<StateId>();
	DrawStateValues _dynamicState;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKVERTEXPASS_H_ */
