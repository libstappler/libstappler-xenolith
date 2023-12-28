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

#ifndef SRC_LAYERS_XLSNNSTATPERCENTLAYER_H_
#define SRC_LAYERS_XLSNNSTATPERCENTLAYER_H_

#include "XLSnnLayer.h"

namespace stappler::xenolith::shadernn {

class StatPercentLayer : public Layer {
public:
	virtual ~StatPercentLayer() = default;

	virtual bool init(Model *, StringView tag, size_t idx, const Value&) override;

	virtual Extent3 getOutputExtent() const override {
		return Extent3(4, _classCount, 1);
	}

	virtual const core::QueuePassData *prepare(core::Queue::Builder &builder,
			Map<Layer *, const core::AttachmentData *> inputs,
			Map<Attachment *, const core::AttachmentData *> attachments) override;

	virtual const core::AttachmentData *makeOutputAttachment(core::Queue::Builder &builder, bool isGlobalOutput) override;

	uint32_t getFieldClass() const { return _fieldClass; }
	uint32_t getFieldSource() const { return _fieldSource; }
	uint32_t getFieldTarget() const { return _fieldTarget; }
	uint32_t getClassMin() const { return _classMin; }
	uint32_t getClassCount() const { return _classCount; }

protected:
	uint32_t _fieldClass = 0;
	uint32_t _fieldSource = 1;
	uint32_t _fieldTarget = 2;
	uint32_t _classMin = 0;
	uint32_t _classCount = 100;
};

class StatAnalysisLayer : public Layer {
public:
	virtual ~StatAnalysisLayer() = default;

	virtual bool init(Model *, StringView tag, size_t idx, const Value&) override;

	virtual void setInputExtent(uint32_t index, Attachment *, Extent3 e) override;

	virtual Extent3 getOutputExtent() const override {
		return Extent3(4, 1, 1);
	}

	virtual const core::QueuePassData *prepare(core::Queue::Builder &builder,
			Map<Layer *, const core::AttachmentData *> inputs,
			Map<Attachment *, const core::AttachmentData *> attachments) override;

	virtual const core::AttachmentData *makeOutputAttachment(core::Queue::Builder &builder, bool isGlobalOutput) override;

	uint32_t getFieldClass() const { return _percent->getFieldClass(); }
	uint32_t getFieldSource() const { return _percent->getFieldSource(); }
	uint32_t getFieldTarget() const { return _percent->getFieldTarget(); }
	uint32_t getClassMin() const { return _percent->getClassMin(); }
	uint32_t getClassCount() const { return _percent->getClassCount(); }
	float getThreshold() const { return _threshold; }

	StatPercentLayer *getPercentLayer() const { return _percent; }

protected:
	StatPercentLayer *_percent = nullptr;
	float _threshold = 1;
};

}

#endif /* SRC_LAYERS_XLSNNSTATPERCENTLAYER_H_ */
