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

#include "XLSnnVkInputLayer.h"
#include "XLSnnVkShaders.h"
#include "XLVkPipeline.h"
#include "XLVkObject.h"
#include "XLCoreAttachment.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameRequest.h"

namespace stappler::xenolith::vk::shadernn {

InputLayer:: ~InputLayer() { }

bool InputLayer::init(Queue::Builder &queueBuilder, QueuePassBuilder &builder, const AttachmentData *input, const AttachmentData *output) {
	using namespace core;

	auto dataBuffer = queueBuilder.addAttachemnt("InputLayerBuffer", [] (AttachmentBuilder &builder) -> Rc<core::Attachment> {
		builder.defineAsInput();
		auto a = Rc<GenericAttachment>::create(builder);
		a->setValidateInputCallback([] (const Attachment &, const Rc<AttachmentInputData> &data) {
			return dynamic_cast<InputDataInput *>(data.get()) != nullptr;
		});
		a->setFrameHandleCallback([] (Attachment &a, const FrameQueue &q) {
			auto h = Rc<AttachmentHandle>::create(a, q);
			h->setInputCallback([] (AttachmentHandle &handle, FrameQueue &queue, AttachmentInputData *input, Function<void(bool)> &&cb) {
				cb(true);
			});
			return h;
		});
		return a;
	});

	auto passInput = builder.addAttachment(input, [] (AttachmentPassBuilder &builder) {
		builder.setDependency(AttachmentDependencyInfo{
			PipelineStage::Transfer, AccessType::TransferWrite,
			PipelineStage::ComputeShader, AccessType::ShaderRead,
			FrameRenderPassState::Submitted,
		});
		builder.setInitialLayout(AttachmentLayout::TransferDstOptimal);
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

	builder.addAttachment(dataBuffer);

	auto layout = builder.addDescriptorLayout([&] (PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&] (DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(passInput, DescriptorType::StorageImage, AttachmentLayout::General);
			setBuilder.addDescriptor(passOutput, DescriptorType::StorageImage, AttachmentLayout::General);
		});
	});

	auto precision = getAttachmentPrecision(output);

	builder.addSubpass([&] (SubpassBuilder &subpassBuilder) {
		subpassBuilder.addComputePipeline("InputLayerPipeline", layout,
				queueBuilder.addProgramByRef("InputLayerProgram", getShader(LayerShader::Norm, precision)));
	});

	_inputAttachment = input;
	_outputAttachment = output;
	_dataAttachment = dataBuffer;

	_frameHandleCallback = [] (core::QueuePass &pass, const FrameQueue &q) {
		return Rc<LayerHandle>::create(pass, q);
	};

	return QueuePass::init(builder);
}

bool InputLayer::LayerHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (InputLayer *)_queuePass.get();

	if (auto imageAttachment = q.getAttachment(pass->getInputAttachment())) {
		_inputImage = (const vk::ImageAttachmentHandle *)imageAttachment->handle.get();
	}

	if (auto imageAttachment = q.getAttachment(pass->getOutputAttachment())) {
		_outputImage = (const vk::ImageAttachmentHandle *)imageAttachment->handle.get();
	}

	if (auto bufferAttachment = q.getAttachment(pass->getDataAttachment())) {
		_dataHandle = (const core::AttachmentHandle *)bufferAttachment->handle.get();
	}

	return vk::QueuePassHandle::prepare(q, sp::move(cb));
}

void InputLayer::LayerHandle::doTransferInput(vk::CommandBuffer &buf, DeviceFrameHandle &handle, InputDataInput *input) {
	auto &pool = handle.getMemPool(nullptr);

	// spawn image from frame memory pool
	auto image = static_cast<Image *>(_inputImage->getQueueData()->image->getImage().get());

	// alloc staging buffer
	auto staging = pool->spawn(AllocationUsage::DeviceLocalHostVisible, BufferInfo(core::BufferUsage::TransferSrc,
			size_t(image->getMemory()->getInfo().size)));

	staging->map([&] (uint8_t *buf, VkDeviceSize size) {
		input->image.writeData(buf, size);
	});

	buf.cmdCopyBufferToImage(staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0);

	ImageMemoryBarrier outImageBarrier(image, VkAccessFlags(core::AccessType::TransferWrite), VkAccessFlags(core::AccessType::ShaderRead),
			VkImageLayout(core::AttachmentLayout::TransferDstOptimal), VkImageLayout(core::AttachmentLayout::General));
	buf.cmdPipelineBarrier(VkPipelineStageFlags(core::PipelineStage::Transfer), VkPipelineStageFlags(core::PipelineStage::ComputeShader),
			0, makeSpanView(&outImageBarrier, 1));
}

Vector<const vk::CommandBuffer *> InputLayer::LayerHandle::doPrepareCommands(FrameHandle &handle) {
	auto buf = _pool->recordBuffer(*_device, Vector<Rc<DescriptorPool>>(_descriptors), [&] (vk::CommandBuffer &buf) {
		auto pass = _data->impl.cast<vk::RenderPass>().get();
		pass->perform(*this, buf, [&] {
			auto data = static_cast<InputDataInput *>(_dataHandle->getInput());
			auto extent = _outputImage->getQueueData()->image->getImage()->getInfo().extent;

			doTransferInput(buf, static_cast<DeviceFrameHandle &>(handle), data);

			buf.cmdBindDescriptorSets(pass, 0);
			buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0, BytesView(reinterpret_cast<uint8_t *>(&data->norm), sizeof(NormData)));

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

InputBufferLayer::~InputBufferLayer() { }

bool InputBufferLayer::init(Queue::Builder &queueBuilder, QueuePassBuilder &builder, Front *front, const AttachmentData *input, const AttachmentData *output) {
	using namespace core;

	auto dataBuffer = queueBuilder.addAttachemnt(toString(front->getName(), "_buffer"), [] (AttachmentBuilder &builder) -> Rc<core::Attachment> {
		builder.defineAsInput();
		auto a = Rc<BufferAttachment>::create(builder, BufferInfo(PassType::Compute, BufferUsage::StorageBuffer, BufferUsage::TransferDst));
		a->setValidateInputCallback([] (const Attachment &, const Rc<AttachmentInputData> &data) {
			return dynamic_cast<InputBufferDataInput *>(data.get()) != nullptr;
		});
		a->setFrameHandleCallback([] (Attachment &a, const FrameQueue &q) {
			auto h = Rc<BufferAttachmentHandle>::create(a, q);
			h->setInputCallback([] (AttachmentHandle &handle, FrameQueue &queue, AttachmentInputData *input, Function<void(bool)> &&cb) {
				cb(true);
			});
			return h;
		});
		return a;
	});

	builder.addAttachment(input);
	builder.addAttachment(output);
	auto passBuffers = builder.addAttachment(dataBuffer);

	auto layout = builder.addDescriptorLayout([&] (PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&] (DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptorArray(passBuffers, 2, DescriptorType::StorageBuffer);
		});
	});

	builder.addSubpass([&] (SubpassBuilder &subpassBuilder) {
		subpassBuilder.addComputePipeline(toString(front->getName(), "_pipeline"), layout,
			SpecializationInfo(
				queueBuilder.addProgram(toString(front->getName(), "_program"),getShader(LayerShader::BufferNorm, Precision::Unknown)),
				Vector<SpecializationConstant>{
					SpecializationConstant(2), // nbuffers
					SpecializationConstant(0), // output
					SpecializationConstant(1) // input
				}));
	});

	_inputAttachment = input;
	_outputAttachment = output;
	_dataAttachment = dataBuffer;
	_front = front;

	_frameHandleCallback = [] (core::QueuePass &pass, const FrameQueue &q) {
		return Rc<LayerHandle>::create(pass, q);
	};

	return QueuePass::init(builder);
}

bool InputBufferLayer::LayerHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (InputBufferLayer *)_queuePass.get();

	if (auto inputAttachment = q.getAttachment(pass->getInputAttachment())) {
		_inputBuffer = (vk::BufferAttachmentHandle *)inputAttachment->handle.get();
	}

	if (auto outputAttachment = q.getAttachment(pass->getOutputAttachment())) {
		_outputBuffer = (vk::BufferAttachmentHandle *)outputAttachment->handle.get();
	}

	if (auto bufferAttachment = q.getAttachment(pass->getDataAttachment())) {
		_dataHandle = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
	}

	_front = pass->getFront();

	bool isOutput = _outputBuffer->isOutput();
	auto input = static_cast<InputBufferDataInput *>(_dataHandle->getInput());
	auto handle = static_cast<DeviceFrameHandle *>(q.getFrame().get());
	auto &pool = handle->getMemPool(nullptr);

	auto bufSize = _front->getBufferSize() * sizeof(float);

	// alloc staging buffer
	auto staging = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::StorageBuffer,
			size_t(bufSize)));
	Rc<Buffer> dest;
	if (isOutput) {
		dest = pool->spawnPersistent(AllocationUsage::DeviceLocalHostVisible,
				BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::StorageBuffer,
				size_t(bufSize)));
	} else {
		dest = pool->spawn(AllocationUsage::DeviceLocal,
				BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::StorageBuffer,
				size_t(bufSize)));
	}

	staging->map([&] (uint8_t *buf, VkDeviceSize size) {
		input->buffer.writeData(buf, size);
	});

	_inputBuffer->addBufferView(staging);
	_outputBuffer->addBufferView(dest);

	_dataHandle->addBufferView(dest); // output
	_dataHandle->addBufferView(staging); // input

	return vk::QueuePassHandle::prepare(q, sp::move(cb));
}

Vector<const vk::CommandBuffer *> InputBufferLayer::LayerHandle::doPrepareCommands(FrameHandle &handle) {
	auto buf = _pool->recordBuffer(*_device, Vector<Rc<DescriptorPool>>(_descriptors), [&] (vk::CommandBuffer &buf) {
		auto pass = _data->impl.cast<vk::RenderPass>().get();
		pass->perform(*this, buf, [&] {
			struct NormBufferData {
				int32_t size;
				float mean;
				float norm;
			};

			NormBufferData pcb{int32_t(_front->getBufferSize()), _front->getMean(), _front->getNorm()};

			buf.cmdBindDescriptorSets(pass, 0);
			buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0, BytesView(reinterpret_cast<uint8_t *>(&pcb), sizeof(NormBufferData)));

			auto pipeline = static_cast<vk::ComputePipeline *>((*_data->subpasses[0]->computePipelines.begin())->pipeline.get());

			auto nbatches = (_front->getBufferSize() - 1) / pipeline->getLocalX() + 1;

			buf.cmdBindPipeline(pipeline);
			buf.cmdDispatch(nbatches, 1, 1);

			BufferMemoryBarrier barrier(_outputBuffer->getBuffers().front().buffer, VkAccessFlags(core::AccessType::ShaderWrite | core::AccessType::ShaderRead),
					VkAccessFlags(core::AccessType::ShaderWrite | core::AccessType::ShaderRead));
			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(&barrier, 1));
		}, true);
		return true;
	});
	return Vector<const vk::CommandBuffer *>{buf};
}

InputCsvIntLayer::~InputCsvIntLayer() { }

bool InputCsvIntLayer::init(Queue::Builder &queueBuilder, QueuePassBuilder &builder, Front *front,
		const AttachmentData *input, const AttachmentData *output) {
	using namespace core;

	auto normBuffer = queueBuilder.addBufferByRef(toString(front->getName(), "_normBuffer"),
			BufferInfo(BufferUsage::StorageBuffer, BufferPersistent(true), PassType::Compute),
			front->getNormDataBuffer(), nullptr, AccessType::ShaderRead);

	auto dataBuffer = queueBuilder.addAttachemnt("InputCsvIntLayerBuffer", [&] (AttachmentBuilder &builder) -> Rc<core::Attachment> {
		builder.defineAsInput();
		auto a = Rc<BufferAttachment>::create(builder, normBuffer);
		a->setValidateInputCallback([] (const Attachment &, const Rc<AttachmentInputData> &data) {
			return dynamic_cast<InputCsvInput *>(data.get()) != nullptr;
		});
		a->setFrameHandleCallback([] (Attachment &a, const FrameQueue &q) {
			auto h = Rc<BufferAttachmentHandle>::create(a, q);
			h->setInputCallback([] (AttachmentHandle &handle, FrameQueue &queue, AttachmentInputData *input, Function<void(bool)> &&cb) {
				cb(true);
			});
			return h;
		});
		return a;
	});

	auto passInput = builder.addAttachment(input);
	auto passOutput = builder.addAttachment(output);
	auto passData = builder.addAttachment(dataBuffer);

	auto layout = builder.addDescriptorLayout([&] (PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&] (DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(passOutput, DescriptorType::StorageBuffer);
			setBuilder.addDescriptor(passInput, DescriptorType::StorageBuffer);
			setBuilder.addDescriptor(passData, DescriptorType::StorageBuffer);
		});
	});

	builder.addSubpass([&] (SubpassBuilder &subpassBuilder) {
		subpassBuilder.addComputePipeline("InputCsvIntPipeline", layout,
				queueBuilder.addProgramByRef("InputCsvIntPProgram", getShader(LayerShader::StatNorm, Precision::Unknown)));
	});

	_inputAttachment = input;
	_outputAttachment = output;
	_dataAttachment = dataBuffer;
	_front = front;

	_frameHandleCallback = [] (core::QueuePass &pass, const FrameQueue &q) {
		return Rc<LayerHandle>::create(pass, q);
	};

	return QueuePass::init(builder);
}

bool InputCsvIntLayer::LayerHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (InputCsvIntLayer *)_queuePass.get();

	if (auto inputAttachment = q.getAttachment(pass->getInputAttachment())) {
		_inputBuffer = (vk::BufferAttachmentHandle *)inputAttachment->handle.get();
	}

	if (auto outputAttachment = q.getAttachment(pass->getOutputAttachment())) {
		_outputBuffer = (vk::BufferAttachmentHandle *)outputAttachment->handle.get();
	}

	if (auto bufferAttachment = q.getAttachment(pass->getDataAttachment())) {
		_dataHandle = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
	}

	_front = pass->getFront();

	auto input = static_cast<InputCsvInput *>(_dataHandle->getInput());
	auto handle = static_cast<DeviceFrameHandle *>(q.getFrame().get());
	auto &pool = handle->getMemPool(nullptr);

	auto bufferSize = sizeof(uint64_t) * _front->getFields().size() * input->data.size();

	// alloc staging buffer
	auto staging = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::StorageBuffer,
			size_t(bufferSize)));
	auto dest = pool->spawn(AllocationUsage::DeviceLocal,
			BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::StorageBuffer,
			size_t(bufferSize)));

	staging->map([&] (uint8_t *buf, VkDeviceSize size) {
		auto target = (uint64_t *)buf;

		for (auto &it : input->data) {
			for (auto &f : _front->getFields()) {
				*target = it.getInteger(f);
				++ target;
			}
		}
	});

	_inputBuffer->addBufferView(staging);
	_outputBuffer->addBufferView(dest);

	return vk::QueuePassHandle::prepare(q, sp::move(cb));
}

Vector<const vk::CommandBuffer *> InputCsvIntLayer::LayerHandle::doPrepareCommands(FrameHandle &handle) {
	auto buf = _pool->recordBuffer(*_device, Vector<Rc<DescriptorPool>>(_descriptors), [&] (vk::CommandBuffer &buf) {
		auto pass = _data->impl.cast<vk::RenderPass>().get();
		pass->perform(*this, buf, [&] {
			struct InputInfo {
				int size;
				int fields;
			};

			auto input = static_cast<InputCsvInput *>(_dataHandle->getInput());
			InputInfo pcb{int(input->data.size()), int(_front->getFields().size())};

			buf.cmdBindDescriptorSets(pass, 0);
			buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0, BytesView(reinterpret_cast<uint8_t *>(&pcb), sizeof(InputInfo)));

			auto pipeline = static_cast<vk::ComputePipeline *>((*_data->subpasses[0]->computePipelines.begin())->pipeline.get());

			auto nbatches = (pcb.size - 1) / pipeline->getLocalY() + 1;

			buf.cmdBindPipeline(pipeline);
			buf.cmdDispatch(pcb.fields, nbatches, 1);
		}, true);
		return true;
	});
	return Vector<const vk::CommandBuffer *>{buf};
}

}
