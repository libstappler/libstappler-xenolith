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

#include "XLSnnVkConvLayer.h"
#include "XLCoreAttachment.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameRequest.h"
#include "XLSnnVkShaders.h"
#include "XLVkPipeline.h"

namespace stappler::xenolith::vk::shadernn {

static core::ImageFormat getPrecisionKernelFormat(Precision p) {
	switch (p) {
	case Precision::Unknown:
		return core::ImageFormat::Undefined;
		break;
	case Precision::F8:
		return core::ImageFormat::R8G8B8A8_UNORM;
		break;
	case Precision::F16:
		return core::ImageFormat::R16G16B16A16_SFLOAT;
		break;
	case Precision::F32:
		return core::ImageFormat::R32G32B32A32_SFLOAT;
		break;
	case Precision::F64:
		return core::ImageFormat::R64G64B64A64_SFLOAT;
		break;
	}
	return core::ImageFormat::Undefined;
}

Conv2DLayer::~Conv2DLayer() { }

bool Conv2DLayer::init(Queue::Builder &queueBuilder, QueuePassBuilder &builder, Front *front,
		const AttachmentData *input, const AttachmentData *output) {
	using namespace core;

	auto precision = getAttachmentPrecision(output);

	_front = front;

	auto kernelImage = queueBuilder.addImageByRef(toString(front->getName(), "_kernelImage"),
			ImageInfo(front->getKernelExtent(), ImageUsage::Storage, ImageTiling::Optimal,
					getPrecisionKernelFormat(precision), PassType::Compute, ImageHints::Static),
			front->getKernelImageData(), AttachmentLayout::General);

	auto biasBuffer = queueBuilder.addBufferByRef(toString(front->getName(), "_biasBuffer"),
			BufferInfo(BufferUsage::StorageBuffer, BufferPersistent(true), PassType::Compute),
			front->getBiasBufferData());

	auto betaBuffer = queueBuilder.addBufferByRef(toString(front->getName(), "_betaBuffer"),
			BufferInfo(BufferUsage::StorageBuffer, BufferPersistent(true), PassType::Compute),
			front->getNormBetaBufferData());

	auto gammaBuffer = queueBuilder.addBufferByRef(toString(front->getName(), "_gammaBuffer"),
			BufferInfo(BufferUsage::StorageBuffer, BufferPersistent(true), PassType::Compute),
			front->getNormGammaBufferData());

	auto meanBuffer = queueBuilder.addBufferByRef(toString(front->getName(), "_meanBuffer"),
			BufferInfo(BufferUsage::StorageBuffer, BufferPersistent(true), PassType::Compute),
			front->getNormMeanBufferData());

	auto varianceBuffer = queueBuilder.addBufferByRef(toString(front->getName(), "_varianceBuffer"),
			BufferInfo(BufferUsage::StorageBuffer, BufferPersistent(true), PassType::Compute),
			front->getNormVarianceBufferData());

	auto kernelAttachment = queueBuilder.addAttachemnt(toString(front->getName(), "_kernel"),
			[&] (AttachmentBuilder &builder) -> Rc<core::Attachment> {
		return Rc<vk::ImageAttachment>::create(builder,
			kernelImage,
			ImageAttachment::AttachmentInfo{
				.initialLayout = AttachmentLayout::Ignored,
				.finalLayout = AttachmentLayout::Ignored,
				.clearOnLoad = false
			}
		);
	});

	auto dataAttachment = queueBuilder.addAttachemnt(toString(front->getName(), "_data"),
			[&] (AttachmentBuilder &builder) -> Rc<core::Attachment> {
		return Rc<vk::BufferAttachment>::create(builder,
			Vector<const BufferData *>{
				biasBuffer,
				betaBuffer,
				gammaBuffer,
				meanBuffer,
				varianceBuffer
			}
		);
	});

	auto passInput = builder.addAttachment(input, [] (AttachmentPassBuilder &builder) {
		builder.setDependency(AttachmentDependencyInfo{
			PipelineStage::ComputeShader, AccessType::ShaderRead,
			PipelineStage::ComputeShader, AccessType::ShaderRead,
			FrameRenderPassState::Submitted,
		});
		builder.setInitialLayout(AttachmentLayout::General);
		builder.setFinalLayout(AttachmentLayout::General);
	});

	auto passOutput = builder.addAttachment(output, [] (AttachmentPassBuilder &builder) {
		builder.setDependency(AttachmentDependencyInfo{
			PipelineStage::ComputeShader, AccessType::ShaderWrite,
			PipelineStage::ComputeShader, AccessType::ShaderWrite,
			FrameRenderPassState::Submitted,
		});
		builder.setInitialLayout(AttachmentLayout::General);
		builder.setFinalLayout(AttachmentLayout::General);
	});

	auto passKernel = builder.addAttachment(kernelAttachment, [] (AttachmentPassBuilder &builder) {
		builder.setDependency(AttachmentDependencyInfo{
			PipelineStage::ComputeShader, AccessType::ShaderRead,
			PipelineStage::ComputeShader, AccessType::ShaderRead,
			FrameRenderPassState::Submitted,
		});
		builder.setInitialLayout(AttachmentLayout::General);
		builder.setFinalLayout(AttachmentLayout::General);
	});

	auto passData = builder.addAttachment(dataAttachment);

	auto layout = builder.addDescriptorLayout([&] (PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&] (DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(passOutput, DescriptorType::StorageImage, AttachmentLayout::General);
			setBuilder.addDescriptor(passInput, DescriptorType::StorageImage, AttachmentLayout::General);
			setBuilder.addDescriptor(passKernel, DescriptorType::StorageImage, AttachmentLayout::General);
			setBuilder.addDescriptor(passData, DescriptorType::StorageBuffer);
		});
	});

	builder.addSubpass([&] (SubpassBuilder &subpassBuilder) {
		auto paddings = front->getPaddingOffset();
		auto kernel = front->getKernelSize();
		auto stride = front->getStride();
		auto mode = front->getPaddingMode();
	    uint32_t dilate = 1;

	    uint32_t paddingMode = 0;
	    if (mode == "constant") {
	        paddingMode = 1;
	    } else if (mode == "replicate") {
	        paddingMode = 2;
	    } else if (mode == "reflect") {
	        paddingMode = 3;
	    }

		SpecializationInfo spec;
		spec.data = queueBuilder.addProgramByRef(toString(front->getName(), "_shader"), getShader(LayerShader::Conv2d, precision));
		spec.constants.emplace_back(SpecializationConstant(paddings.x)); // 0
		spec.constants.emplace_back(SpecializationConstant(paddings.z)); // 1
		spec.constants.emplace_back(SpecializationConstant(kernel)); // 2
		spec.constants.emplace_back(SpecializationConstant(kernel)); // 3
		spec.constants.emplace_back(SpecializationConstant(stride)); // 4
		spec.constants.emplace_back(SpecializationConstant(stride)); // 5
		spec.constants.emplace_back(SpecializationConstant(dilate)); // 6
		spec.constants.emplace_back(SpecializationConstant(dilate)); // 7
		spec.constants.emplace_back(SpecializationConstant(4)); // 8
		spec.constants.emplace_back(SpecializationConstant(front->getActivation())); // 9
		spec.constants.emplace_back(SpecializationConstant(paddingMode)); // 10
		spec.constants.emplace_back(SpecializationConstant(uint32_t(front->useBatchNormalization()))); // 11
		spec.constants.emplace_back(SpecializationConstant(uint32_t(front->useBias()))); // 12
		spec.constants.emplace_back(SpecializationConstant(front->getLeakyReluAlpha())); // 13

		subpassBuilder.addComputePipeline(toString(front->getName(), "_pipeline"), layout, move(spec));
	});

	_inputAttachment = input;
	_outputAttachment = output;
	_kernelAttachment = kernelAttachment;
	_dataAttachment = dataAttachment;

	_frameHandleCallback = [] (core::QueuePass &pass, const FrameQueue &q) {
		return Rc<LayerHandle>::create(pass, q);
	};

	return QueuePass::init(builder);
}

bool Conv2DLayer::LayerHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (Conv2DLayer *)_queuePass.get();

	if (auto imageAttachment = q.getAttachment(pass->getInputAttachment())) {
		_inputImage = (const vk::ImageAttachmentHandle *)imageAttachment->handle.get();
	}

	if (auto imageAttachment = q.getAttachment(pass->getOutputAttachment())) {
		_outputImage = (const vk::ImageAttachmentHandle *)imageAttachment->handle.get();
	}

	if (auto kernelAttachment = q.getAttachment(pass->getKernelAttachment())) {
		_kernelImage = (const vk::ImageAttachmentHandle *)kernelAttachment->handle.get();
	}

	if (auto bufferAttachment = q.getAttachment(pass->getDataAttachment())) {
		_dataHandle = bufferAttachment->handle.get();
	}

	_front = pass->getFront();

	return vk::QueuePassHandle::prepare(q, move(cb));
}

Vector<const vk::CommandBuffer *> Conv2DLayer::LayerHandle::doPrepareCommands(FrameHandle &frame) {
	auto buf = _pool->recordBuffer(*_device, Vector<Rc<DescriptorPool>>(_descriptors), [&] (vk::CommandBuffer &buf) {
		auto pass = _data->impl.cast<vk::RenderPass>().get();
		pass->perform(*this, buf, [&] {
			buf.cmdBindDescriptorSets(pass, 0);

			auto extent = _outputImage->getQueueData()->image->getInfo().extent;

			auto oc_4 = UP_DIV(_front->getNumOutputPlanes(), uint32_t(4));

			auto pipeline = static_cast<vk::ComputePipeline *>((*_data->subpasses[0]->computePipelines.begin())->pipeline.get());

			buf.cmdBindPipeline(pipeline);
			buf.cmdDispatch((extent.width - 1) / pipeline->getLocalX() + 1,
					(extent.height - 1) / pipeline->getLocalY() + 1,
					(oc_4 - 1) / pipeline->getLocalZ() + 1);
		}, true);
		return true;
	});
	return Vector<const vk::CommandBuffer *>{buf};
}

}
