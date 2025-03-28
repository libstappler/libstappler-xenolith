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

#include "XLSnnVkSubpixelLayer.h"
#include "XLCoreAttachment.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameRequest.h"
#include "XLSnnVkShaders.h"
#include "XLVkPipeline.h"

namespace stappler::xenolith::vk::shadernn {

SubpixelLayer::~SubpixelLayer() { }

bool SubpixelLayer::init(Queue::Builder &queueBuilder, QueuePassBuilder &builder, Front *front,
		const AttachmentData *input, const AttachmentData *output) {
	using namespace core;

	auto precision = getAttachmentPrecision(output);

	_front = front;

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

	auto layout = builder.addDescriptorLayout([&] (PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&] (DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(passOutput, DescriptorType::StorageImage, AttachmentLayout::General);
			setBuilder.addDescriptor(passInput, DescriptorType::StorageImage, AttachmentLayout::General);
		});
	});

	builder.addSubpass([&] (SubpassBuilder &subpassBuilder) {
	    uint32_t subPixelFactor = 2;

		SpecializationInfo spec;
		spec.data = queueBuilder.addProgramByRef(toString(front->getName(), "_shader"), getShader(LayerShader::Subpixel, precision));
		spec.constants.emplace_back(SpecializationConstant(subPixelFactor)); // 0

		subpassBuilder.addComputePipeline(toString(front->getName(), "_pipeline"), layout, move(spec));
	});

	_inputAttachment = input;
	_outputAttachment = output;

	_frameHandleCallback = [] (core::QueuePass &pass, const FrameQueue &q) {
		return Rc<LayerHandle>::create(pass, q);
	};

	return QueuePass::init(builder);
}

bool SubpixelLayer::LayerHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (SubpixelLayer *)_queuePass.get();

	if (auto imageAttachment = q.getAttachment(pass->getInputAttachment())) {
		_inputImage = (const vk::ImageAttachmentHandle *)imageAttachment->handle.get();
	}

	if (auto imageAttachment = q.getAttachment(pass->getOutputAttachment())) {
		_outputImage = (const vk::ImageAttachmentHandle *)imageAttachment->handle.get();
	}

	_front = pass->getFront();

	return vk::QueuePassHandle::prepare(q, sp::move(cb));
}

Vector<const core::CommandBuffer *> SubpixelLayer::LayerHandle::doPrepareCommands(FrameHandle &frame) {
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
	return Vector<const core::CommandBuffer *>{buf};
}

}
