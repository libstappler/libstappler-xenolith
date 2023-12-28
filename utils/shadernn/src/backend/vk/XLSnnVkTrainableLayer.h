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

#ifndef SRC_BACKEND_VK_XLSNNVKTRAINABLELAYER_H_
#define SRC_BACKEND_VK_XLSNNVKTRAINABLELAYER_H_

#include "XLVkQueuePass.h"
#include "XLVkAttachment.h"

namespace stappler::xenolith::vk::shadernn {

class TrainableLayer : public vk::QueuePass {
public:
	enum VariableIndex {
		TV_MomentDecayRateVar = 0,
		TV_OpMomentDecayRateVar,
		TV_OpRegL2MomentDecayRateVar,
		TV_RateVar,
		TV_L1Threshold,
		TV_L1Mult,
		TV_Count
	};

	virtual ~TrainableLayer();

	virtual void initPropagation(Queue::Builder &queueBuilder, QueuePassBuilder &,
			const core::AttachmentData *source, uint32_t bufferIndex);

	virtual void initPropagationSubpass(Queue::Builder &queueBuilder, core::QueuePassBuilder &,
			core::SubpassBuilder &, const core::PipelineLayoutData *);

	virtual uint32_t getPropagationSubpassBufferCount() const { return _fullPropagationBuffers; }

	virtual Vector<const core::BufferData *> getTrainableGradients(Queue::Builder &queueBuilder) const;

	virtual bool isBackwardNeeded() const;

	const AttachmentData *getInputAttachment() const { return _inputAttachment; }
	const AttachmentData *getOutputAttachment() const { return _outputAttachment; }
	const AttachmentData *getWeightsAttachment() const { return _weightsAttachment; }
	const AttachmentData *getPropagationAttachment() const { return _propagationAttachment; }
	uint32_t getTargetPropagationBufferIdx() const { return _targetPropagationIdx; }

	const AttachmentData *getExternalPropagationDataSource() const { return _externalPropagationDataSource; }
	uint32_t getExternalPropagationBufferIdx() const { return _externalPropagationBufferIdx; }

protected:
	using QueuePass::init;

	const AttachmentData *_inputAttachment = nullptr;
	const AttachmentData *_outputAttachment = nullptr;
	const AttachmentData *_weightsAttachment = nullptr;

	const AttachmentData *_propagationAttachment = nullptr;
	uint32_t _targetPropagationIdx = 2;

	const AttachmentData *_externalPropagationDataSource = nullptr;
	uint32_t _externalPropagationBufferIdx = 0;

	uint32_t _staticPropagationBuffers = 1;
	uint32_t _fullPropagationBuffers = 2;

	float _momentDecayRate = 0.9f;
	float _learningRate = 0.01f;
	float _regularizationL2 = 0.f;
	float _regularizationL1 = 0.f;
	float _maxGradientNorm = -1.f;
};

}

#endif /* SRC_BACKEND_VK_XLSNNVKTRAINABLELAYER_H_ */
