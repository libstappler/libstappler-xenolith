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

#include "XLSnnVkLossLayer.h"

namespace stappler::xenolith::vk::shadernn {

static auto BufferAccessFlags =
		VkAccessFlags(core::AccessType::ShaderWrite | core::AccessType::ShaderRead);

static StringView getPipelineOpName(CrossEntropyLossLayer::PipelineOpIndex idx) {
	switch (idx) {
	case CrossEntropyLossLayer::PipelineOpIndex::MatrixSoftmaxByRows:
		return "MatrixSoftmaxByRows";
		break;
	case CrossEntropyLossLayer::PipelineOpIndex::VectorNegLog: return "VectorNegLog"; break;
	case CrossEntropyLossLayer::PipelineOpIndex::VectorEltwiseMultiply:
		return "VectorEltwiseMultiply";
		break;
	case CrossEntropyLossLayer::PipelineOpIndex::SumMatrixColumnsToResult:
		return "SumMatrixColumnsToResult";
		break;
	case CrossEntropyLossLayer::PipelineOpIndex::VectorSub: return "VectorSub"; break;
	case CrossEntropyLossLayer::PipelineOpIndex::SumMatrixColumnsLabels:
		return "SumMatrixColumns";
		break;
	case CrossEntropyLossLayer::PipelineOpIndex::MultiplyDiagMatrixByMatrix:
		return "MultiplyDiagMatrixByMatrix";
		break;
	case CrossEntropyLossLayer::PipelineOpIndex::VectorDotProduct: return "VectorDotProduct"; break;
	case CrossEntropyLossLayer::PipelineOpIndex::MultiplyDiagMatrixByMatrixForInput:
		return "MultiplyDiagMatrixByMatrixForInput";
		break;
	}
	return StringView();
}

static LayerShader getPipelineOpShader(CrossEntropyLossLayer::PipelineOpIndex idx) {
	switch (idx) {
	case CrossEntropyLossLayer::PipelineOpIndex::MatrixSoftmaxByRows:
		return LayerShader::MatrixSoftmaxByRows;
		break;
	case CrossEntropyLossLayer::PipelineOpIndex::VectorNegLog: return LayerShader::VectorLog; break;
	case CrossEntropyLossLayer::PipelineOpIndex::VectorEltwiseMultiply:
		return LayerShader::VectorEltwiseMultiply;
		break;
	case CrossEntropyLossLayer::PipelineOpIndex::SumMatrixColumnsToResult:
		return LayerShader::SumMatrixColumns;
		break;
	case CrossEntropyLossLayer::PipelineOpIndex::VectorSub: return LayerShader::VectorSub; break;
	case CrossEntropyLossLayer::PipelineOpIndex::SumMatrixColumnsLabels:
		return LayerShader::SumMatrixColumns;
		break;
	case CrossEntropyLossLayer::PipelineOpIndex::MultiplyDiagMatrixByMatrix:
		return LayerShader::MultiplyDiagMatrixByMatrix;
		break;
	case CrossEntropyLossLayer::PipelineOpIndex::VectorDotProduct:
		return LayerShader::VectorDotProduct;
		break;
	case CrossEntropyLossLayer::PipelineOpIndex::MultiplyDiagMatrixByMatrixForInput:
		return LayerShader::MultiplyDiagMatrixByMatrix;
		break;
	}
	return LayerShader::Gen;
}

CrossEntropyLossLayer::~CrossEntropyLossLayer() { }

bool CrossEntropyLossLayer::init(Queue::Builder &queueBuilder, QueuePassBuilder &builder,
		Front *front, const AttachmentData *inputLabels, const AttachmentData *inputNetwork,
		const AttachmentData *output) {
	using namespace core;

	_front = front;

	auto paramsBuffer = queueBuilder.addBuffer(toString(builder.getName(), "_params_buffer"),
			BufferInfo(front->getParameters().size() * sizeof(float),
					BufferUsage::StorageBuffer | BufferUsage::TransferSrc, PassType::Compute),
			[front = front](uint8_t *buf, uint64_t size, const BufferData::DataCallback &cb) {
		memcpy(buf, front->getParameters().data(), size);
	});

	auto weightsBuffer = queueBuilder.addBuffer(toString(builder.getName(), "_weights_buffer"),
			BufferInfo(front->getWeightBufferSize(),
					BufferUsage::StorageBuffer | BufferUsage::TransferSrc, PassType::Compute),
			[](uint8_t *buf, uint64_t size, const BufferData::DataCallback &cb) {
		FillFloatBuffer(buf, size, 1.0f);
	});

	auto resultBuffer = queueBuilder.addBuffer(toString(builder.getName(), "_result_buffer"),
			BufferInfo(front->getResultBufferSize(),
					BufferUsage::StorageBuffer | BufferUsage::TransferSrc, PassType::Compute),
			[](uint8_t *buf, uint64_t size, const BufferData::DataCallback &cb) {
		FillFloatBuffer(buf, size, 0.0f);
	});

	auto lossGradientBuffer =
			queueBuilder.addBuffer(toString(builder.getName(), "_lossGradient_buffer"),
					BufferInfo(front->getLossGradientBufferSize(), BufferUsage::StorageBuffer,
							PassType::Compute),
					[](uint8_t *buf, uint64_t size, const BufferData::DataCallback &cb) {
		FillFloatBuffer(buf, size, 0.0f);
	});

	auto weightsAttachment = queueBuilder.addAttachemnt(toString(builder.getName(), "_weights"),
			[&](AttachmentBuilder &builder) -> Rc<core::Attachment> {
		return Rc<vk::BufferAttachment>::create(builder,
				Vector<const BufferData *>{
					paramsBuffer,
					weightsBuffer,
					resultBuffer,
					lossGradientBuffer,
				});
	});

	builder.addAttachment(inputLabels,
			AttachmentDependencyInfo::make(PipelineStage::ComputeShader, AccessType::ShaderRead));
	builder.addAttachment(inputNetwork,
			AttachmentDependencyInfo::make(PipelineStage::ComputeShader, AccessType::ShaderRead));
	builder.addAttachment(output);
	auto passWeights = builder.addAttachment(weightsAttachment);

	auto layout = builder.addDescriptorLayout([&](PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&](DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptorArray(passWeights, DescriptorArraySize,
					DescriptorType::StorageBuffer);
		});
	});

	builder.addSubpass([&](SubpassBuilder &subpassBuilder) {
		auto addPipeline2 = [&](PipelineOpIndex idx, uint32_t output, uint32_t input,
									PipelineOpFn &&fn) {
			auto name = getPipelineOpName(idx);
			auto shader = getPipelineOpShader(idx);

			auto data = subpassBuilder.addComputePipeline(toString(builder.getName(), "_", name),
					layout->defaultFamily,
					SpecializationInfo(queueBuilder.addProgramByRef(
											   toString(builder.getName(), "_", name, "_shader"),
											   getShader(shader, Precision::Unknown)),
							Vector<SpecializationConstant>{
								SpecializationConstant(DescriptorArraySize), // nbuffers
								SpecializationConstant(output), // output
								SpecializationConstant(input), // input
							}));

			_pipelineOps.emplace(idx, PipelineOp(idx, data, sp::move(fn)));
		};

		auto addPipeline3 = [&](PipelineOpIndex idx, uint32_t output, uint32_t inputA,
									uint32_t inputB, PipelineOpFn &&fn,
									Vector<SpecializationConstant> &&extra =
											Vector<SpecializationConstant>()) {
			auto name = getPipelineOpName(idx);
			auto shader = getPipelineOpShader(idx);

			auto constants = Vector<SpecializationConstant>{
				SpecializationConstant(DescriptorArraySize), // nbuffers
				SpecializationConstant(output), // output
				SpecializationConstant(inputA), // input
				SpecializationConstant(inputB), // input
			};

			for (auto &it : extra) { constants.emplace_back(it); }

			auto data = subpassBuilder.addComputePipeline(toString(builder.getName(), "_", name),
					layout->defaultFamily,
					SpecializationInfo(queueBuilder.addProgramByRef(
											   toString(builder.getName(), "_", name, "_shader"),
											   getShader(shader, Precision::Unknown)),
							constants));

			_pipelineOps.emplace(idx, PipelineOp(idx, data, sp::move(fn)));

			return data;
		};

		addPipeline2(PipelineOpIndex::MatrixSoftmaxByRows, ActivationIdx, InputNetworkIdx,
				[](Front *front, CommandBuffer &buf, const core::ComputePipelineData *pipeline,
						SpanView<BufferView> buffers) {
			MatrixSoftmaxByRows(buf, pipeline, front->getBatchSize(), front->getClassesCount());
		});

		addPipeline2(PipelineOpIndex::VectorNegLog, ActivationEltwiseMulIdx, ActivationIdx,
				[](Front *front, CommandBuffer &buf, const core::ComputePipelineData *pipeline,
						SpanView<BufferView> buffers) {
			BufferMemoryBarrier barrier(buffers[ActivationIdx].buffer, BufferAccessFlags,
					BufferAccessFlags);

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(&barrier, 1));

			VectorNegLog(buf, pipeline, front->getBatchSize() * front->getClassesCount());
		});

		addPipeline3(PipelineOpIndex::VectorEltwiseMultiply, ActivationEltwiseMulIdx,
				InputLabelsIdx, ActivationEltwiseMulIdx,
				[](Front *front, CommandBuffer &buf, const core::ComputePipelineData *pipeline,
						SpanView<BufferView> buffers) {
			BufferMemoryBarrier barrier(buffers[ActivationEltwiseMulIdx].buffer, BufferAccessFlags,
					BufferAccessFlags);

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(&barrier, 1));

			VectorEltwiseMultiply(buf, pipeline, front->getBatchSize() * front->getClassesCount());
		});

		addPipeline2(PipelineOpIndex::SumMatrixColumnsToResult, LossValueIdx,
				ActivationEltwiseMulIdx,
				[](Front *front, CommandBuffer &buf, const core::ComputePipelineData *pipeline,
						SpanView<BufferView> buffers) {
			BufferMemoryBarrier barrier(buffers[ActivationEltwiseMulIdx].buffer, BufferAccessFlags,
					BufferAccessFlags);

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(&barrier, 1));

			SumMatrixColumns(buf, pipeline, front->getBatchSize(), front->getClassesCount());
		});

		addPipeline3(PipelineOpIndex::VectorDotProduct, ParamsIdx, WeightsIdx, LossValueIdx,
				[](Front *front, CommandBuffer &buf, const core::ComputePipelineData *pipeline,
						SpanView<BufferView> buffers) {
			BufferMemoryBarrier barriers[2] = {
				BufferMemoryBarrier(buffers[WeightsIdx].buffer, BufferAccessFlags,
						BufferAccessFlags),
				BufferMemoryBarrier(buffers[LossValueIdx].buffer, BufferAccessFlags,
						BufferAccessFlags),
			};

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(barriers, 2));

			VectorDotProduct(buf, pipeline, front->getBatchSize());
		},
				Vector<SpecializationConstant>{
					SpecializationConstant(Front::P_Loss),
					SpecializationConstant(1),
					SpecializationConstant(Front::P_LossDivider),
				});

		if (front->getModel()->isTrainable()) {
			addPipeline3(PipelineOpIndex::VectorSub, ActivationEltwiseMulIdx, ActivationIdx,
					InputLabelsIdx,
					[](Front *front, CommandBuffer &buf, const core::ComputePipelineData *pipeline,
							SpanView<BufferView> buffers) {
				BufferMemoryBarrier barriers[2] = {
					BufferMemoryBarrier(buffers[ActivationIdx].buffer, BufferAccessFlags,
							BufferAccessFlags),
					BufferMemoryBarrier(buffers[ActivationEltwiseMulIdx].buffer, BufferAccessFlags,
							BufferAccessFlags),
				};

				buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(barriers, 2));

				VectorSub(buf, pipeline, front->getBatchSize() * front->getClassesCount());
			});

			addPipeline2(PipelineOpIndex::SumMatrixColumnsLabels, ActivationIdx, InputLabelsIdx,
					[](Front *front, CommandBuffer &buf, const core::ComputePipelineData *pipeline,
							SpanView<BufferView> buffers) {
				BufferMemoryBarrier barrier(buffers[ActivationIdx].buffer, BufferAccessFlags,
						BufferAccessFlags);

				buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(&barrier, 1));

				SumMatrixColumns(buf, pipeline, front->getBatchSize(), front->getClassesCount());
			});

			addPipeline3(PipelineOpIndex::MultiplyDiagMatrixByMatrix, LossGradientIdx,
					ActivationIdx, ActivationEltwiseMulIdx,
					[](Front *front, CommandBuffer &buf, const core::ComputePipelineData *pipeline,
							SpanView<BufferView> buffers) {
				BufferMemoryBarrier barriers[2] = {
					BufferMemoryBarrier(buffers[ActivationIdx].buffer, BufferAccessFlags,
							BufferAccessFlags),
					BufferMemoryBarrier(buffers[ActivationEltwiseMulIdx].buffer, BufferAccessFlags,
							BufferAccessFlags),
				};

				buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(barriers, 2));

				MultiplyDiagMatrixByMatrix(buf, pipeline, front->getBatchSize(),
						front->getClassesCount(), front->getBatchSize() * front->getClassesCount());
			});

			addPipeline3(PipelineOpIndex::MultiplyDiagMatrixByMatrixForInput, ActivationIdx,
					WeightsIdx, LossGradientIdx,
					[](Front *front, CommandBuffer &buf, const core::ComputePipelineData *pipeline,
							SpanView<BufferView> buffers) {
				BufferMemoryBarrier barrier(buffers[ActivationIdx].buffer, BufferAccessFlags,
						BufferAccessFlags);

				buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(&barrier, 1));

				MultiplyDiagMatrixByMatrix(buf, pipeline, front->getBatchSize(),
						front->getClassesCount(), front->getBatchSize() * front->getClassesCount());
			},
					Vector<SpecializationConstant>{
						SpecializationConstant(1), // MODIFIERS_ENABLED
						SpecializationConstant(ParamsIdx), // PARAMETERS_INDEX
						SpecializationConstant(
								Front::P_LossGradientDivider), // MULTIPLIER_PARAMETER_OFFSET
						SpecializationConstant(Front::P_MinGradient), // MIN_PARAMETER_OFFSET
						SpecializationConstant(Front::P_MaxGradient), // MAX_PARAMETER_OFFSET
					});
		}

		subpassBuilder.setPrepareCallback(
				[](core::FrameQueue &q, const core::SubpassData &subpass) {
			auto layer = (CrossEntropyLossLayer *)subpass.pass->pass.get();

			vk::BufferAttachmentHandle *inputNetworkBuffer = nullptr;
			vk::BufferAttachmentHandle *inputLabelsBuffer = nullptr;
			vk::BufferAttachmentHandle *outputBuffer = nullptr;
			vk::BufferAttachmentHandle *weightsBuffer = nullptr;

			if (auto bufferAttachment = q.getAttachment(layer->getInputNetworkAttachment())) {
				inputNetworkBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
			}

			if (auto bufferAttachment = q.getAttachment(layer->getInputLabelsAttachment())) {
				inputLabelsBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
			}

			if (auto bufferAttachment = q.getAttachment(layer->getOutputAttachment())) {
				outputBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
			}

			if (auto bufferAttachment = q.getAttachment(layer->getWeightsAttachment())) {
				weightsBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
			}

			auto front = layer->getFront();

			auto handle = static_cast<DeviceFrameHandle *>(q.getFrame().get());
			auto pool = handle->getMemPool(nullptr);

			auto batchSize = front->getBatchSize();
			auto vectorSize = front->getClassesCount();
			auto totalSize = batchSize * vectorSize;

			auto activationBuffer = pool->spawn(AllocationUsage::DeviceLocal,
					BufferInfo(size_t(totalSize * sizeof(float)),
							BufferUsage::StorageBuffer | BufferUsage::TransferSrc));
			auto activationEltwiseMulBuffer = pool->spawn(AllocationUsage::DeviceLocal,
					BufferInfo(size_t(totalSize * sizeof(float)),
							BufferUsage::StorageBuffer | BufferUsage::TransferSrc));

			outputBuffer->addBufferView(weightsBuffer->getBuffers()[2].buffer);
			outputBuffer->addBufferView(weightsBuffer->getBuffers()[0].buffer);

			weightsBuffer->addBufferView(inputNetworkBuffer->getBuffers().front().buffer);
			weightsBuffer->addBufferView(inputLabelsBuffer->getBuffers().front().buffer);
			weightsBuffer->addBufferView(activationBuffer);
			weightsBuffer->addBufferView(activationEltwiseMulBuffer);
		});

		subpassBuilder.setCommandsCallback(
				[](FrameQueue &q, const SubpassData &subpass, core::CommandBuffer &b) {
			auto &buf = static_cast<CommandBuffer &>(b);
			auto pass = static_cast<RenderPass *>(subpass.pass->impl.get());
			auto layer = (CrossEntropyLossLayer *)subpass.pass->pass.get();

			vk::BufferAttachmentHandle *weightsBuffer = nullptr;
			if (auto bufferAttachment = q.getAttachment(layer->getWeightsAttachment())) {
				weightsBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
			}

			buf.cmdBindDescriptorSets(pass, 0);

			layer->runAll(buf, weightsBuffer->getBuffers());
		});
	});

	builder.addCompleteCallback(
			[front = _front](FrameQueue &q, const QueuePassData &pass, bool success) {
		auto layer = (CrossEntropyLossLayer *)pass.pass.get();

		vk::BufferAttachmentHandle *weightsBuffer = nullptr;
		if (auto bufferAttachment = q.getAttachment(layer->getWeightsAttachment())) {
			weightsBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
		}

		auto params = weightsBuffer->getBuffers()[ParamsIdx].buffer;

		if (success) {
			/*q.getFrame()->getLoop()->captureBuffer([front] (const BufferInfo &info, BytesView view) {
				auto name = toString(front->getName(),".", front->getInputIndex(), ".label.bin");
				xenolith::shadernn::Model::saveBlob(
						filesystem::currentDir<Interface>(name).data(),
						view.data(), view.size());
				std::cout << "Save " << name << "\n";
			}, weightsBuffer->getBuffers()[InputLabelsIdx].buffer.get());

			q.getFrame()->getLoop()->captureBuffer([front] (const BufferInfo &info, BytesView view) {
				auto name = toString(front->getName(),".", front->getInputIndex(), ".activation.bin");
				xenolith::shadernn::Model::saveBlob(
						filesystem::currentDir<Interface>(name).data(),
						view.data(), view.size());
				std::cout << "Save " << name << "\n";
			}, weightsBuffer->getBuffers()[ActivationIdx].buffer.get());

			q.getFrame()->getLoop()->captureBuffer([front] (const BufferInfo &info, BytesView view) {
				auto name = toString(front->getName(),".", front->getInputIndex(), ".activation.mul.bin");
				xenolith::shadernn::Model::saveBlob(
						filesystem::currentDir<Interface>(name).data(),
						view.data(), view.size());
				std::cout << "Save " << name << "\n";
			}, weightsBuffer->getBuffers()[ActivationEltwiseMulIdx].buffer.get());

			q.getFrame()->getLoop()->captureBuffer([front] (const BufferInfo &info, BytesView view) {
				auto name = toString(front->getName(),".", front->getInputIndex(), ".value.bin");
				xenolith::shadernn::Model::saveBlob(
						filesystem::currentDir<Interface>(name).data(),
						view.data(), view.size());
				std::cout << "Save " << name << "\n";
			}, weightsBuffer->getBuffers()[LossValueIdx].buffer.get());
			*/
		}
	});

	_inputLabelsAttachment = inputLabels;
	_inputNetworkAttachment = inputNetwork;
	_weightAttachment = weightsAttachment;
	_outputAttachment = output;

	_frameHandleCallback = [](core::QueuePass &pass, const FrameQueue &q) {
		return Rc<vk::QueuePassHandle>::create(pass, q);
	};

	if (_front->getModel()->isTrainable()) {
		initPropagation(queueBuilder, builder);
	}

	return QueuePass::init(builder);
}

void CrossEntropyLossLayer::initPropagation(Queue::Builder &queueBuilder,
		QueuePassBuilder &builder) {
	const core::QueuePassData *pass = _inputNetworkAttachment->passes.front()->pass;
	if (auto trainableLayer = dynamic_cast<TrainableLayer *>(pass->pass.get())) {
		trainableLayer->initPropagation(queueBuilder, builder, _weightAttachment, ActivationIdx);
	}
}

void CrossEntropyLossLayer::runAll(CommandBuffer &buf, SpanView<BufferView> buffers) {
	for (auto &it : _pipelineOps) { it.second.command(_front, buf, it.second.pipeline, buffers); }
}

} // namespace stappler::xenolith::vk::shadernn
