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

#include "XLSnnModel.h"
#include "XLSnnLossLayer.h"
#include "XLSnnAttachment.h"

namespace stappler::xenolith::shadernn {

void Model::saveBlob(const char *path, const uint8_t *buf, size_t size) {
	::unlink(path);

	auto f = fopen(path, "w");
	::fwrite(&size, sizeof(size), 1, f);
	::fwrite(buf, size, 1, f);
	::fclose(f);
}

void Model::loadBlob(const char *path, const std::function<void(const uint8_t *, size_t)> &cb) {
	auto f = fopen(path, "r");
	if (!f) {
		return;
	}

	size_t size = 0;
	::fread(&size, sizeof(size), 1, f);

	auto buf = new uint8_t[size];
	if (size > 0) {
		::fread(buf, size, 1, f);

		cb(buf, size);
	}

	::fclose(f);
	delete[] buf;
}

bool Model::compareBlob(const uint8_t *a, size_t na, const uint8_t *b, size_t nb, float v) {
	return compareBlob((const float *)a, na / sizeof(float), (const float *)b, nb / sizeof(float), v);
}

bool Model::compareBlob(const float *a, size_t na, const float *b, size_t nb, float v) {
	if (na != nb) {
		return false;
	}

	while (na > 0) {
		if (std::abs(*a - *b) > v) {
			return false;
		}
		++ a; ++ b; -- na;
	}
	return true;
}

Model::~Model() { }

bool Model::init(ModelFlags f, const Value &val, uint32_t numLayers, StringView dataFilePath) {
	_flags = f;
	_numLayers = numLayers;

	if (!dataFilePath.empty()) {
		_dataFile = filesystem::openForReading(dataFilePath);
		if (!_dataFile) {
			log::error("snn::Model", "Fail to open model data file: ", dataFilePath);
			return false;
		}
	}

	if (val.isBool("trainable")) {
		if (val.getBool("trainable")) {
			_flags |= ModelFlags::Trainable;
		}
	}

	auto range = val.getString("inputRange");
	if (range == "[0,1]") {
		_flags |= ModelFlags::Range01;
	}

	if (auto &block = val.getValue("block_0")) {
		for (auto &it : block.asDict()) {
			if (it.first == "Input Height") {
				_inputHeight = it.second.getInteger();
			} else if (it.first == "Input Width") {
				_inputWidth = it.second.getInteger();
			}
		}
	}

	if (auto &node = val.getValue("node")) {
		for (auto &it : node.asDict()) {
			if (it.first == "inputChannels") {
				_inputChannels = it.second.getInteger();
			} else if (it.first == "upscale") {
				_upscale = it.second.getInteger();
			} else if (it.first == "useSubpixel") {
				_useSubPixel = it.second.getBool();
			}
		}
	}

	return true;
}

void Model::addLayer(Rc<Layer> &&l) {
	_layers.emplace(l->getInputIndex(), move(l));
}

bool Model::link() {
	// find inputs
	Vector<Layer *> linkedLayers;
	for (auto &it : _layers) {
		if (it.second->isInput()) {
			auto attachment = Rc<Attachment>::create(_attachments.size(), it.second->getOutputExtent(), it.second);
			it.second->setOutput(attachment);
			linkedLayers.emplace_back(it.second.get());
			linkInput(linkedLayers, it.second, _attachments.emplace_back(move(attachment)));
		}
	}

	if (linkedLayers.size() == _layers.size()) {
		_sortedLayers = move(linkedLayers);
		return true;
	}

	log::error("snn::Model", "Fail to link model: potential loop in execution tree");
	return false;
}

float Model::readFloatData() {
	float value = 0.0f;
	if (_dataFile.read((uint8_t*) &value, sizeof(float)) == sizeof(float)) {
		if (isHalfPrecision()) {
			value = convertToMediumPrecision(value);
		}
	}
	return value;
}

float Model::getLastLoss() const {
	if (auto l = dynamic_cast<LossLayer *>(_sortedLayers.back())) {
		return l->getParameter(LossLayer::P_Loss);
	}
	return 0.0f;
}

Vector<Layer *> Model::getInputs() const {
	Vector<Layer *> ret;
	for (auto &it : _sortedLayers) {
		if (it->isInput()) {
			ret.emplace_back(it);
		}
	}
	return ret;
}

void Model::linkInput(Vector<Layer *> &layers, Layer *inputLayer, Attachment *attachment) {
	// find usage
	for (auto &it : _layers) {
		for (auto &input : it.second->getInputs()) {
			if (input.layer == inputLayer->getInputIndex()) {
				attachment->addInputBy(it.second);
				it.second->setInputExtent(input.index, attachment, inputLayer->getOutputExtent());
				if (it.second->isInputDefined()) {
					if (std::find(layers.begin(), layers.end(), it.second.get()) == layers.end()) {
						auto attachment = Rc<Attachment>::create(_attachments.size(), it.second->getOutputExtent(), it.second);
						it.second->setOutput(attachment);
						layers.emplace_back(it.second.get());
						std::cout << "Layer " << it.second->getName() << " (" << it.second->getTag() << ") " << it.second->getOutputExtent() << "\n";
						linkInput(layers, it.second, _attachments.emplace_back(move(attachment)));
					}
				}
			}
		}
	}
}

Activation getActivationValue(StringView istr) {
	auto str = string::toupper<Interface>(istr);

	if (str == "RELU") {
		return Activation::RELU;
	} else if (str == "RELU6") {
		return Activation::RELU6;
	} else if (str == "TANH") {
		return Activation::TANH;
	} else if (str == "SIGMOID") {
		return Activation::SIGMOID;
	} else if (str == "LEAKYRELU") {
		return Activation::LEAKYRELU;
	} else if (str == "SILU") {
		return Activation::SILU;
	} else if (str == "LINEAR") {
		return Activation::None;
	}
	log::error("snn::Model", "Unknown activation: ", istr);
	return Activation::None;
}

float convertToMediumPrecision(float in) {
	union tmp {
		unsigned int unsint;
		float flt;
	};

	tmp _16to32, _32to16;

	_32to16.flt = in;

	unsigned short sign = (_32to16.unsint & 0x80000000) >> 31;
	unsigned short exponent = (_32to16.unsint & 0x7F800000) >> 23;
	unsigned int mantissa = _32to16.unsint & 0x7FFFFF;

	short newexp = exponent + (-127 + 15);

	unsigned int newMantissa;
	if (newexp >= 31) {
		newexp = 31;
		newMantissa = 0x00;
	} else if (newexp <= 0) {
		newexp = 0;
		newMantissa = 0;
	} else {
		newMantissa = mantissa >> 13;
	}

	if (newexp == 0) {
		if (newMantissa == 0) {
			_16to32.unsint = sign << 31;
		} else {
			newexp = 0;
			while ((newMantissa & 0x200) == 0) {
				newMantissa <<= 1;
				newexp++;
			}
			newMantissa <<= 1;
			newMantissa &= 0x3FF;
			_16to32.unsint = (sign << 31) | ((newexp + (-15 + 127)) << 23) | (newMantissa << 13);
		}
	} else if (newexp == 31) {
		_16to32.unsint = (sign << 31) | (0xFF << 23) | (newMantissa << 13);
	} else {
		_16to32.unsint = (sign << 31) | ((newexp + (-15 + 127)) << 23) | (newMantissa << 13);
	}

	return _16to32.flt;
}

float convertToHighPrecision(uint16_t in) {
	union tmp {
		unsigned int unsint;
		float flt;
	};

	unsigned short sign = (in & 0x8000) >> 15;
	unsigned short exponent = (in & 0x7C00) >> 10;
	unsigned int mantissa = (in & 0x3FF);

	tmp _16to32;

	short newexp = exponent + 127 - 15;
	unsigned int newMantissa = mantissa;
	if (newexp <= 0) {
		newexp = 0;
		newMantissa = 0;
	}

	_16to32.unsint = (sign << 31) | (newexp << 23) | (newMantissa << 13);
	return _16to32.flt;
}

void convertToMediumPrecision(Vector<float> &in) {
	for (auto &val : in) {
		val = convertToMediumPrecision(val);
	}
}

void convertToMediumPrecision(Vector<double> &in) {
	for (auto &val : in) {
		val = convertToMediumPrecision(val);
	}
}

void getByteRepresentation(float in, Vector<unsigned char> &byteRep, bool fp16) {
	if (fp16) {
		union tmp {
			unsigned int unsint;
			float flt;
		};
		tmp _32to16;
		_32to16.flt = in;
		unsigned short fp16Val;

		unsigned short sign = (_32to16.unsint & 0x80000000) >> 31;
		unsigned short exponent = (_32to16.unsint & 0x7F800000) >> 23;
		unsigned int mantissa = (_32to16.unsint & 0x7FFFFF);

		short newexp = exponent + (-127 + 15);

		unsigned int newMantissa;
		if (newexp >= 31) {
			newexp = 31;
			newMantissa = 0x00;
		} else if (newexp <= 0) {
			newexp = 0;
			newMantissa = 0;
		} else {
			newMantissa = mantissa >> 13;
		}

		fp16Val = sign << 15;
		fp16Val = fp16Val | (newexp << 10);
		fp16Val = fp16Val | (newMantissa);

		byteRep.push_back(fp16Val & 0xFF);
		byteRep.push_back(fp16Val >> 8 & 0xFF);
	} else {
		uint32_t fp32Val;
		::memcpy(&fp32Val, &in, 4);
		for (int i = 0; i < 4; i++) {
			byteRep.push_back((fp32Val >> 8 * i) & 0xFF);
		}
	}
}

}
