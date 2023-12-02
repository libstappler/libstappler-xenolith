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

#ifndef SRC_LAYERS_XLSNNCONVLAYER_H_
#define SRC_LAYERS_XLSNNCONVLAYER_H_

#include "XLSnnLayer.h"

namespace stappler::xenolith::shadernn {

struct MatVec {
	Extent2 extent;
	Vector<float> data;

	MatVec(Extent2 e) : extent(e) {
		data.resize(e.width * e.height, 0.0f);
	}

	void set(uint32_t i, uint32_t j, float value) {
		(data.data() + i * extent.width)[j] = value;
	}

	float at(int i0) const {
		return data[i0];
	}
};

class ConvLayer : public Layer {
public:
	virtual ~ConvLayer() = default;

	uint32_t getActivation() const { return toInt(_activation); }
	uint32_t getKernelSize() const { return _kernelSize; }
	uint32_t getStride() const { return _stride; }

	Extent3 getKernelExtent() const;

	bool useBias() const { return _biases.size() > 0; }

	BytesView getKernelImageData() const;
	BytesView getBiasBufferData() const;

protected:
	Vector<MatVec> _weightsCvM;
	Vector<float> _weightsData;
	Vector<uint16_t> _weightsDataF16;
	Vector<float> _biases;
	Activation _activation;
	uint32_t _kernelSize = 0;
	uint32_t _stride = 0;
};

class Conv2DLayer : public ConvLayer {
public:
	static bool oihw2hwo4i4(const Vector<MatVec> &inputWeights, Vector<float> &outVec, int inChannels, int outChannels,
			int fw, int fh, int unit = 4);

	struct BatchNormalization {
		Vector<float> beta;
		Vector<float> gamma;
		Vector<float> mean;
		Vector<float> variance;
	};

	virtual ~Conv2DLayer() = default;

	virtual bool init(Model *, StringView tag, size_t idx, const Value &) override;

	virtual LayerTransformInfo getOutputTransform() const override;

	UVec4 getPaddingOffset() const;

	StringView getPaddingMode() const { return _paddingMode; }

	bool useBatchNormalization() const { return _useBatchNormalization; }

	float getLeakyReluAlpha() const { return _leakyReluAlpha; }

	BytesView getNormBetaBufferData() const;
	BytesView getNormGammaBufferData() const;
	BytesView getNormMeanBufferData() const;
	BytesView getNormVarianceBufferData() const;

	virtual const core::QueuePassData *prepare(core::Queue::Builder &builder,
			Map<Layer *, const core::AttachmentData *> inputs,
			Map<Attachment *, const core::AttachmentData *> attachments) override;

protected:
	BatchNormalization _batchNormalization;
	bool _useBatchNormalization = false;
	bool _useMultiInputs = false;
	float _leakyReluAlpha = 0.0f;
	bool _useUniformShaders = true;
	uint32_t _paddingT = 0;
	uint32_t _paddingB = 0;
	uint32_t _paddingL = 0;
	uint32_t _paddingR = 0;
	String _paddingValue;
	String _paddingMode = "constant";
};

}

#endif /* SRC_LAYERS_XLSNNCONVLAYER_H_ */
