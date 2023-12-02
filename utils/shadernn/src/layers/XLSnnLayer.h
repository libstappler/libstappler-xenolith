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

#ifndef SRC_LAYERS_XLSNNLAYER_H_
#define SRC_LAYERS_XLSNNLAYER_H_

#include "XLSnnModel.h"
#include "XLCoreQueue.h"

namespace stappler::xenolith::shadernn {

struct ModelSpecialization;

struct LayerInputInfo {
	uint32_t index;
	uint32_t layer;
    Extent3 extent;
    core::ImageFormat format;
    String name;
    Attachment *attachment;
};

// This structure describes image shape transformation
struct LayerTransformInfo {
	bool isFixed = 0;
	union {
		struct {
			float scaleWidth;
			float scaleHeight;
			float translateWidth;
			float translateHeight;
		};
		struct {
			uint32_t fixedWidth;
			uint32_t fixedHeight;
			uint32_t fixedDepth;
			uint32_t fixedBatch;
		};
	};

	static constexpr LayerTransformInfo identity() {
		return {0, { {1.0f, 1.0f, 0.0f, 0.0f}}};
	}
};

class Layer : public Ref {
public:
	virtual ~Layer() = default;

	virtual bool init(Model *, StringView tag, size_t idx, const Value &);

	virtual void setInputExtent(uint32_t index, Attachment *, Extent3 e);

	virtual void setOutput(Attachment *a) { _output = a; }
	virtual Attachment *getOutput() const { return _output; }

	virtual bool isInputDefined() const;

	virtual Extent3 getOutputExtent() const;

	virtual Extent3 getOutputExtent(const ModelSpecialization &) const;

	virtual LayerTransformInfo getOutputTransform() const {
		return LayerTransformInfo::identity();
	}

	uint32_t getInputIndex() const { return _inputIndex; }

	bool isInput() const { return _isInputLayer; }

	SpanView<LayerInputInfo> getInputs() const { return _inputs; }

	StringView getName() const { return _name; }
	StringView getTag() const { return _tag; }

	uint32_t getNumOutputPlanes() const { return _numOutputPlanes; }
	uint32_t getNumInputPlanes() const { return _numInputPlanes; }

	virtual const core::QueuePassData *prepare(core::Queue::Builder &builder,
			Map<Layer *, const core::AttachmentData *> inputs,
			Map<Attachment *, const core::AttachmentData *> attachments) {
		return nullptr;
	}

protected:
	Model *_model = nullptr;
	String _tag;
	String _name;

	bool _isInputLayer = false;

	uint32_t _numOutputPlanes = 0;
	uint32_t _numInputPlanes = 0;
	uint32_t _kernelSize = 0; // move here for layer name

	// The index of this layer in all layers array
	uint32_t _inputIndex = 0;

	Vector<LayerInputInfo> _inputs;

	Attachment *_output = nullptr;
};

}

#endif /* SRC_LAYERS_XLSNNLAYER_H_ */
