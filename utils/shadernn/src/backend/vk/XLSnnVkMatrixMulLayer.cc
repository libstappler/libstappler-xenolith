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

#include "XLSnnVkMatrixMulLayer.h"
#include "XLSnnVkNmeMath.h"
#include "XLSnnMatrixMulLayer.h"
#include "XLSnnModel.h"

namespace stappler::xenolith::vk::shadernn {

MatrixMulLayer::~MatrixMulLayer() { }

bool MatrixMulLayer::init(Queue::Builder &queueBuilder, QueuePassBuilder &builder, Front *front,
		const AttachmentData *input, const AttachmentData *output) {
	using namespace core;

	_front = front;

	auto weightsBuffer = queueBuilder.addBuffer(toString(builder.getName(), "_weights_buffer"),
			BufferInfo(front->getWeightBufferSize(), BufferUsage::StorageBuffer | BufferUsage::TransferSrc, PassType::Compute),
			[front = _front] (uint8_t *buf, uint64_t size, const BufferData::DataCallback &cb) {
		/*auto name = toString(front->getName(), ".", front->getInputIndex(), ".weights.bin");
		xenolith::shadernn::Model::loadBlob(name.data(), [&] (const uint8_t *blob, size_t s) {
			memcpy(buf, blob, size);
		});*/

		front->generateWeights(buf, size, cb);
	});

	auto freeTermsBuffer = queueBuilder.addBuffer(toString(builder.getName(), "_freeTerms_buffer"),
			BufferInfo(front->getKernelSize() * sizeof(float), BufferUsage::StorageBuffer | BufferUsage::TransferSrc, PassType::Compute),
			[front = _front] (uint8_t *buf, uint64_t size, const BufferData::DataCallback &cb) {
		/*auto name = toString(front->getName(), ".", front->getInputIndex(), ".terms.bin");
		xenolith::shadernn::Model::loadBlob(name.data(), [&] (const uint8_t *blob, size_t s) {
			memcpy(buf, blob, size);
		});*/

		front->generateFreeTerms(buf, size, cb);
	});

	_nbuffers = 4;
	_weightsBufferIndex = 0;
	_freeTermBufferIndex = 1;
	_inputBufferIndex = 2;
	_outputBufferIndex = 3;

	const AttachmentData *weightsAttachment = nullptr;

	weightsAttachment = queueBuilder.addAttachemnt(toString(builder.getName(), "_weights"),
			[&] (AttachmentBuilder &builder) -> Rc<core::Attachment> {
		return Rc<vk::BufferAttachment>::create(builder, Vector<const BufferData *>{
			weightsBuffer,
			freeTermsBuffer
		});
	});

	builder.addAttachment(input, AttachmentDependencyInfo::make(PipelineStage::ComputeShader, AccessType::ShaderRead));
	builder.addAttachment(output);
	auto passWeights = builder.addAttachment(weightsAttachment);

	auto layout = builder.addDescriptorLayout([&] (PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&] (DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptorArray(passWeights, _nbuffers, DescriptorType::StorageBuffer);
		});
	});

	builder.addSubpass([&] (SubpassBuilder &subpassBuilder) {

		auto matMul = subpassBuilder.addComputePipeline(toString(builder.getName(), "_matMul_pipeline"), layout,
			SpecializationInfo(
				queueBuilder.addProgramByRef(toString(builder.getName(), "_matMul_shader"),
					getShader(LayerShader::MultiplyMatrixByTransposedMatrix, Precision::Unknown)),
				Vector<SpecializationConstant>{
					SpecializationConstant(_nbuffers), // nbuffers
					SpecializationConstant(_outputBufferIndex), // output
					SpecializationConstant(_inputBufferIndex), // input
					SpecializationConstant(_weightsBufferIndex),  // weight
					SpecializationConstant(_front->getInputIndex())
				}));

		auto matMulBorders = subpassBuilder.addComputePipeline(toString(builder.getName(), "_matMulBorders_pipeline"), layout,
			SpecializationInfo(
				queueBuilder.addProgramByRef(toString(builder.getName(), "_matMulBorders_shader"),
					getShader(LayerShader::MultiplyMatrixByTransposedMatrixBorder, Precision::Unknown)),
				Vector<SpecializationConstant>{
					SpecializationConstant(_nbuffers), // nbuffers
					SpecializationConstant(_outputBufferIndex), // output
					SpecializationConstant(_inputBufferIndex), // input
					SpecializationConstant(_weightsBufferIndex),  // weight
					SpecializationConstant(_front->getInputIndex())
				}));

		auto addVec = subpassBuilder.addComputePipeline(toString(builder.getName(), "_addVec_pipeline"), layout,
			SpecializationInfo(
				queueBuilder.addProgramByRef(toString(builder.getName(), "_addVec_shader"),
					getShader(LayerShader::AddVectorToMatrixRows, Precision::Unknown)),
				Vector<SpecializationConstant>{
					SpecializationConstant(_nbuffers), // nbuffers
					SpecializationConstant(_outputBufferIndex), // output
					SpecializationConstant(_outputBufferIndex), // output
					SpecializationConstant(_freeTermBufferIndex)  // terms
				}));

		auto relu = subpassBuilder.addComputePipeline(toString(builder.getName(), "_relu_pipeline"), layout,
			SpecializationInfo(
				queueBuilder.addProgramByRef(toString(builder.getName(), "_relu_shader"),
					getShader(LayerShader::VectorReLU, Precision::Unknown)),
				Vector<SpecializationConstant>{
					SpecializationConstant(_nbuffers), // nbuffers
					SpecializationConstant(_outputBufferIndex), // output
					SpecializationConstant(_outputBufferIndex), // output
				}));

		auto relu4 = subpassBuilder.addComputePipeline(toString(builder.getName(), "_relu4_pipeline"), layout,
			SpecializationInfo(
				queueBuilder.addProgramByRef(toString(builder.getName(), "_relu4_shader"),
					getShader(LayerShader::VectorReLU4, Precision::Unknown)),
				Vector<SpecializationConstant>{
					SpecializationConstant(_nbuffers), // nbuffers
					SpecializationConstant(_outputBufferIndex), // output
					SpecializationConstant(_outputBufferIndex), // output
				}));

		subpassBuilder.setPrepareCallback([this] (const SubpassData &subpass, FrameQueue &q) {
			// log::debug("MatrixMulLayer", getName(), ": prepare");
			auto layer = (MatrixMulLayer *)subpass.pass->pass.get();

			vk::BufferAttachmentHandle *inputBuffer = nullptr;
			vk::BufferAttachmentHandle *outputBuffer = nullptr;
			vk::BufferAttachmentHandle *weightsBuffer = nullptr;

			if (auto bufferAttachment = q.getAttachment(layer->getInputAttachment())) {
				inputBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
			}

			if (auto bufferAttachment = q.getAttachment(layer->getOutputAttachment())) {
				outputBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
			}

			if (auto bufferAttachment = q.getAttachment(layer->getWeightsAttachment())) {
				weightsBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
			}

			auto handle = static_cast<DeviceFrameHandle *>(q.getFrame().get());
			auto &pool = handle->getMemPool(nullptr);

			auto extent = layer->getFront()->getOutputExtent();

			auto input = inputBuffer->getBuffers().front().buffer;
			auto output = pool->spawn(AllocationUsage::DeviceLocal,
				BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::StorageBuffer, PassType::Compute,
					size_t(extent.depth * layer->getFront()->getKernelSize() * sizeof(float))
			));

			auto feedback = pool->spawn(AllocationUsage::DeviceLocal,
				BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::StorageBuffer, PassType::Compute,
					size_t(output->getSize())
			));

			weightsBuffer->addBufferView(input);
			weightsBuffer->addBufferView(output);

			outputBuffer->addBufferView(output);
			outputBuffer->addBufferView(feedback);
		});

		subpassBuilder.setCommandsCallback(
				[this, outputBufferIndex = _outputBufferIndex, matMul, matMulBorders, addVec, relu4, relu]
				 	 (const SubpassData &subpass, FrameQueue &q, core::CommandBuffer &b) {
			// log::debug("MatrixMulLayer", getName(), ": commands");
			auto &buf = static_cast<CommandBuffer &>(b);
			auto pass = static_cast<RenderPass *>(subpass.pass->impl.get());
			auto layer = static_cast<MatrixMulLayer *>(subpass.pass->pass.get());

			vk::BufferAttachmentHandle *weightsBuffer = nullptr;
			if (auto bufferAttachment = q.getAttachment(layer->getWeightsAttachment())) {
				weightsBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
			}

			vk::BufferAttachmentHandle *outputBuffer = nullptr;
			if (auto bufferAttachment = q.getAttachment(layer->getOutputAttachment())) {
				outputBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
			}

			auto output = outputBuffer->getBuffers()[0].buffer;
			auto feedback = outputBuffer->getBuffers()[1].buffer;

			auto kernelSize = layer->getFront()->getWeightSize();

			const int secondHeight = kernelSize.height;
			const int secondWidth = kernelSize.width;

			auto input = layer->getFront()->getInput();

			const int firstHeight = input->getOutputExtent().depth;
			const int firstWidth = input->getOutputExtent().width;
			const int resultWidth = layer->getFront()->getOutputExtent().width;

			/*std::cout << "secondHeight: " << secondHeight << "\n";
			std::cout << "secondWidth: " << secondWidth << "\n";
			std::cout << "firstHeight: " << firstHeight << "\n";
			std::cout << "firstWidth: " << firstWidth << "\n";
			std::cout << "resultWidth: " << resultWidth << "\n";
			std::cout << "\n";*/

			buf.cmdBindDescriptorSets(pass, 0);

			auto flags = VkAccessFlags(core::AccessType::ShaderWrite | core::AccessType::ShaderRead);

			BufferMemoryBarrier barriers[4] = {
					BufferMemoryBarrier(weightsBuffer->getBuffers()[0].buffer, flags, flags),
					BufferMemoryBarrier(weightsBuffer->getBuffers()[1].buffer, flags, flags),
					BufferMemoryBarrier(weightsBuffer->getBuffers()[2].buffer, flags, flags),
					BufferMemoryBarrier(weightsBuffer->getBuffers()[3].buffer, flags, flags)
			};

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(barriers, 4));

			MultiplyMatrixByTransposedMatrix(
					buf, static_cast<ComputePipeline *>(matMul->pipeline.get()), static_cast<ComputePipeline *>(matMulBorders->pipeline.get()),
					/*first inputData, */firstHeight, firstWidth, firstWidth,
					/*second weightData, */secondHeight, secondWidth,
					/*resultoutputData, */resultWidth, /*unused*/0 );

			BufferMemoryBarrier barrier(output, VkAccessFlags(core::AccessType::ShaderWrite | core::AccessType::ShaderRead),
					VkAccessFlags(core::AccessType::ShaderWrite | core::AccessType::ShaderRead));
			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(&barrier, 1));

			AddVectorToMatrixRows(buf, static_cast<ComputePipeline *>(addVec->pipeline.get()),  /*batchSize*/1, firstHeight, resultWidth);

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(barriers, 4));

			// save original output
			buf.cmdCopyBuffer(output, feedback);

			if (layer->getFront()->getActivation() == Activation::RELU) {
				buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(&barrier, 1));
				VectorReLU(buf, static_cast<ComputePipeline *>(relu4->pipeline.get()), static_cast<ComputePipeline *>(relu->pipeline.get()),
						output->getSize() / sizeof(float), 0.0f);
			}

			BufferMemoryBarrier barrier1[1] = {
				BufferMemoryBarrier(feedback, flags, flags),
			};

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(barrier1, 1));
		});
	});

	builder.addCompleteCallback([this] (const QueuePassData &, FrameQueue &q, bool success) {
		// log::debug("MatrixMulLayer", getName(), ": submitted");
		/*vk::BufferAttachmentHandle *weightsBuffer = nullptr;
		vk::BufferAttachmentHandle *outputBuffer = nullptr;

		if (auto bufferAttachment = q.getAttachment(getWeightsAttachment())) {
			weightsBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
		}

		if (auto bufferAttachment = q.getAttachment(getOutputAttachment())) {
			outputBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
		}

		auto sec = Time::now().toSeconds();

		q.getFrame()->getLoop()->captureBuffer([this, sec] (const BufferInfo &info, BytesView view) {
			xenolith::shadernn::Model::saveBlob(
					filesystem::currentDir<Interface>(toString(getName(),".", _front->getInputIndex(), ".weights.bin")).data(),
					view.data(), view.size());
		}, weightsBuffer->getBuffers()[_weightsBufferIndex].buffer.get());

		q.getFrame()->getLoop()->captureBuffer([this, sec] (const BufferInfo &info, BytesView view) {
			xenolith::shadernn::Model::saveBlob(
					filesystem::currentDir<Interface>(toString(getName(),".", _front->getInputIndex(), ".terms.bin")).data(),
					view.data(), view.size());
		}, weightsBuffer->getBuffers()[_freeTermBufferIndex].buffer.get());*/

		/*outputBuffer->getBuffers()[1].buffer->map([this, sec] (uint8_t *data, VkDeviceSize size) {
			std::cout << getName() << " ";
			base16::encode(std::cout, BytesView(data, 64));
			std::cout << "\n";
			xenolith::shadernn::Model::saveBlob(
					filesystem::currentDir<Interface>(toString(getName(),".", _front->getInputIndex(), ".output.bin")).data(),
					data, size);
		});*/

		/*q.getFrame()->getLoop()->captureBuffer([this, sec] (const BufferInfo &info, BytesView view) {
			xenolith::shadernn::Model::saveBlob(
					filesystem::currentDir<Interface>(toString(getName(),".", _front->getInputIndex(), ".input.bin")).data(),
					view.data(), view.size());
		}, weightsBuffer->getBuffers()[_inputBufferIndex].buffer.get());*/

		q.getFrame()->getLoop()->captureBuffer([this, sec] (const BufferInfo &info, BytesView view) {
			xenolith::shadernn::Model::saveBlob(
					filesystem::currentDir<Interface>(toString(getName(),".", _front->getInputIndex(), ".output.bin")).data(),
					view.data(), view.size());
		}, weightsBuffer->getBuffers()[_outputBufferIndex].buffer.get());
	});

	_inputAttachment = input;
	_outputAttachment = output;
	_weightsAttachment = weightsAttachment;

	_frameHandleCallback = [] (core::QueuePass &pass, const FrameQueue &q) {
		return Rc<vk::QueuePassHandle>::create(pass, q);
	};

	return QueuePass::init(builder);
}

void MatrixMulLayer::initPropagationSubpass(core::Queue::Builder &builder, core::QueuePassBuilder &queueBuilder,
		core::SubpassBuilder &subpass, const core::PipelineLayoutData *layout) {

	auto backwardNeeded = isBackwardNeeded();

	_fullPropagationBuffers = _staticPropagationBuffers;

	_propWeightsIndex = _fullPropagationBuffers ++;
	_propTermsIndex = _fullPropagationBuffers ++;
	_propOriginalOutput = _fullPropagationBuffers ++;
	_propOriginalInput = _fullPropagationBuffers ++;

	_propOutputDiff = _fullPropagationBuffers ++;
	_propWeightsDiff = _fullPropagationBuffers ++;
	_propTermsDiff = _fullPropagationBuffers ++;
	_propFeedback = _fullPropagationBuffers ++;
	_propTargetIndex = _fullPropagationBuffers ++;

	const core::ComputePipelineData *matMul = nullptr;
	const core::ComputePipelineData *matMulBorders = nullptr;
	const core::ComputePipelineData *reluDiff = nullptr;

	subpass.setPrepareCallback([this, backwardNeeded, sourceWeights = _weightsBufferIndex, sourceTerms = _freeTermBufferIndex]
								(const core::SubpassData &subpass, FrameQueue &q) {
		auto handle = static_cast<DeviceFrameHandle *>(q.getFrame().get());
		auto &pool = handle->getMemPool(nullptr);

		vk::BufferAttachmentHandle *weightsBuffer = nullptr;
		vk::BufferAttachmentHandle *outputBuffer = nullptr;
		vk::BufferAttachmentHandle *inputBuffer = nullptr;
		vk::BufferAttachmentHandle *propagationBuffer = nullptr;
		vk::BufferAttachmentHandle *externalPropagationSource = nullptr;

		if (auto bufferAttachment = q.getAttachment(getWeightsAttachment())) {
			weightsBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
		}

		if (auto bufferAttachment = q.getAttachment(getOutputAttachment())) {
			outputBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
		}

		if (auto bufferAttachment = q.getAttachment(getInputAttachment())) {
			inputBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
		}

		if (auto bufferAttachment = q.getAttachment(getPropagationAttachment())) {
			propagationBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
		}

		if (auto bufferAttachment = q.getAttachment(getExternalPropagationDataSource())) {
			externalPropagationSource = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
		}

		propagationBuffer->addBufferView(weightsBuffer->getBuffers()[sourceWeights].buffer);
		propagationBuffer->addBufferView(weightsBuffer->getBuffers()[sourceTerms].buffer);
		propagationBuffer->addBufferView(outputBuffer->getBuffers().back().buffer); // use feedback, direct output transformed with activation
		propagationBuffer->addBufferView(inputBuffer->getBuffers().front().buffer);

		// output from prev layer
		propagationBuffer->addBufferView(externalPropagationSource->getBuffers()[getExternalPropagationBufferIdx()].buffer);

		auto weightsDiff = pool->spawn(AllocationUsage::DeviceLocal,
			BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::TransferDst | core::BufferUsage::StorageBuffer, PassType::Compute,
				size_t(_front->getWeightBufferSize())
		));
		propagationBuffer->addBufferView(weightsDiff);

		auto termsDiff = pool->spawn(AllocationUsage::DeviceLocal,
			BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::TransferDst | core::BufferUsage::StorageBuffer, PassType::Compute,
				size_t(_front->getKernelSize() * sizeof(float))
		));
		propagationBuffer->addBufferView(termsDiff);

		auto weightExtent = _front->getWeightSize();
		auto outputExtent = _front->getOutputExtent();

		const int resultBufferSize = outputExtent.depth * weightExtent.width;

		auto feedback = pool->spawn(AllocationUsage::DeviceLocal,
			BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::TransferDst | core::BufferUsage::StorageBuffer, PassType::Compute,
				size_t(resultBufferSize * sizeof(float))
		));
		propagationBuffer->addBufferView(feedback);

		auto inputDiff = pool->spawn(AllocationUsage::DeviceLocal,
			BufferInfo(core::BufferUsage::TransferSrc | core::BufferUsage::StorageBuffer, PassType::Compute,
				size_t(resultBufferSize * sizeof(float))
		));

		propagationBuffer->addBufferView(inputDiff);
	});

	if (backwardNeeded) {
		matMul = subpass.addComputePipeline(toString(getName(), "_BackwardOnce_mul"), layout,
			core::SpecializationInfo(
				builder.addProgramByRef(toString(getName(), "_BackwardOnce_mul"),
					getShader(LayerShader::MultiplyMatrixByMatrix, Precision::Unknown)),
				Vector<core::SpecializationConstant>{
					core::SpecializationConstant(getPropagationSubpassBufferCount()), // nbuffers
					core::SpecializationConstant(_propTargetIndex), // output
					core::SpecializationConstant(_propOutputDiff), // input
					core::SpecializationConstant(_propWeightsIndex),  // weight
					core::SpecializationConstant(_front->getInputIndex()),
				}));

		matMulBorders = subpass.addComputePipeline(toString(getName(), "_BackwardOnce_mulBorders"), layout,
			core::SpecializationInfo(
				builder.addProgramByRef(toString(getName(), "_BackwardOnce_mulBorders"),
					getShader(LayerShader::MultiplyMatrixByMatrixBorder, Precision::Unknown)),
				Vector<core::SpecializationConstant>{
					core::SpecializationConstant(getPropagationSubpassBufferCount()), // nbuffers
					core::SpecializationConstant(_propTargetIndex), // output
					core::SpecializationConstant(_propOutputDiff), // input
					core::SpecializationConstant(_propWeightsIndex)  // weight
				}));
	}

	if (_front->getActivation() == Activation::RELU) {
		reluDiff = subpass.addComputePipeline(toString(getName(), "_BackwardOnce_reluDiff"), layout,
			core::SpecializationInfo(
				builder.addProgramByRef(toString(getName(), "_BackwardOnce_reluDiff"),
					getShader(LayerShader::VectorReLUDiff, Precision::Unknown)),
				Vector<core::SpecializationConstant>{
					core::SpecializationConstant(getPropagationSubpassBufferCount()), // nbuffers
					core::SpecializationConstant(_propOutputDiff), // input diff
					core::SpecializationConstant(_propOriginalOutput), // original output
					core::SpecializationConstant(_propOutputDiff)  // output diff
				}));
	}

	auto learnMatMul = subpass.addComputePipeline(toString(getName(), "_LearnOnce_MatMul"), layout,
		core::SpecializationInfo(
			builder.addProgramByRef(toString(getName(), "_LearnOnce_MatMul"),
				getShader(LayerShader::MultiplyTransposedMatrixByMatrix, Precision::Unknown)),
			Vector<core::SpecializationConstant>{
				core::SpecializationConstant(getPropagationSubpassBufferCount()), // nbuffers
				core::SpecializationConstant(_propWeightsDiff),
				core::SpecializationConstant(_propOutputDiff),
				core::SpecializationConstant(_propOriginalInput)
			}));

	auto learnMatMulBorder = subpass.addComputePipeline(toString(getName(), "_LearnOnce_MatMulBorder"), layout,
		core::SpecializationInfo(
			builder.addProgramByRef(toString(getName(), "_LearnOnce_MatMulBorder"),
				getShader(LayerShader::MultiplyTransposedMatrixByMatrixBorder, Precision::Unknown)),
			Vector<core::SpecializationConstant>{
				core::SpecializationConstant(getPropagationSubpassBufferCount()), // nbuffers
				core::SpecializationConstant(_propWeightsDiff),
				core::SpecializationConstant(_propOutputDiff),
				core::SpecializationConstant(_propOriginalInput)
			}));

	auto learnSum = subpass.addComputePipeline(toString(getName(), "_LearnOnce_Sum"), layout,
			core::SpecializationInfo(
				builder.addProgramByRef(toString(getName(), "_LearnOnce_Sum"),
					getShader(LayerShader::SumMatrixRows, Precision::Unknown)),
				Vector<core::SpecializationConstant>{
					core::SpecializationConstant(getPropagationSubpassBufferCount()), // nbuffers
					core::SpecializationConstant(_propTermsDiff),
					core::SpecializationConstant(_propOutputDiff),
				}));

	struct TrainPipelines {
		const core::ComputePipelineData *decayHistory;
		const core::ComputePipelineData *multHistory;
		const core::ComputePipelineData *add4;
		const core::ComputePipelineData *add1;
	};

	auto initTrainPipelines = [&] (uint32_t staticparam, uint32_t diff, uint32_t target) {
		TrainPipelines ret;

		ret.decayHistory = subpass.addComputePipeline(toString(getName(), "_trainDecayHistory", staticparam), layout,
			core::SpecializationInfo(
				builder.addProgramByRef(toString(getName(), "_trainDecayHistory", staticparam),
					getShader(LayerShader::VectorEltwiseMultiply, Precision::Unknown)),
				Vector<core::SpecializationConstant>{
					core::SpecializationConstant(getPropagationSubpassBufferCount()), // nbuffers
					core::SpecializationConstant(staticparam),
					core::SpecializationConstant(staticparam),
					core::SpecializationConstant(_staticParams),
					core::SpecializationConstant(TV_MomentDecayRateVar),
				}));

		ret.multHistory = subpass.addComputePipeline(toString(getName(), "_trainHistoryAdd", staticparam), layout,
			core::SpecializationInfo(
				builder.addProgramByRef(toString(getName(), "_trainHistoryAdd", staticparam),
					getShader(LayerShader::VectorMultiplyAndAdd, Precision::Unknown)),
				Vector<core::SpecializationConstant>{
					core::SpecializationConstant(getPropagationSubpassBufferCount()), // nbuffers
					core::SpecializationConstant(staticparam),
					core::SpecializationConstant(staticparam),
					core::SpecializationConstant(diff),
					core::SpecializationConstant(_staticParams),
					core::SpecializationConstant(TV_RateVar),
				}));

		ret.add4 = subpass.addComputePipeline(toString(getName(), "_trainAdd4_", staticparam), layout,
			core::SpecializationInfo(
				builder.addProgramByRef(toString(getName(), "_trainAdd4_", staticparam),
					getShader(LayerShader::VectorAddFloat4, Precision::Unknown)),
				Vector<core::SpecializationConstant>{
					core::SpecializationConstant(getPropagationSubpassBufferCount()), // nbuffers
					core::SpecializationConstant(target),
					core::SpecializationConstant(target),
					core::SpecializationConstant(staticparam)
				}));

		ret.add1 = subpass.addComputePipeline(toString(getName(), "_trainAdd1_", staticparam), layout,
			core::SpecializationInfo(
				builder.addProgramByRef(toString(getName(), "_trainAdd1_", staticparam),
					getShader(LayerShader::VectorAddFloat1, Precision::Unknown)),
				Vector<core::SpecializationConstant>{
					core::SpecializationConstant(getPropagationSubpassBufferCount()), // nbuffers
					core::SpecializationConstant(target),
					core::SpecializationConstant(target),
					core::SpecializationConstant(staticparam)
				}));

		return ret;
	};

	auto trainWeights = initTrainPipelines(_staticWeightsHistoryIndex, _propWeightsDiff, _propWeightsIndex);
	auto trainTerms = initTrainPipelines(_staticTermsHistoryIndex, _propTermsDiff, _propTermsIndex);

	subpass.setCommandsCallback([this, backwardNeeded, layoutIndex = layout->index, matMul, matMulBorders,
								 reluDiff, learnMatMul, learnMatMulBorder, learnSum,
								 trainWeights, trainTerms]
								 (const core::SubpassData &subpass, core::FrameQueue &q, core::CommandBuffer &b) {
		auto &buf = static_cast<CommandBuffer &>(b);
		auto pass = static_cast<RenderPass *>(subpass.pass->impl.get());
		auto front = getFront();

		auto weightExtent = front->getWeightSize();
		auto outputExtent = front->getOutputExtent();

		auto makeFullBarrier = [&] (Buffer *b) {
			auto BufferAccessFlags = VkAccessFlags(core::AccessType::ShaderWrite | core::AccessType::ShaderRead);
			BufferMemoryBarrier barrier(b, BufferAccessFlags, BufferAccessFlags);
			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(&barrier, 1));
		};

		auto makeFullBarrier2 = [&] (Buffer *b1, Buffer *b2) {
			auto BufferAccessFlags = VkAccessFlags(core::AccessType::ShaderWrite | core::AccessType::ShaderRead);
			BufferMemoryBarrier barriers[] = {
					BufferMemoryBarrier(b1, BufferAccessFlags, BufferAccessFlags),
					BufferMemoryBarrier(b2, BufferAccessFlags, BufferAccessFlags)
			};
			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(barriers, 2));
		};

		auto makeFullBarrier4 = [&] (Buffer *b1, Buffer *b2, Buffer *b3, Buffer *b4) {
			auto BufferAccessFlags = VkAccessFlags(core::AccessType::ShaderWrite | core::AccessType::ShaderRead);
			BufferMemoryBarrier barriers[] = {
					BufferMemoryBarrier(b1, BufferAccessFlags, BufferAccessFlags),
					BufferMemoryBarrier(b2, BufferAccessFlags, BufferAccessFlags),
					BufferMemoryBarrier(b3, BufferAccessFlags, BufferAccessFlags),
					BufferMemoryBarrier(b4, BufferAccessFlags, BufferAccessFlags)
			};
			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(barriers, 4));
		};

		vk::BufferAttachmentHandle *propagationBuffer = nullptr;

		if (auto bufferAttachment = q.getAttachment(getPropagationAttachment())) {
			propagationBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
		}

		buf.cmdBindDescriptorSets(pass, layoutIndex);

		makeFullBarrier(propagationBuffer->getBuffers()[_propOutputDiff].buffer);

		if (front->getActivation() == Activation::RELU) {
			const int inputBufferSize = outputExtent.depth * front->getKernelSize();
			VectorReLUDiff(buf, static_cast<ComputePipeline *>(reluDiff->pipeline.get()), inputBufferSize, 0.0f);
			makeFullBarrier(propagationBuffer->getBuffers()[_propOutputDiff].buffer);
		}

		if (backwardNeeded) {
			const int secondWidth = weightExtent.width;
			const int firstHeight = outputExtent.depth;
			const int firstWidth = outputExtent.width;
			const int resultBufferSize = firstHeight * secondWidth;

			MultiplyMatrixByMatrix(buf, static_cast<ComputePipeline *>(matMul->pipeline.get()), static_cast<ComputePipeline *>(matMulBorders->pipeline.get()),
					1, firstHeight, firstWidth, secondWidth, resultBufferSize);

			makeFullBarrier(propagationBuffer->getBuffers()[_propTargetIndex].buffer);
			buf.cmdCopyBuffer(propagationBuffer->getBuffers()[_propTargetIndex].buffer, propagationBuffer->getBuffers()[_propFeedback].buffer);
			makeFullBarrier(propagationBuffer->getBuffers()[_propFeedback].buffer);
		}

		MultiplyTransposedMatrixByMatrix(buf, static_cast<ComputePipeline *>(learnMatMul->pipeline.get()),
				static_cast<ComputePipeline *>(learnMatMulBorder->pipeline.get()),
				outputExtent.depth, weightExtent.height, weightExtent.height,
				weightExtent.width, weightExtent.width, weightExtent.width,
				weightExtent.width * weightExtent.height);

		SumMatrixRows(buf, static_cast<ComputePipeline *>(learnSum->pipeline.get()),
				1, outputExtent.depth, weightExtent.height);

		auto weightsSize = front->getWeightBufferSize() / sizeof(float);
		auto termsSize = front->getKernelSize();

		VectorMultiply(buf, static_cast<ComputePipeline *>(trainWeights.decayHistory->pipeline.get()), weightsSize);
		VectorMultiply(buf, static_cast<ComputePipeline *>(trainTerms.decayHistory->pipeline.get()), termsSize);

		makeFullBarrier4(propagationBuffer->getBuffers()[_propWeightsDiff].buffer, propagationBuffer->getBuffers()[_propTermsDiff].buffer,
				propagationBuffer->getBuffers()[_staticWeightsHistoryIndex].buffer, propagationBuffer->getBuffers()[_staticTermsHistoryIndex].buffer);
		VectorMultiplyAndAdd(buf, static_cast<ComputePipeline *>(trainWeights.multHistory->pipeline.get()), weightsSize);
		VectorMultiplyAndAdd(buf, static_cast<ComputePipeline *>(trainTerms.multHistory->pipeline.get()), termsSize);

		makeFullBarrier2(propagationBuffer->getBuffers()[_staticWeightsHistoryIndex].buffer,
				propagationBuffer->getBuffers()[_staticTermsHistoryIndex].buffer);

		VectorAdd(buf, static_cast<ComputePipeline *>(trainWeights.add4->pipeline.get()),
				static_cast<ComputePipeline *>(trainWeights.add1->pipeline.get()), weightsSize);
		VectorAdd(buf, static_cast<ComputePipeline *>(trainTerms.add4->pipeline.get()),
				static_cast<ComputePipeline *>(trainTerms.add1->pipeline.get()), termsSize);

		makeFullBarrier2(propagationBuffer->getBuffers()[_propWeightsIndex].buffer,
				propagationBuffer->getBuffers()[_propTermsIndex].buffer);
	});

	queueBuilder.addCompleteCallback([this] (const core::QueuePassData &, FrameQueue &q, bool success) {
		vk::BufferAttachmentHandle *propagationBuffer = nullptr;
		if (auto bufferAttachment = q.getAttachment(getPropagationAttachment())) {
			propagationBuffer = (vk::BufferAttachmentHandle *)bufferAttachment->handle.get();
		}

		/*q.getFrame()->getLoop()->captureBuffer([front = _front] (const BufferInfo &info, BytesView view) {
			xenolith::shadernn::Model::saveBlob(
					filesystem::currentDir<Interface>(toString(front->getName(),".", front->getInputIndex(), ".weightsDiff.bin")).data(),
					view.data(), view.size());
			std::cout << "Save: " << toString(front->getName(),".", front->getInputIndex(), ".weightsDiff.bin") << "\n";
		}, propagationBuffer->getBuffers()[_propWeightsDiff].buffer.get());

		q.getFrame()->getLoop()->captureBuffer([front = _front] (const BufferInfo &info, BytesView view) {
			xenolith::shadernn::Model::saveBlob(
					filesystem::currentDir<Interface>(toString(front->getName(),".", front->getInputIndex(), ".termsDiff.bin")).data(),
					view.data(), view.size());
			std::cout << "Save: " << toString(front->getName(),".", front->getInputIndex(), ".termsDiff.bin") << "\n";
		}, propagationBuffer->getBuffers()[_propTermsDiff].buffer.get());

		q.getFrame()->getLoop()->captureBuffer([front = _front] (const BufferInfo &info, BytesView view) {
			xenolith::shadernn::Model::saveBlob(
					filesystem::currentDir<Interface>(toString(front->getName(),".", front->getInputIndex(), ".feedback.bin")).data(),
					view.data(), view.size());
			std::cout << "Save: " << toString(front->getName(),".", front->getInputIndex(), ".output.bin") << "\n";
		}, propagationBuffer->getBuffers()[_propFeedback].buffer.get());*/
	});

	_targetPropagationIdx = _propTargetIndex;
}

Vector<const core::BufferData *> MatrixMulLayer::getTrainableGradients(Queue::Builder &queueBuilder) const {
	auto weightsGradientBuffer = queueBuilder.addBuffer(toString(getName(), "_weightsGradient_buffer"),
			BufferInfo(_front->getWeightBufferSize(), core::BufferUsage::StorageBuffer, PassType::Compute),
			[] (uint8_t *buf, uint64_t size, const core::BufferData::DataCallback &cb) {
		FillFloatBuffer(buf, size, 0.0f);
	});

	auto freeTermsGradientBuffer = queueBuilder.addBuffer(toString(getName(), "_freeTermsGradient_buffer"),
			BufferInfo(_front->getKernelSize() * sizeof(float), core::BufferUsage::StorageBuffer, PassType::Compute),
			[] (uint8_t *buf, uint64_t size, const core::BufferData::DataCallback &cb) {
		FillFloatBuffer(buf, size, 0.0f);
	});

	return Vector<const core::BufferData *>{
		weightsGradientBuffer,
		freeTermsGradientBuffer
	};
}

}
