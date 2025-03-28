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

#include "XLSnnVkStatPercentLayer.h"
#include "XLSnnVkShaders.h"

namespace stappler::xenolith::vk::shadernn {

StatPercentLayer::~StatPercentLayer() { }

bool StatPercentLayer::init(Queue::Builder &queueBuilder, QueuePassBuilder &builder, Front *front,
		const AttachmentData *input, const AttachmentData *output) {
	using namespace core;

	auto classesBuffer = queueBuilder.addAttachemnt("StatPercentLayerClassesBuffer", [&] (AttachmentBuilder &builder) -> Rc<core::Attachment> {
		return Rc<BufferAttachment>::create(builder, BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::StorageBuffer));
	});

	auto passInput = builder.addAttachment(input);
	auto passOutput = builder.addAttachment(output);
	auto passClasses = builder.addAttachment(classesBuffer);

	auto layout = builder.addDescriptorLayout([&] (PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&] (DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(passOutput, DescriptorType::StorageBuffer);
			setBuilder.addDescriptor(passInput, DescriptorType::StorageBuffer);
			setBuilder.addDescriptor(passClasses, DescriptorType::StorageBuffer);
		});
	});

	builder.addSubpass([&] (SubpassBuilder &subpassBuilder) {
		subpassBuilder.addComputePipeline(StatPercentLayerClassesPipeline, layout,
				queueBuilder.addProgramByRef("StatPercentLayerClassesPProgram", getShader(LayerShader::StatClassMap, Precision::Unknown)));
		subpassBuilder.addComputePipeline(StatPercentLayerPercentPipeline, layout,
				queueBuilder.addProgramByRef("StatPercentLayerPercentProgram", getShader(LayerShader::StatClassPercent, Precision::Unknown)));
	});

	_inputAttachment = input;
	_outputAttachment = output;
	_classesAttachment = classesBuffer;
	_front = front;

	_frameHandleCallback = [] (core::QueuePass &pass, const FrameQueue &q) {
		return Rc<LayerHandle>::create(pass, q);
	};

	return QueuePass::init(builder);
}

bool StatPercentLayer::LayerHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (StatPercentLayer *)_queuePass.get();

	if (auto imageAttachment = q.getAttachment(pass->getInputAttachment())) {
		_inputBuffer = (vk::BufferAttachmentHandle *)imageAttachment->handle.get();
	}

	if (auto imageAttachment = q.getAttachment(pass->getOutputAttachment())) {
		_outputBuffer = (vk::BufferAttachmentHandle *)imageAttachment->handle.get();
	}

	if (auto bufferAttachment = q.getAttachment(pass->getClassesAttachment())) {
		_classesBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
	}

	_front = pass->getFront();

	auto handle = static_cast<DeviceFrameHandle *>(q.getFrame().get());
	auto &pool = handle->getMemPool(nullptr);
	auto extent = handle->getFrameConstraints().extent;

	_classesSizes = pool->spawnPersistent(AllocationUsage::DeviceLocal,
			BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::StorageBuffer, PassType::Compute,
				size_t(_front->getClassCount() * sizeof(uint32_t))
	));
	_classesIndexes = pool->spawn(AllocationUsage::DeviceLocal,
			BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::StorageBuffer, PassType::Compute,
				size_t(_front->getClassCount() * extent.height * sizeof(uint32_t))
	));
	_output = pool->spawnPersistent(AllocationUsage::DeviceLocal,
			BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::StorageBuffer, PassType::Compute,
				size_t(_front->getClassCount() * (sizeof(float) * 4 + sizeof(uint32_t) * 4))
	));

	_classesBuffer->addBufferView(_classesSizes);
	_classesBuffer->addBufferView(_classesIndexes);
	_outputBuffer->addBufferView(_output);

	return vk::QueuePassHandle::prepare(q, sp::move(cb));
}

Vector<const core::CommandBuffer *> StatPercentLayer::LayerHandle::doPrepareCommands(FrameHandle &handle) {
	auto buf = _pool->recordBuffer(*_device, Vector<Rc<DescriptorPool>>(_descriptors), [&] (vk::CommandBuffer &buf) {
		auto pass = _data->impl.cast<vk::RenderPass>().get();
		pass->perform(*this, buf, [&] {
			struct ClassesInputInfo {
				int size;
				int fields;
				int fieldClass;
				int classMin;
				int classMax;
				int fieldSource;
				int fieldTarget;
				int classCount;
			};

			auto extent = handle.getFrameConstraints().extent;

			ClassesInputInfo pcb1;
			pcb1.size = extent.height;
			pcb1.fields = _inputBuffer->getBuffers().front().buffer->getSize() / (sizeof(uint64_t) * pcb1.size);
			pcb1.fieldClass = _front->getFieldClass();
			pcb1.classMin = _front->getClassMin();
			pcb1.classMax = _front->getClassMin() + _front->getClassCount() - 1;
			pcb1.fieldSource = _front->getFieldSource();
			pcb1.fieldTarget = _front->getFieldTarget();
			pcb1.classCount = _front->getClassCount();

			buf.cmdFillBuffer(_classesIndexes, 0);
			buf.cmdFillBuffer(_classesSizes, 0);

			BufferMemoryBarrier b[2] = {
				BufferMemoryBarrier(_classesSizes, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT),
				BufferMemoryBarrier(_classesSizes, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT)
			};

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(b, 2));

			buf.cmdBindDescriptorSets(pass, 0);
			buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0, BytesView(reinterpret_cast<uint8_t *>(&pcb1), sizeof(ClassesInputInfo)));

			vk::ComputePipeline *classesPipeline = nullptr;
			auto classesPipelineIt = _data->subpasses[0]->computePipelines.find(StatPercentLayerClassesPipeline);
			if (classesPipelineIt != _data->subpasses[0]->computePipelines.end()) {
				classesPipeline = static_cast<vk::ComputePipeline *>((*classesPipelineIt)->pipeline.get());
			}

			buf.cmdBindPipeline(classesPipeline);
			buf.cmdDispatch(1, (pcb1.size - 1) / classesPipeline->getLocalY() + 1, 1);

			vk::ComputePipeline *percentPipeline = nullptr;
			auto percentPipelineeIt = _data->subpasses[0]->computePipelines.find(StatPercentLayerPercentPipeline);
			if (percentPipelineeIt != _data->subpasses[0]->computePipelines.end()) {
				percentPipeline = static_cast<vk::ComputePipeline *>((*percentPipelineeIt)->pipeline.get());
			}

			buf.cmdBindPipeline(percentPipeline);
			buf.cmdDispatch((pcb1.classCount - 1) / percentPipeline->getLocalX() + 1, 1, 1);
		}, true);
		return true;
	});
	return Vector<const core::CommandBuffer *>{buf};
}

void StatPercentLayer::LayerHandle::doSubmitted(FrameHandle &h, Function<void(bool)> &&cb, bool s, Rc<Fence> &&fence) {
	vk::QueuePassHandle::doSubmitted(h, sp::move(cb), s, sp::move(fence));

	/*h.getLoop()->captureBuffer([] (const BufferInfo &info, BytesView view) {
		std::cout << view.size() / (sizeof(float) * 4) << "\n";

		std::cout << "0";

		size_t row = 1;
		size_t i = 0;
		while (!view.empty()) {
			switch (i) {
			case 0:
			case 1:
			case 2:
			case 3:
				std::cout << ", " << view.readFloat32();
				break;
			case 4:
			case 5:
			case 6:
				std::cout << ", " << view.readUnsigned32();
				break;
			default:
				view.readUnsigned32();
				break;
			}
			++ i;
			if (i > 7) {
				i = 0;
				std::cout << "\n" << row ++;
			}
		}
		std::cout << "\n";
	}, _output);*/
}

StatAnalysisLayer::~StatAnalysisLayer() { }

bool StatAnalysisLayer::init(Queue::Builder &queueBuilder, QueuePassBuilder &builder, Front *front,
		const AttachmentData *inputData, const AttachmentData *inputClasses, const AttachmentData *output) {
	using namespace core;

	auto passInputData = builder.addAttachment(inputData);
	auto passInputClasses = builder.addAttachment(inputClasses);
	auto passOutput = builder.addAttachment(output);

	auto layout = builder.addDescriptorLayout([&] (PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&] (DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(passOutput, DescriptorType::StorageBuffer);
			setBuilder.addDescriptor(passInputData, DescriptorType::StorageBuffer);
			setBuilder.addDescriptor(passInputClasses, DescriptorType::StorageBuffer);
		});
	});

	builder.addSubpass([&] (SubpassBuilder &subpassBuilder) {
		subpassBuilder.addComputePipeline("StatAnalysisLayerProgram", layout,
				queueBuilder.addProgramByRef("StatAnalysisLayerProgram", getShader(LayerShader::StatAnalysis, Precision::Unknown)));
	});

	_inputDataAttachment = inputData;
	_inputClassesAttachment = inputClasses;
	_outputAttachment = output;
	_front = front;

	_frameHandleCallback = [] (core::QueuePass &pass, const FrameQueue &q) {
		return Rc<LayerHandle>::create(pass, q);
	};

	return QueuePass::init(builder);
}

bool StatAnalysisLayer::LayerHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (StatAnalysisLayer *)_queuePass.get();

	if (auto attachment = q.getAttachment(pass->getInputDataAttachment())) {
		_inputDataBuffer = (vk::BufferAttachmentHandle *)attachment->handle.get();
	}

	if (auto attachment = q.getAttachment(pass->getInputClassesAttachment())) {
		_inputClassesBuffer = (vk::BufferAttachmentHandle *)attachment->handle.get();
	}

	if (auto imageAttachment = q.getAttachment(pass->getOutputAttachment())) {
		_outputBuffer = (vk::BufferAttachmentHandle *)imageAttachment->handle.get();
	}

	_front = pass->getFront();

	auto handle = static_cast<DeviceFrameHandle *>(q.getFrame().get());
	auto &pool = handle->getMemPool(nullptr);
	auto extent = handle->getFrameConstraints().extent;

	_output = pool->spawnPersistent(AllocationUsage::DeviceLocal,
			BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::StorageBuffer, PassType::Compute,
				size_t(extent.height * (sizeof(float) * 4))
	));

	_outputBuffer->addBufferView(_output);

	return vk::QueuePassHandle::prepare(q, sp::move(cb));
}

Vector<const core::CommandBuffer *> StatAnalysisLayer::LayerHandle::doPrepareCommands(FrameHandle &handle) {
	auto buf = _pool->recordBuffer(*_device, Vector<Rc<DescriptorPool>>(_descriptors), [&] (vk::CommandBuffer &buf) {
		auto pass = _data->impl.cast<vk::RenderPass>().get();
		pass->perform(*this, buf, [&] {
			struct InputInfo {
				int size;
				int fields;
				int fieldClass;
				int classMin;
				int classMax;
				int fieldSource;
				int fieldTarget;
				int classCount;
				float threshold;
			};

			auto extent = handle.getFrameConstraints().extent;

			InputInfo pcb1;
			pcb1.size = extent.height;
			pcb1.fields = _inputDataBuffer->getBuffers().front().buffer->getSize() / (sizeof(uint64_t) * pcb1.size);
			pcb1.fieldClass = _front->getFieldClass();
			pcb1.classMin = _front->getClassMin();
			pcb1.classMax = _front->getClassMin() + _front->getClassCount() - 1;
			pcb1.fieldSource = _front->getFieldSource();
			pcb1.fieldTarget = _front->getFieldTarget();
			pcb1.classCount = _front->getClassCount();
			pcb1.threshold = _front->getThreshold();

			buf.cmdBindDescriptorSets(pass, 0);
			buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0, BytesView(reinterpret_cast<uint8_t *>(&pcb1), sizeof(InputInfo)));

			auto pipeline = static_cast<vk::ComputePipeline *>((*_data->subpasses[0]->computePipelines.begin())->pipeline.get());

			buf.cmdBindPipeline(pipeline);
			buf.cmdDispatch((pcb1.size - 1) / pipeline->getLocalX() + 1, 1, 1);
		}, true);
		return true;
	});
	return Vector<const core::CommandBuffer *>{buf};
}

void StatAnalysisLayer::LayerHandle::doSubmitted(FrameHandle &h, Function<void(bool)> &&cb, bool s, Rc<Fence> &&fence) {
	vk::QueuePassHandle::doSubmitted(h, sp::move(cb), s, sp::move(fence));

	/*h.getLoop()->captureBuffer([] (const BufferInfo &info, BytesView view) {
		std::cout << view.size() / (sizeof(float) * 4) << "\n";
	}, _output);*/
}

}
