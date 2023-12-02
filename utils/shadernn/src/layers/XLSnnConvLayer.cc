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

#include "XLSnnConvLayer.h"
#include "XLSnnVkConvLayer.h"

namespace stappler::xenolith::shadernn {

// one vec4 for shader
static Vector<float> s_InvalidVector{ 0.0f, 0.0f, 0.0f, 0.0f };

Extent3 ConvLayer::getKernelExtent() const {
	uint32_t unit = 4;
	uint32_t ic_4 = UP_DIV(_numInputPlanes, unit);
	uint32_t oc_4 = UP_DIV(_numOutputPlanes, unit);

	return Extent3(ic_4 * unit, oc_4, (uint32_t) (_kernelSize * _kernelSize));
}

BytesView ConvLayer::getKernelImageData() const {
	if (_model->isHalfPrecision()) {
		return BytesView((const uint8_t *)_weightsDataF16.data(), _weightsDataF16.size() * sizeof(uint16_t));
	} else {
		return BytesView((const uint8_t *)_weightsData.data(), _weightsData.size() * sizeof(float));
	}
}

BytesView ConvLayer::getBiasBufferData() const {
	if (!_biases.empty()) {
		return BytesView((const uint8_t *)_biases.data(), _biases.size() * sizeof(float));
	}
	return BytesView((const uint8_t *)s_InvalidVector.data(), s_InvalidVector.size() * sizeof(float));
}

bool Conv2DLayer::oihw2hwo4i4(const Vector<MatVec>& inputWeights, Vector<float>& outVec, int inChannels, int outChannels, int fw, int fh, int unit) {
	int alignedWeightSize = ROUND_UP(outChannels, unit) * fw * fh * ROUND_UP(inChannels, unit);

	// SNN_LOGD("inChannels = %d, outChannels = %d, fw = %d, fh = %d, all: %d", inChannels, outChannels, fw, fh, alignedWeightSize);

	outVec.clear();
	outVec.resize(alignedWeightSize);
	float *out = (float*) outVec.data();
	int planeSize = ROUND_UP(outChannels, unit) * ROUND_UP(inChannels, unit);
	memset(out, 0, alignedWeightSize * sizeof(float));
	for (int b = 0; b < outChannels; ++b) {
		int b_4 = b / unit;
		int mx = b % unit;
		for (int d = 0; d < inChannels; ++d) {
			for (int y = 0; y < fh; ++y) {
				for (int x = 0; x < fw; ++x) {
					int base = (y * fw + x) * planeSize;
					int inSize = ROUND_UP(inChannels, unit) * unit;
					out[base + inSize * b_4 + d * unit + mx] = inputWeights[b * inChannels + d].at(y * fw + x);
				}
			}
		}
	}
	return 0;
}

bool Conv2DLayer::init(Model *m, StringView tag, size_t idx, const Value &data) {
	if (!ConvLayer::init(m, tag, idx, data)) {
		return false;
	}

	_activation = getActivationValue(data.getString("activation"));
    _kernelSize = data.getInteger("kernel_size");
    _stride = data.getInteger("strides");

	if (data.isArray("padding")) {
		auto &arr = data.getValue("padding");
        auto &upPadding = arr.getValue(0);
        auto &sidePadding = arr.getValue(1);

        if (upPadding.isArray()) {
    		_paddingT = upPadding.getInteger(0);
    		_paddingB = upPadding.getInteger(1);
        } else {
        	_paddingT = _paddingB = upPadding.getInteger();
        }
        if (sidePadding.isArray()) {
    		_paddingL = sidePadding.getInteger(0);
    		_paddingR = sidePadding.getInteger(1);
        } else {
        	_paddingL = _paddingR = sidePadding.getInteger();
        }

		_paddingMode = data.getString("mode");
	} else if (data.isString("padding")) {
		_paddingValue = data.getString("padding");
	}

	if (data.hasValue("use_multi_inputs")) {
		_useMultiInputs = data.getString("use_multi_inputs") == "True";
	} else {
		_useMultiInputs = false;
	}

	auto readFloatValue = [&] (Value::ArrayType::const_iterator &arrIt) -> float {
		auto value = static_cast<float>((arrIt++)->getDouble());
		if (_model->isHalfPrecision()) {
			value = convertToMediumPrecision(value);
		}
		return value;
	};

    auto & weightObj = data.getValue("weights");

    _weightsCvM = Vector<MatVec>(_numInputPlanes * _numOutputPlanes, MatVec(Extent2(_kernelSize, _kernelSize)));
    auto matrixIt = _weightsCvM.begin();
	if (_model->usesDataFile()) {
		for (uint32_t i = 0; i < _numOutputPlanes; ++i) {
			for (uint32_t j = 0; j < _numInputPlanes; ++j) {
				for (uint32_t writingRow = 0; writingRow < _kernelSize; ++writingRow) {
					for (uint32_t writingCol = 0; writingCol < _kernelSize; ++writingCol) {
						matrixIt->set(writingRow, writingCol, _model->readFloatData());
					}
				}
				matrixIt++;
			}
		}
	} else {
		auto &arr = weightObj.getArray("kernel");
		auto arrIt = arr.begin();
		for (uint32_t i = 0; i < _numOutputPlanes; ++i) {
			for (uint32_t j = 0; j < _numInputPlanes; ++j) {
				for (uint32_t writingRow = 0; writingRow < _kernelSize; ++writingRow) {
					for (uint32_t writingCol = 0; writingCol < _kernelSize; ++writingCol) {
						auto value = readFloatValue(arrIt);
						matrixIt->set(writingRow, writingCol, value);
					}
				}
				matrixIt++;
			}
		}
	}

	oihw2hwo4i4(_weightsCvM, _weightsData, _numInputPlanes, _numOutputPlanes, _kernelSize, _kernelSize);

	if (_model->isHalfPrecision()) {
		_weightsDataF16.reserve(_weightsData.size());
		for (auto &it : _weightsData) {
			_weightsDataF16.emplace_back(halffloat::encode(it));
		}
	}

	_biases.resize(_numOutputPlanes, 0.0);
	if (data.getString("useBias") == "True") {
		if (_model->usesDataFile()) {
			for (uint32_t i = 0; i < _numOutputPlanes; i++) {
				_biases[i] = _model->readFloatData();
			}
		} else {
			auto &arr = weightObj.getArray("bias");
			auto arrIt = arr.begin();
			for (uint32_t i = 0; i < _numOutputPlanes; i++) {
				_biases[i] = readFloatValue(arrIt);
			}
		}
	}

    _useBatchNormalization = (data.getString("useBatchNormalization") == "True") ? true : false;
	if (_useBatchNormalization) {
		_batchNormalization.gamma.resize(_numOutputPlanes, 0.0f);
		_batchNormalization.beta.resize(_numOutputPlanes, 0.0f);
		_batchNormalization.mean.resize(_numOutputPlanes, 0.0f);
		_batchNormalization.variance.resize(_numOutputPlanes, 0.0f);

		auto &batchNormObj = data.getValue("batchNormalization");
		if (_model->usesDataFile()) {
			for (uint32_t i = 0; i < _numOutputPlanes; i++) {
				_batchNormalization.gamma[i] = _model->readFloatData();
			}
			for (uint32_t i = 0; i < _numOutputPlanes; i++) {
				_batchNormalization.beta[i] = _model->readFloatData();
			}
			for (uint32_t i = 0; i < _numOutputPlanes; i++) {
				_batchNormalization.mean[i] = _model->readFloatData();
			}
			for (uint32_t i = 0; i < _numOutputPlanes; i++) {
				_batchNormalization.variance[i] = _model->readFloatData();
			}
		} else {
			auto betaArray = batchNormObj.getArray("beta").begin();
			auto gammaArray = batchNormObj.getArray("gamma").begin();
			auto movingMean = batchNormObj.getArray(batchNormObj.hasValue("moving_mean") ? "moving_mean" : "movingMean").begin();
			auto movingVariance = batchNormObj.getArray(batchNormObj.hasValue("moving_variance") ? "moving_variance" : "movingVariance").begin();

			for (uint32_t i = 0; i < _numOutputPlanes; i++) {
				_batchNormalization.beta[i] = readFloatValue(betaArray);
				_batchNormalization.gamma[i] = readFloatValue(gammaArray);
				_batchNormalization.mean[i] = readFloatValue(movingMean);
				_batchNormalization.variance[i] = readFloatValue(movingVariance);
			}
		}
	}

	if (_activation == Activation::LEAKYRELU) {
		if (data.hasValue("leakyReluAlpha")) {
			_leakyReluAlpha = data.getDouble("leakyReluAlpha");
		} else {
			_leakyReluAlpha = data.getDouble("alpha");
		}
		if (_model->isHalfPrecision()) {
			_leakyReluAlpha = convertToMediumPrecision(_leakyReluAlpha);
		}
	}

	return true;
}

LayerTransformInfo Conv2DLayer::getOutputTransform() const {
	auto offset = getPaddingOffset();
	float scale = 1 / static_cast<float>(_stride);
	float translation = 0.0f;
	if (_kernelSize % 2 != 0) {
		translation = 1 + (static_cast<float>(offset.x + offset.y) - static_cast<float>(_kernelSize)) / static_cast<float>(_stride);
	} else {
		translation = 1 + (static_cast<float>(offset.x + offset.y - 1) - static_cast<float>(_kernelSize)) / static_cast<float>(_stride);
	}
	return {0, { {scale, scale, translation, translation}}};
}

BytesView Conv2DLayer::getNormBetaBufferData() const {
	if (!_batchNormalization.beta.empty()) {
		return BytesView((const uint8_t *)_batchNormalization.beta.data(), _batchNormalization.beta.size() * sizeof(float));
	}
	return BytesView((const uint8_t *)s_InvalidVector.data(), s_InvalidVector.size() * sizeof(float));
}

BytesView Conv2DLayer::getNormGammaBufferData() const {
	if (!_batchNormalization.gamma.empty()) {
		return BytesView((const uint8_t *)_batchNormalization.gamma.data(), _batchNormalization.gamma.size() * sizeof(float));
	}
	return BytesView((const uint8_t *)s_InvalidVector.data(), s_InvalidVector.size() * sizeof(float));
}

BytesView Conv2DLayer::getNormMeanBufferData() const {
	if (!_batchNormalization.mean.empty()) {
		return BytesView((const uint8_t *)_batchNormalization.mean.data(), _batchNormalization.mean.size() * sizeof(float));
	}
	return BytesView((const uint8_t *)s_InvalidVector.data(), s_InvalidVector.size() * sizeof(float));
}

BytesView Conv2DLayer::getNormVarianceBufferData() const {
	if (!_batchNormalization.variance.empty()) {
		return BytesView((const uint8_t *)_batchNormalization.variance.data(), _batchNormalization.variance.size() * sizeof(float));
	}
	return BytesView((const uint8_t *)s_InvalidVector.data(), s_InvalidVector.size() * sizeof(float));
}

UVec4 Conv2DLayer::getPaddingOffset() const {
	UVec4 offsets;
	if (_paddingValue.empty()) {
		offsets.x = _paddingT;
		offsets.y = _paddingB;
		offsets.z = _paddingL;
		offsets.w = _paddingR;
	} else {
		if (_paddingValue == "valid" || _paddingValue == "none") {
			offsets.x = 0;
			offsets.y = 0;
			offsets.z = 0;
			offsets.w = 0;
		} else {
			if (_kernelSize > 1) {
				offsets.x = std::max(static_cast<uint32_t>(_kernelSize / 2), (uint32_t) 1);
				offsets.y = std::max(static_cast<uint32_t>(_kernelSize / 2), (uint32_t) 1);
				offsets.z = std::max(static_cast<uint32_t>(_kernelSize / 2), (uint32_t) 1);
				offsets.w = std::max(static_cast<uint32_t>(_kernelSize / 2), (uint32_t) 1);
				if (_kernelSize % 2 == 0) {
					offsets.x = offsets.x - 1;
					offsets.z = offsets.z - 1;
				}
			} else {
				offsets.x = 0;
				offsets.y = 0;
				offsets.z = 0;
				offsets.w = 0;
			}
		}
	}
	return offsets;
}

const core::QueuePassData *Conv2DLayer::prepare(core::Queue::Builder &builder,
		Map<Layer *, const core::AttachmentData *> inputs,
		Map<Attachment *, const core::AttachmentData *> attachments) {

	auto inputIt = attachments.find(_inputs.front().attachment);
	auto outputIt = attachments.find(getOutput());

	if (inputIt == attachments.end() || outputIt == attachments.end()) {
		log::error("snn::InputLayer", "No attachments specified");
		return nullptr;
	}

	return builder.addPass(getName(), core::PassType::Compute, core::RenderOrdering(_inputIndex),
			[&] (core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<vk::shadernn::Conv2DLayer>::create(builder, passBuilder, this, inputIt->second, outputIt->second);
	});
}

}
