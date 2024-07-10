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

#ifndef SRC_LAYERS_XLSNNMATRIXMULLAYER_H_
#define SRC_LAYERS_XLSNNMATRIXMULLAYER_H_

#include "XLSnnLayer.h"

namespace stappler::xenolith::shadernn {

class MatrixMulLayer : public Layer {
public:
	virtual ~MatrixMulLayer() = default;

	virtual bool init(Model *, StringView tag, size_t idx, const Value&) override;

	virtual bool isTrainable() const override { return _model->isTrainable(); }

	virtual Extent3 getOutputExtent() const override {
		return Extent3(_kernelSize, 1, _batchSize);
	}

	virtual void setInputExtent(uint32_t index, Attachment *, Extent3 e) override;

	Extent3 getWeightSize() const {
		auto ret = Layer::getOutputExtent();
		return Extent3(ret.width, _kernelSize, 1);
	}

	size_t getWeightBufferSize() const {
		auto extent = getWeightSize();
		return extent.width * extent.height * extent.depth * sizeof(float);
	}

	virtual const core::QueuePassData *prepare(core::Queue::Builder &builder,
			Map<Layer *, const core::AttachmentData *> inputs,
			Map<Attachment *, const core::AttachmentData *> attachments) override;

	virtual const core::AttachmentData *makeOutputAttachment(core::Queue::Builder &builder, bool isGlobalOutput) override;

	Activation getActivation() const { return _activation; }
	uint32_t getKernelSize() const { return _kernelSize; }

	Layer *getInput() const { return _input; }

	void generateWeights(uint8_t *buf, uint64_t size, const core::BufferData::DataCallback &cb) const;
	void generateFreeTerms(uint8_t *buf, uint64_t size, const core::BufferData::DataCallback &cb) const;

protected:
	Activation _activation = Activation::RELU;
	uint32_t _kernelSize = 2;
	uint32_t _batchSize = 128;
	Layer *_input = nullptr;
};

}

#endif /* SRC_LAYERS_XLSNNMATRIXMULLAYER_H_ */
