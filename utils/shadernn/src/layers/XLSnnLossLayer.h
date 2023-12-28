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

#ifndef SRC_LAYERS_XLSNNLOSSLAYER_H_
#define SRC_LAYERS_XLSNNLOSSLAYER_H_

#include "XLSnnLayer.h"

namespace stappler::xenolith::shadernn {

class LossLayer : public Layer {
public:
	enum ParameterIndex {
		P_LossWeight = 0, // the weight for the loss function
		P_Loss, // the loss value on the last step
		P_LossDivider, // the averaging factor for calculating the loss value
		P_LossGradientDivider, // the averaging factor for calculating the loss gradient (takes lossWeight into account)
		P_MinGradient,
		P_MaxGradient,
		P_Count
	};

	SpanView<float> getParameters() const { return _params; }
	BytesView getParametersBuffer() const { return BytesView((const uint8_t *)_params.data(), _params.size() * sizeof(float)); }
	void synchronizeParameters(SpanView<float>);

	void setParameter(ParameterIndex, float);
	float getParameter(ParameterIndex) const;

protected:
	std::array<float, P_Count> _params;
};

class CrossEntropyLossLayer : public LossLayer {
public:
	virtual ~CrossEntropyLossLayer() = default;

	virtual bool init(Model *, StringView tag, size_t idx, const Value&) override;

	virtual void setInputExtent(uint32_t index, Attachment *, Extent3 e) override;

	virtual const core::QueuePassData *prepare(core::Queue::Builder &builder,
			Map<Layer *, const core::AttachmentData *> inputs,
			Map<Attachment *, const core::AttachmentData *> attachments) override;

	virtual const core::AttachmentData *makeOutputAttachment(core::Queue::Builder &builder, bool isGlobalOutput) override;

	Layer *getInputLabels() const { return _inputLabels; }
	uint32_t getBatchSize() const { return _batchSize; }
	uint32_t getClassesCount() const { return _classesCount; }

	uint32_t getWeightBufferSize() const { return sizeof(float) * _batchSize; }
	uint32_t getResultBufferSize() const { return sizeof(float) * _batchSize; }
	uint32_t getLossGradientBufferSize() const { return sizeof(float) * _batchSize * _classesCount; }

protected:
	uint32_t _batchSize = 100;
	uint32_t _classesCount = 10;
	String _labelsInputName;
	Layer *_inputLabels = nullptr;
	Layer *_inputNetwork = nullptr;
};

}

#endif /* SRC_LAYERS_XLSNNLOSSLAYER_H_ */
