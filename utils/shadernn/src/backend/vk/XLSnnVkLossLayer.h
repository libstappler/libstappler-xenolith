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

#ifndef SRC_BACKEND_VK_XLSNNVKLOSSLAYER_H_
#define SRC_BACKEND_VK_XLSNNVKLOSSLAYER_H_

#include "XLVkRenderPass.h"
#include "XLVkQueuePass.h"
#include "XLVkAttachment.h"
#include "XLSnnLossLayer.h"

namespace stappler::xenolith::vk::shadernn {

class CrossEntropyLossLayer : public vk::QueuePass {
public:
	using Front = xenolith::shadernn::CrossEntropyLossLayer;

	using BufferView = BufferAttachmentHandle::BufferView;

	using PipelineOpFn = Function<void(Front *, CommandBuffer &, ComputePipeline *, SpanView<BufferView>)>;

	enum class PipelineOpIndex {
		MatrixSoftmaxByRows, // softmax to activation
		VectorNegLog, // transform activation
		VectorEltwiseMultiply, // multiply activation on labels
		SumMatrixColumnsToResult, // sum activation columns to result
		VectorSub, // compute diff fro activation to label
		SumMatrixColumnsLabels, // sum labels
		MultiplyDiagMatrixByMatrix, // calc gradient
		VectorDotProduct, // loss value function
		MultiplyDiagMatrixByMatrixForInput // prepare error propagation
	};

	struct PipelineOp {
		PipelineOpIndex idx;
		const core::ComputePipelineData *pipeline;
		PipelineOpFn command;

		PipelineOp(PipelineOpIndex i, const core::ComputePipelineData *p, PipelineOpFn &&f)
		: idx(i), pipeline(p), command(sp::move(f)) { };
	};

	static constexpr uint32_t DescriptorArraySize = 8;
	static constexpr uint32_t ParamsIdx = 0;
	static constexpr uint32_t WeightsIdx = 1;
	static constexpr uint32_t LossValueIdx = 2;
	static constexpr uint32_t LossGradientIdx = 3;
	static constexpr uint32_t InputNetworkIdx = 4;
	static constexpr uint32_t InputLabelsIdx = 5;
	static constexpr uint32_t ActivationIdx = 6;
	static constexpr uint32_t ActivationEltwiseMulIdx = 7;

	virtual ~CrossEntropyLossLayer();

	virtual bool init(Queue::Builder &queueBuilder, QueuePassBuilder &, Front *,
			const AttachmentData *inputLabels, const AttachmentData *inputNetwork, const AttachmentData *output);

	void initPropagation(Queue::Builder &queueBuilder, QueuePassBuilder &);

	void runAll(CommandBuffer &, SpanView<BufferView>);

	const AttachmentData *getInputLabelsAttachment() const { return _inputLabelsAttachment; }
	const AttachmentData *getInputNetworkAttachment() const { return _inputNetworkAttachment; }
	const AttachmentData *getWeightsAttachment() const { return _weightAttachment; }
	const AttachmentData *getOutputAttachment() const { return _outputAttachment; }

	const Front *getFront() const { return _front; }

protected:
	using QueuePass::init;

	const AttachmentData *_inputLabelsAttachment = nullptr;
	const AttachmentData *_inputNetworkAttachment = nullptr;
	const AttachmentData *_weightAttachment = nullptr;
	const AttachmentData *_outputAttachment = nullptr;

	Rc<Front> _front;
	Map<PipelineOpIndex, PipelineOp> _pipelineOps;
};

}

#endif /* SRC_BACKEND_VK_XLSNNVKLOSSLAYER_H_ */
