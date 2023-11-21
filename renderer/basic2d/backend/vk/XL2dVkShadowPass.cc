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

#include "XL2dVkShadowPass.h"

#include "XLPlatform.h"
#include "XLApplication.h"
#include "XLVkLoop.h"
#include "XLVkRenderPass.h"
#include "XLVkPipeline.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameCache.h"
#include "XL2dFrameContext.h"
#include "glsl/XL2dShaders.h"

namespace stappler::xenolith::basic2d::vk {

bool ShadowPass::makeDefaultRenderQueue(Queue::Builder &builder, RenderQueueInfo &info) {
	using namespace core;

	Rc<ComputeShadowPass> computePass;

	builder.addPass("MaterialComputeShadowPass", PassType::Compute, RenderOrdering(0), [&] (QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		computePass = Rc<ComputeShadowPass>::create(builder, passBuilder, info.extent);
		return computePass;
	});

	builder.addPass("MaterialSwapchainPass", PassType::Graphics, RenderOrderingHighest, [&] (QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<ShadowPass>::create(builder, passBuilder, PassCreateInfo{
			info.target, info.extent, info.flags,
			computePass->getSdf(), computePass->getLights(), computePass->getPrimitives()
		});
	});

	// define internal resources (images and buffers)
	Resource::Builder resourceBuilder("LoaderResources");
	if (info.resourceCallback) {
		info.resourceCallback(resourceBuilder);
		builder.setInternalResource(Rc<core::Resource>::create(move(resourceBuilder)));
	}

	return true;
}

bool ShadowPass::init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder, const PassCreateInfo &info) {
	using namespace core;

	_output = queueBuilder.addAttachemnt("Output", [&] (AttachmentBuilder &builder) -> Rc<Attachment> {
		// swapchain output
		builder.defineAsOutput();

		return Rc<vk::ImageAttachment>::create(builder,
			ImageInfo(
				info.extent,
				core::ForceImageUsage(core::ImageUsage::ColorAttachment),
				platform::getCommonFormat()),
			core::ImageAttachment::AttachmentInfo{
				.initialLayout = AttachmentLayout::Undefined,
				.finalLayout = AttachmentLayout::PresentSrc,
				.clearOnLoad = true,
				.clearColor = Color4F(1.0f, 1.0f, 1.0f, 1.0f) // Color4F::WHITE;
		});
	});

	_shadow = queueBuilder.addAttachemnt("Shadow", [&] (AttachmentBuilder &builder) -> Rc<Attachment> {
		// swapchain output
		return Rc<vk::ImageAttachment>::create(builder,
			ImageInfo(
				info.extent,
				core::ForceImageUsage(core::ImageUsage::ColorAttachment | core::ImageUsage::InputAttachment),
				core::ImageFormat::R16_SFLOAT),
			core::ImageAttachment::AttachmentInfo{
				.initialLayout = AttachmentLayout::Undefined,
				.finalLayout = AttachmentLayout::ShaderReadOnlyOptimal,
				.clearOnLoad = true,
				.clearColor = Color4F(0.0f, 0.0f, 0.0f, 0.0f) // Color4F::BLACK;
		});
	});

	_depth2d = queueBuilder.addAttachemnt("CommonDepth2d", [&] (AttachmentBuilder &builder) -> Rc<Attachment> {
		// swapchain output
		return Rc<vk::ImageAttachment>::create(builder,
			ImageInfo(
				info.extent,
				core::ForceImageUsage(core::ImageUsage::DepthStencilAttachment),
				VertexPass::selectDepthFormat(info.target->getGlLoop()->getSupportedDepthStencilFormat())),
			core::ImageAttachment::AttachmentInfo{
				.initialLayout = AttachmentLayout::Undefined,
				.finalLayout = AttachmentLayout::DepthStencilAttachmentOptimal,
				.clearOnLoad = true,
				.clearColor = Color4F::WHITE
		});
	});

	_sdf = info.shadowSdfAttachment;

	_materials = queueBuilder.addAttachemnt(FrameContext2d::MaterialAttachmentName, [] (AttachmentBuilder &builder) -> Rc<Attachment> {
		return Rc<vk::MaterialAttachment>::create(builder, BufferInfo(core::BufferUsage::StorageBuffer));
	});

	_vertexes = queueBuilder.addAttachemnt(FrameContext2d::VertexAttachmentName, [&] (AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<VertexAttachment>::create(builder, BufferInfo(core::BufferUsage::StorageBuffer), _materials);
	});

	_lightsData = info.lightsAttachment;
	_shadowPrimitives = info.sdfPrimitivesAttachment;

	auto colorAttachment = passBuilder.addAttachment(_output);
	auto shadowAttachment = passBuilder.addAttachment(_shadow);
	auto sdfAttachment = passBuilder.addAttachment(_sdf);
	auto depth2dAttachment = passBuilder.addAttachment(_depth2d);

	auto layout2d = passBuilder.addDescriptorLayout([&] (PipelineLayoutBuilder &layoutBuilder) {
		// Vertex input attachment - per-frame vertex list
		layoutBuilder.addSet([&] (DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(passBuilder.addAttachment(_vertexes));
			setBuilder.addDescriptor(passBuilder.addAttachment(_materials));
			setBuilder.addDescriptor(passBuilder.addAttachment(_lightsData));
			setBuilder.addDescriptor(passBuilder.addAttachment(_shadowPrimitives));
			setBuilder.addDescriptor(shadowAttachment, DescriptorType::InputAttachment, AttachmentLayout::ShaderReadOnlyOptimal);
			setBuilder.addDescriptor(sdfAttachment, DescriptorType::SampledImage, AttachmentLayout::ShaderReadOnlyOptimal);
		});
	});

	auto subpass2d = passBuilder.addSubpass([&] (SubpassBuilder &subpassBuilder) {
		// load shaders by ref - do not copy data into engine
		auto materialVert = queueBuilder.addProgramByRef("Loader_MaterialVert", shaders::MaterialVert);
		auto materialFrag = queueBuilder.addProgramByRef("Loader_MaterialFrag", shaders::MaterialFrag);

		auto shaderSpecInfo = Vector<SpecializationInfo>({
			// no specialization required for vertex shader
			core::SpecializationInfo(materialVert, Vector<PredefinedConstant>{
				PredefinedConstant::BuffersArraySize
			}),
			// specialization for fragment shader - use platform-dependent array sizes
			core::SpecializationInfo(materialFrag, Vector<PredefinedConstant>{
				PredefinedConstant::SamplersArraySize,
				PredefinedConstant::TexturesArraySize
			})
		});

		// pipelines for material-besed rendering
		auto materialPipeline = subpassBuilder.addGraphicPipeline("Solid", layout2d, shaderSpecInfo, PipelineMaterialInfo({
			BlendInfo(),
			DepthInfo(true, true, CompareOp::Less)
		}));

		auto transparentPipeline = subpassBuilder.addGraphicPipeline("Transparent", layout2d, shaderSpecInfo, PipelineMaterialInfo({
			BlendInfo(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha, BlendOp::Add,
					BlendFactor::Zero, BlendFactor::One, BlendOp::Add),
			DepthInfo(false, true, CompareOp::LessOrEqual)
		}));

		// pipeline for debugging - draw lines instead of triangles
		subpassBuilder.addGraphicPipeline("DebugTriangles", layout2d, shaderSpecInfo, PipelineMaterialInfo(
			BlendInfo(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha, BlendOp::Add,
					BlendFactor::Zero, BlendFactor::One, BlendOp::Add),
			DepthInfo(false, true, CompareOp::LessOrEqual),
			LineWidth(1.0f)
		));

		auto cache = info.target->getResourceCache();
		static_cast<MaterialAttachment *>(_materials->attachment.get())->addPredefinedMaterials(Vector<Rc<Material>>({
			Rc<Material>::create(Material::MaterialIdInitial, materialPipeline, cache->getEmptyImage(), ColorMode::IntensityChannel),
			Rc<Material>::create(Material::MaterialIdInitial, materialPipeline, cache->getSolidImage(), ColorMode::IntensityChannel),
			Rc<Material>::create(Material::MaterialIdInitial, transparentPipeline, cache->getEmptyImage(), ColorMode()),
			Rc<Material>::create(Material::MaterialIdInitial, transparentPipeline, cache->getSolidImage(), ColorMode()),
		}));

		subpassBuilder.addColor(colorAttachment, AttachmentDependencyInfo{
			PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
			PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
			FrameRenderPassState::Submitted,
		});

		subpassBuilder.addColor(shadowAttachment, AttachmentDependencyInfo{
			PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
			PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
			FrameRenderPassState::Submitted,
		});

		subpassBuilder.setDepthStencil(depth2dAttachment, AttachmentDependencyInfo{
			PipelineStage::EarlyFragmentTest, AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,
			PipelineStage::LateFragmentTest, AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,
			FrameRenderPassState::Submitted,
		});
	});

	auto subpassShadows = passBuilder.addSubpass([&] (SubpassBuilder &subpassBuilder) {
		subpassBuilder.addColor(colorAttachment, AttachmentDependencyInfo{
			PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
			PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
			FrameRenderPassState::Submitted,
		});

		subpassBuilder.addInput(shadowAttachment, AttachmentDependencyInfo{ // 4
			PipelineStage::FragmentShader, AccessType::ShaderRead,
			PipelineStage::FragmentShader, AccessType::ShaderRead,
			FrameRenderPassState::Submitted,
		});

		auto shadowVert = queueBuilder.addProgramByRef("ShadowMergeVert", shaders::SdfShadowsVert);
		auto shadowFrag = queueBuilder.addProgramByRef("ShadowMergeFrag", shaders::SdfShadowsFrag);

		subpassBuilder.addGraphicPipeline(ShadowPass::ShadowPipeline, layout2d, Vector<SpecializationInfo>({
			// no specialization required for vertex shader
			shadowVert,
			// specialization for fragment shader - use platform-dependent array sizes
			SpecializationInfo(shadowFrag, Vector<PredefinedConstant>{
				PredefinedConstant::SamplersArraySize,
			})
		}), PipelineMaterialInfo({
			BlendInfo(BlendFactor::Zero, BlendFactor::SrcColor, BlendOp::Add,
					BlendFactor::Zero, BlendFactor::One, BlendOp::Add),
			DepthInfo()
		}));
	});

	passBuilder.addSubpassDependency(subpass2d, PipelineStage::LateFragmentTest, AccessType::DepthStencilAttachmentWrite,
			subpassShadows, PipelineStage::FragmentShader, AccessType::ShaderRead, true);

	if (!VertexPass::init(passBuilder)) {
		return false;
	}

	_flags = info.flags;
	return true;
}

auto ShadowPass::makeFrameHandle(const FrameQueue &handle) -> Rc<QueuePassHandle> {
	return Rc<ShadowPassHandle>::create(*this, handle);
}

bool ShadowPassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = static_cast<ShadowPass *>(_queuePass.get());

	if (auto lightsBuffer = q.getAttachment(pass->getLightsData())) {
		_shadowData = static_cast<const ShadowLightDataAttachmentHandle *>(lightsBuffer->handle.get());
	}

	if (auto shadowPrimitives = q.getAttachment(pass->getShadowPrimitives())) {
		_shadowPrimitives = static_cast<const ShadowPrimitivesAttachmentHandle *>(shadowPrimitives->handle.get());
	}

	if (auto sdfImage = q.getAttachment(pass->getSdf())) {
		_sdfImage = static_cast<const ShadowSdfImageAttachmentHandle *>(sdfImage->handle.get());
	}

	return VertexPassHandle::prepare(q, move(cb));
}

void ShadowPassHandle::prepareRenderPass(CommandBuffer &buf) {
	Vector<BufferMemoryBarrier> bufferBarriers;
	Vector<ImageMemoryBarrier> imageBarriers;
	if (_shadowData->getLightsCount() && _shadowData->getBuffer()) {
		if (auto b = _shadowData->getBuffer()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowPrimitives->getTriangles()) {
		if (auto b = _shadowPrimitives->getTriangles()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowPrimitives->getGridSize()) {
		if (auto b = _shadowPrimitives->getGridSize()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowPrimitives->getGridIndex()) {
		if (auto b = _shadowPrimitives->getGridIndex()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowPrimitives->getCircles()) {
		if (auto b = _shadowPrimitives->getCircles()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowPrimitives->getRects()) {
		if (auto b = _shadowPrimitives->getRects()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowPrimitives->getRoundedRects()) {
		if (auto b = _shadowPrimitives->getRoundedRects()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (_shadowPrimitives->getPolygons()) {
		if (auto b = _shadowPrimitives->getPolygons()->getPendingBarrier()) {
			bufferBarriers.emplace_back(*b);
		}
	}

	if (auto image = _sdfImage->getImage()) {
		if (auto b = image->getImage().cast<vk::Image>()->getPendingBarrier()) {
			imageBarriers.emplace_back(*b);
		}
	}

	if (!imageBarriers.empty() || !bufferBarriers.empty()) {
		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, bufferBarriers, imageBarriers);
	} else if (!imageBarriers.empty()) {
		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, imageBarriers);
	} else if (!bufferBarriers.empty()) {
		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, bufferBarriers);
	}
}

void ShadowPassHandle::prepareMaterialCommands(core::MaterialSet * materials, CommandBuffer &buf) {
	VertexPassHandle::prepareMaterialCommands(materials, buf);

	auto &fb = getFramebuffer();
	auto currentExtent = fb->getExtent();

	auto subpassIdx = buf.cmdNextSubpass();

	if (_shadowData->getLightsCount() && _shadowData->getBuffer() && _shadowData->getObjectsCount()) {
		auto pipeline = static_cast<GraphicPipeline *>(_data->subpasses[subpassIdx]->graphicPipelines
				.get(StringView(ShadowPass::ShadowPipeline))->pipeline.get());

		buf.cmdBindPipeline(pipeline);

		auto viewport = VkViewport{ 0.0f, 0.0f, float(currentExtent.width), float(currentExtent.height), 0.0f, 1.0f };
		buf.cmdSetViewport(0, makeSpanView(&viewport, 1));

		auto scissorRect = VkRect2D{ { 0, 0}, { currentExtent.width, currentExtent.height } };
		buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));

		uint32_t samplerIndex = 1; // linear filtering
		buf.cmdPushConstants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0, BytesView(reinterpret_cast<const uint8_t *>(&samplerIndex), sizeof(uint32_t)));

		buf.cmdDrawIndexed(
			6, // indexCount
			1, // instanceCount
			6, // firstIndex
			0, // int32_t   vertexOffset
			0  // uint32_t  firstInstance
		);
	}
}

bool ComputeShadowPass::init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder, Extent2 defaultExtent) {
	using namespace core;

	_lights = queueBuilder.addAttachemnt(FrameContext2d::LightDataAttachmentName, [] (AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<ShadowLightDataAttachment>::create(builder);
	});

	_vertexes = queueBuilder.addAttachemnt(FrameContext2d::ShadowVertexAttachmentName, [] (AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<ShadowVertexAttachment>::create(builder);
	});

	_primitives = queueBuilder.addAttachemnt("ShadowPrimitivesAttachment", [] (AttachmentBuilder &builder) -> Rc<Attachment> {
		return Rc<ShadowPrimitivesAttachment>::create(builder);
	});

	_sdf = queueBuilder.addAttachemnt(FrameContext2d::SdfImageAttachmentName, [&] (AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		//builder.defineAsOutput();
		return Rc<ShadowSdfImageAttachment>::create(builder, defaultExtent);
	});

	auto layout = passBuilder.addDescriptorLayout([&] (PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&] (DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(passBuilder.addAttachment(_lights));
			setBuilder.addDescriptor(passBuilder.addAttachment(_vertexes));
			setBuilder.addDescriptor(passBuilder.addAttachment(_primitives));
			setBuilder.addDescriptor(passBuilder.addAttachment(_sdf), DescriptorType::StorageImage, AttachmentLayout::General);
		});
	});

	passBuilder.addSubpass([&] (SubpassBuilder &subpassBuilder) {
		subpassBuilder.addComputePipeline(ComputeShadowPass::SdfTrianglesComp, layout,
				queueBuilder.addProgramByRef("ShadowPass_SdfTrianglesComp", shaders::SdfTrianglesComp));

		subpassBuilder.addComputePipeline(ComputeShadowPass::SdfCirclesComp, layout,
				queueBuilder.addProgramByRef("ShadowPass_SdfCirclesComp", shaders::SdfCirclesComp));

		subpassBuilder.addComputePipeline(ComputeShadowPass::SdfRectsComp, layout,
				queueBuilder.addProgramByRef("ShadowPass_SdfRectsComp", shaders::SdfRectsComp));

		subpassBuilder.addComputePipeline(ComputeShadowPass::SdfRoundedRectsComp, layout,
				queueBuilder.addProgramByRef("ShadowPass_SdfRoundedRectsComp", shaders::SdfRoundedRectsComp));

		subpassBuilder.addComputePipeline(ComputeShadowPass::SdfPolygonsComp, layout,
				queueBuilder.addProgramByRef("ShadowPass_SdfPolygonsComp", shaders::SdfPolygonsComp));

		subpassBuilder.addComputePipeline(ComputeShadowPass::SdfImageComp, layout,
				queueBuilder.addProgramByRef("ShadowPass_SdfImageComp", shaders::SdfImageComp));
	});

	return QueuePass::init(passBuilder);
}

auto ComputeShadowPass::makeFrameHandle(const FrameQueue &handle) -> Rc<QueuePassHandle> {
	return Rc<ComputeShadowPassHandle>::create(*this, handle);
}

bool ComputeShadowPassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = static_cast<ComputeShadowPass *>(_queuePass.get());

	ShadowPrimitivesAttachmentHandle *trianglesHandle = nullptr;
	ShadowLightDataAttachmentHandle *lightsHandle = nullptr;

	if (auto lightsBuffer = q.getAttachment(pass->getLights())) {
		_lightsBuffer = lightsHandle = static_cast<ShadowLightDataAttachmentHandle *>(lightsBuffer->handle.get());
	}

	if (auto primitivesBuffer = q.getAttachment(pass->getPrimitives())) {
		_primitivesBuffer = trianglesHandle = static_cast<ShadowPrimitivesAttachmentHandle *>(primitivesBuffer->handle.get());
	}

	if (auto vertexBuffer = q.getAttachment(pass->getVertexes())) {
		_vertexBuffer = static_cast<const ShadowVertexAttachmentHandle *>(vertexBuffer->handle.get());
	}

	if (auto sdfImage = q.getAttachment(pass->getSdf())) {
		_sdfImage = static_cast<const ShadowSdfImageAttachmentHandle *>(sdfImage->handle.get());
	}

	if (lightsHandle && lightsHandle->getLightsCount()) {
		lightsHandle->allocateBuffer(static_cast<DeviceFrameHandle *>(q.getFrame().get()), _vertexBuffer, _gridCellSize);

		if (lightsHandle->getObjectsCount() > 0 && trianglesHandle) {
			trianglesHandle->allocateBuffer(static_cast<DeviceFrameHandle *>(q.getFrame().get()),
					lightsHandle->getObjectsCount(), lightsHandle->getShadowData());
		}

		return QueuePassHandle::prepare(q, move(cb));
	} else {
		cb(true);
		return true;
	}
}

void ComputeShadowPassHandle::writeShadowCommands(RenderPass *pass, CommandBuffer &buf) {
	auto sdfImage = static_cast<Image *>(_sdfImage->getImage()->getImage().get());

	if (!_lightsBuffer || _lightsBuffer->getObjectsCount() == 0) {
		ImageMemoryBarrier inImageBarriers[] = {
			ImageMemoryBarrier(sdfImage, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL)
		};

		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, inImageBarriers);
		buf.cmdClearColorImage(sdfImage, VK_IMAGE_LAYOUT_GENERAL, Color4F(128.0f, 0.0f, 0.0f, 0.0f));

		auto gIdx = _device->getQueueFamily(QueueOperations::Graphics)->index;

		if (_pool->getFamilyIdx() != gIdx) {
			BufferMemoryBarrier transferBufferBarrier(_lightsBuffer->getBuffer(),
				VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
				QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE);

			ImageMemoryBarrier transferImageBarrier(sdfImage,
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx});
			sdfImage->setPendingBarrier(transferImageBarrier);

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
					makeSpanView(&transferBufferBarrier, 1), makeSpanView(&transferImageBarrier, 1));
		} else {
			ImageMemoryBarrier transferImageBarrier(sdfImage,
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			sdfImage->setPendingBarrier(transferImageBarrier);
		}
		return;
	}

	ComputePipeline *pipeline = nullptr;
	buf.cmdBindDescriptorSets(pass, 0);
	buf.cmdFillBuffer(_primitivesBuffer->getGridSize(), 0);

	BufferMemoryBarrier bufferBarrier(_primitivesBuffer->getGridSize(),
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
	);

	buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(&bufferBarrier, 1));

	if (_vertexBuffer->getTrianglesCount()) {
		pipeline = static_cast<ComputePipeline *>(_data->subpasses[0]->computePipelines.get(ComputeShadowPass::SdfTrianglesComp)->pipeline.get());
		buf.cmdBindPipeline(pipeline);
		buf.cmdDispatch((_vertexBuffer->getTrianglesCount() - 1) / pipeline->getLocalX() + 1);
	}

	if (_vertexBuffer->getCirclesCount()) {
		pipeline = static_cast<ComputePipeline *>(_data->subpasses[0]->computePipelines.get(ComputeShadowPass::SdfCirclesComp)->pipeline.get());
		buf.cmdBindPipeline(pipeline);
		buf.cmdDispatch((_vertexBuffer->getCirclesCount() - 1) / pipeline->getLocalX() + 1);
	}

	if (_vertexBuffer->getRectsCount()) {
		pipeline = static_cast<ComputePipeline *>(_data->subpasses[0]->computePipelines.get(ComputeShadowPass::SdfRectsComp)->pipeline.get());
		buf.cmdBindPipeline(pipeline);
		buf.cmdDispatch((_vertexBuffer->getRectsCount() - 1) / pipeline->getLocalX() + 1);
	}

	if (_vertexBuffer->getRoundedRectsCount()) {
		pipeline = static_cast<ComputePipeline *>(_data->subpasses[0]->computePipelines.get(ComputeShadowPass::SdfRoundedRectsComp)->pipeline.get());
		buf.cmdBindPipeline(pipeline);
		buf.cmdDispatch((_vertexBuffer->getRoundedRectsCount() - 1) / pipeline->getLocalX() + 1);
	}

	if (_vertexBuffer->getPolygonsCount()) {
		pipeline = static_cast<ComputePipeline *>(_data->subpasses[0]->computePipelines.get(ComputeShadowPass::SdfPolygonsComp)->pipeline.get());
		buf.cmdBindPipeline(pipeline);
		buf.cmdDispatch((_vertexBuffer->getPolygonsCount() - 1) / pipeline->getLocalX() + 1);
	}

	BufferMemoryBarrier bufferBarriers[] = {
		BufferMemoryBarrier(_vertexBuffer->getVertexes(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_primitivesBuffer->getTriangles(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_primitivesBuffer->getGridSize(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_primitivesBuffer->getGridIndex(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_primitivesBuffer->getCircles(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_primitivesBuffer->getRects(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_primitivesBuffer->getRoundedRects(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		BufferMemoryBarrier(_primitivesBuffer->getPolygons(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
	};

	ImageMemoryBarrier inImageBarriers[] = {
		ImageMemoryBarrier(sdfImage, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL)
	};

	buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
			bufferBarriers, inImageBarriers);

	pipeline = static_cast<ComputePipeline *>(_data->subpasses[0]->computePipelines.get(ComputeShadowPass::SdfImageComp)->pipeline.get());
	buf.cmdBindPipeline(pipeline);

	buf.cmdDispatch(
			(sdfImage->getInfo().extent.width - 1) / pipeline->getLocalX() + 1,
			(sdfImage->getInfo().extent.height - 1) / pipeline->getLocalY() + 1);

	// transfer image and buffer to transfer queue
	auto gIdx = _device->getQueueFamily(QueueOperations::Graphics)->index;

	if (_pool->getFamilyIdx() != gIdx) {

		BufferMemoryBarrier bufferBarriers[] = {
			BufferMemoryBarrier(_primitivesBuffer->getTriangles(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_primitivesBuffer->getGridSize(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_primitivesBuffer->getGridIndex(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_primitivesBuffer->getCircles(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_primitivesBuffer->getRects(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_primitivesBuffer->getRoundedRects(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_primitivesBuffer->getPolygons(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE),
			BufferMemoryBarrier(_lightsBuffer->getBuffer(), VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE)
		};

		ImageMemoryBarrier transferImageBarrier(sdfImage,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx});
		sdfImage->setPendingBarrier(transferImageBarrier);

		_primitivesBuffer->getTriangles()->setPendingBarrier(bufferBarriers[0]);
		_primitivesBuffer->getGridSize()->setPendingBarrier(bufferBarriers[1]);
		_primitivesBuffer->getGridIndex()->setPendingBarrier(bufferBarriers[2]);
		_primitivesBuffer->getCircles()->setPendingBarrier(bufferBarriers[3]);
		_primitivesBuffer->getRects()->setPendingBarrier(bufferBarriers[4]);
		_primitivesBuffer->getRoundedRects()->setPendingBarrier(bufferBarriers[5]);
		_primitivesBuffer->getPolygons()->setPendingBarrier(bufferBarriers[6]);
		_lightsBuffer->getBuffer()->setPendingBarrier(bufferBarriers[3]);

		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
				bufferBarriers, makeSpanView(&transferImageBarrier, 1));
	} else {
		ImageMemoryBarrier transferImageBarrier(sdfImage,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		sdfImage->setPendingBarrier(transferImageBarrier);
	}
}

Vector<const CommandBuffer *> ComputeShadowPassHandle::doPrepareCommands(FrameHandle &h) {
	auto buf = _pool->recordBuffer(*_device, [&] (CommandBuffer &buf) {
		auto pass = static_cast<RenderPass *>(_data->impl.get());

		pass->perform(*this, buf, [&] {
			writeShadowCommands(pass, buf);
		});
		return true;
	});

	return Vector<const CommandBuffer *>{buf};
}

}
