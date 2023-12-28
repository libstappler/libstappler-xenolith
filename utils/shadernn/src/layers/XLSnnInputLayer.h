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

#ifndef SRC_LAYERS_XLSNNINPUTLAYER_H_
#define SRC_LAYERS_XLSNNINPUTLAYER_H_

#include "XLSnnLayer.h"

namespace stappler::xenolith::shadernn {

class InputLayer : public Layer {
public:
	virtual ~InputLayer() = default;

	virtual bool init(Model *, StringView tag, size_t idx, const Value&) override;

	virtual Extent3 getOutputExtent() const override {
		return Extent3(_inputWidth, _inputHeight, (_inputChannels + 3) / 4);
	}

	virtual const core::QueuePassData *prepare(core::Queue::Builder &builder,
			Map<Layer *, const core::AttachmentData *> inputs,
			Map<Attachment *, const core::AttachmentData *> attachments) override;

	virtual const core::AttachmentData *makeInputAttachment(core::Queue::Builder &builder) override;
	virtual const core::AttachmentData *makeOutputAttachment(core::Queue::Builder &builder, bool isGlobalOutput) override;

protected:
	uint32_t _inputWidth = 1;
	uint32_t _inputHeight = 1;
	uint32_t _inputChannels = 1;
};

class InputBufferLayer : public Layer {
public:
	virtual ~InputBufferLayer() = default;

	virtual bool init(Model *, StringView tag, size_t idx, const Value&) override;

	virtual Extent3 getOutputExtent() const override {
		return Extent3(_inputWidth * _inputHeight, 1, _inputObjects);
	}

	virtual const core::QueuePassData *prepare(core::Queue::Builder &builder,
			Map<Layer *, const core::AttachmentData *> inputs,
			Map<Attachment *, const core::AttachmentData *> attachments) override;

	virtual const core::AttachmentData *makeInputAttachment(core::Queue::Builder &builder) override;
	virtual const core::AttachmentData *makeOutputAttachment(core::Queue::Builder &builder, bool isGlobalOutput) override;

	float getNorm() const { return _norm; }
	float getMean() const { return _mean; }

	uint32_t getBufferSize() const { return _inputWidth * _inputHeight * _inputObjects; }

protected:
	uint32_t _inputWidth = 1;
	uint32_t _inputHeight = 1;
	uint32_t _inputObjects = 1;

	float _norm = 1.0f;
	float _mean = 0.0f;
};

class InputCsvIntLayer : public Layer {
public:
	struct NormData {
		uint64_t offset;
		uint64_t norm;
	};

	virtual ~InputCsvIntLayer() = default;

	virtual bool init(Model *, StringView tag, size_t idx, const Value&) override;

	virtual Extent3 getOutputExtent() const override {
		return Extent3(_fields.size(), _inputObjects, 1);
	}

	SpanView<NormData> getNormData() const { return _norm; }
	BytesView getNormDataBuffer() const { return BytesView((const uint8_t *)_norm.data(), _norm.size() * sizeof(NormData)); }

	SpanView<uint32_t> getFields() const { return _fields; }

	virtual const core::QueuePassData *prepare(core::Queue::Builder &builder,
			Map<Layer *, const core::AttachmentData *> inputs,
			Map<Attachment *, const core::AttachmentData *> attachments) override;

	virtual const core::AttachmentData *makeInputAttachment(core::Queue::Builder &builder) override;
	virtual const core::AttachmentData *makeOutputAttachment(core::Queue::Builder &builder, bool isGlobalOutput) override;

protected:
	uint32_t _inputObjects = 1;
	Vector<uint32_t> _fields;
	Vector<NormData> _norm;
};

}

#endif /* SRC_LAYERS_XLSNNINPUTLAYER_H_ */
