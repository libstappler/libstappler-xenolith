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

#ifndef SRC_BACKEND_VK_XLSNNVKMATRIXMULLAYER_H_
#define SRC_BACKEND_VK_XLSNNVKMATRIXMULLAYER_H_

#include "XLVkRenderPass.h"
#include "XLSnnVkTrainableLayer.h"
#include "XLSnnMatrixMulLayer.h"

namespace stappler::xenolith::vk::shadernn {

class MatrixMulLayer : public TrainableLayer {
public:
	using Front = xenolith::shadernn::MatrixMulLayer;

	virtual ~MatrixMulLayer();

	virtual bool init(Queue::Builder &queueBuilder, QueuePassBuilder &, Front *,
			const AttachmentData *input, const AttachmentData *output);

	virtual void initPropagationSubpass(core::Queue::Builder &builder, core::QueuePassBuilder &, core::SubpassBuilder &subpass,
			 const core::PipelineLayoutData *layout) override;

	virtual Vector<const core::BufferData *> getTrainableGradients(Queue::Builder &queueBuilder) const;

	const Front *getFront() const { return _front; }

	virtual uint32_t getPropagationSubpassBufferCount() const override { return 12; }
protected:
	using QueuePass::init;

	Rc<Front> _front;

	uint32_t _nbuffers = 4;
	uint32_t _weightsBufferIndex = 0;
	uint32_t _freeTermBufferIndex = 1;
	uint32_t _inputBufferIndex = 2;
	uint32_t _outputBufferIndex = 3;

	uint32_t _staticParams = 0;
	uint32_t _staticWeightsHistoryIndex = 1;
	uint32_t _staticTermsHistoryIndex = 2;

	uint32_t _propWeightsIndex = 0;
	uint32_t _propTermsIndex = 0;
	uint32_t _propOriginalOutput = 0;
	uint32_t _propOriginalInput = 0;
	uint32_t _propOutputDiff = 0;
	uint32_t _propTargetIndex = 0;
	uint32_t _propWeightsDiff = 0;
	uint32_t _propTermsDiff = 0;
	uint32_t _propFeedback = 0;
};

}

#endif /* SRC_BACKEND_VK_XLSNNVKMATRIXMULLAYER_H_ */
