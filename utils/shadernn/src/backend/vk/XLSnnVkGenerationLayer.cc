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

#include "XLVkPipeline.h"
#include "XLCoreAttachment.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameRequest.h"
#include "XLSnnVkGenerationLayer.h"
#include "XLSnnVkShaders.h"

namespace stappler::xenolith::vk::shadernn {

GenerationLayer::~GenerationLayer() { }

bool GenerationLayer::init(Queue::Builder &queueBuilder, QueuePassBuilder &builder, const AttachmentData *output) {
	using namespace core;

	auto dataBuffer = queueBuilder.addAttachemnt("GenerationLayerData", [] (AttachmentBuilder &builder) -> Rc<core::Attachment> {
		builder.defineAsInput();
		auto a = Rc<GenericAttachment>::create(builder);
		a->setValidateInputCallback([] (const Attachment &, const Rc<AttachmentInputData> &data) {
			return dynamic_cast<GenerationDataInput *>(data.get()) != nullptr;
		});
		a->setFrameHandleCallback([] (Attachment &a, const FrameQueue &q) {
			auto h = Rc<core::AttachmentHandle>::create(a, q);
			h->setInputCallback([] (AttachmentHandle &handle, FrameQueue &queue, AttachmentInputData *input, Function<void(bool)> &&cb) {
				cb(true);
			});
			return h;
		});
		return a;
	});

	auto passOutput = builder.addAttachment(output, [] (AttachmentPassBuilder &builder) {
		builder.setDependency(AttachmentDependencyInfo{
			PipelineStage::ComputeShader, AccessType::ShaderWrite | AccessType::ShaderRead,
			PipelineStage::ComputeShader, AccessType::ShaderWrite | AccessType::ShaderRead,
			FrameRenderPassState::Submitted,
		});
	});

	builder.addAttachment(dataBuffer);

	auto layout = builder.addDescriptorLayout([&] (PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&] (DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(passOutput, DescriptorType::StorageImage, AttachmentLayout::General);
		});
	});

	auto precision = getAttachmentPrecision(output);

	builder.addSubpass([&] (SubpassBuilder &subpassBuilder) {
		subpassBuilder.addComputePipeline("GenerationLayerPipeline", layout,
				queueBuilder.addProgramByRef("GenerationLayerPipeline", getShader(LayerShader::Gen, precision)));
	});

	_outputAttachment = output;
	_dataAttachment = dataBuffer;

	_frameHandleCallback = [] (core::QueuePass &pass, const FrameQueue &q) {
		return Rc<LayerHandle>::create(pass, q);
	};

	return QueuePass::init(builder);
}

GenerationLayer::LayerHandle::~LayerHandle() { }

bool GenerationLayer::LayerHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (GenerationLayer *)_queuePass.get();

	if (auto imageAttachment = q.getAttachment(pass->getOutputAttachment())) {
		_outputImage = (const vk::ImageAttachmentHandle *)imageAttachment->handle.get();
	}

	if (auto bufferAttachment = q.getAttachment(pass->getDataAttachment())) {
		_dataBuffer = (const vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
	}

	return vk::QueuePassHandle::prepare(q, move(cb));
}

Vector<const vk::CommandBuffer *> GenerationLayer::LayerHandle::doPrepareCommands(FrameHandle &handle) {
	auto buf = _pool->recordBuffer(*_device, Vector<Rc<DescriptorPool>>(_descriptors), [&] (vk::CommandBuffer &buf) {
		auto pass = _data->impl.cast<vk::RenderPass>().get();
		pass->perform(*this, buf, [&] {
			auto extent = handle.getFrameConstraints().extent;
			auto input = static_cast<GenerationDataInput *>(_dataBuffer->getInput());

			buf.cmdBindDescriptorSets(pass, 0);
			buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0, BytesView(reinterpret_cast<uint8_t *>(&input->data), sizeof(GenerationData)));

			auto pipeline = static_cast<vk::ComputePipeline *>((*_data->subpasses[0]->computePipelines.begin())->pipeline.get());

			buf.cmdBindPipeline(pipeline);
			buf.cmdDispatch((extent.width - 1) / pipeline->getLocalX() + 1,
					(extent.height - 1) / pipeline->getLocalY() + 1,
					(extent.depth - 1) / pipeline->getLocalZ() + 1);
		}, true);
		return true;
	});
	return Vector<const vk::CommandBuffer *>{buf};
}

}
