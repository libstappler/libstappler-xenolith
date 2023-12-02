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

#include "XLSnnLayer.h"
#include "XLSnnModelProcessor.h"

namespace stappler::xenolith::shadernn {

bool Layer::init(Model *model, StringView tag, size_t idx, const Value &data) {
	_model = model;
	_tag = tag.str<Interface>();
	_name = data.getString("name");
	_inputIndex = idx;
	_numInputPlanes = uint32_t(data.getInteger("inputPlanes"));
	_numOutputPlanes = uint32_t(data.getInteger("outputPlanes"));

	auto numInputs = data.getInteger("numInputs");
	_inputs.reserve(numInputs);

	uint32_t i = 0;
	for (auto &it : data.getArray("inputId")) {
		_inputs.emplace_back(LayerInputInfo({i, static_cast<uint32_t>(it.getInteger())}));
		++ i;
	}

	i = 0;
	for (auto &it : data.getArray("inbounds")) {
		if (i < _inputs.size()) {
			_inputs[i].name = it.getString();
		}
		++ i;
	}

	return true;
}

void Layer::setInputExtent(uint32_t index, Attachment *a, Extent3 e) {
	if (index < _inputs.size()) {
		_inputs[index].extent = e;
		_inputs[index].attachment = a;
	}
}

bool Layer::isInputDefined() const {
	for (auto &it : _inputs) {
		if (it.extent == Extent3()) {
			return false;
		}
	}

	return true;
}

Extent3 Layer::getOutputExtent() const {
	Extent3 ret;
	LayerTransformInfo accumulatedTransform( { 0, { { 0.0f, 0.0f, 0.0f, 0.0f } } });
	auto t = getOutputTransform();
	for (auto &dim : _inputs) {
		if (!t.isFixed) {
			accumulatedTransform.scaleWidth = std::max(accumulatedTransform.scaleWidth, t.scaleWidth * dim.extent.width);
			accumulatedTransform.translateWidth = std::max(accumulatedTransform.translateWidth, t.translateWidth);
			accumulatedTransform.scaleHeight = std::max(accumulatedTransform.scaleHeight, t.scaleHeight * dim.extent.height);
			accumulatedTransform.translateHeight = std::max(accumulatedTransform.translateHeight, t.translateHeight);
			ret.width = accumulatedTransform.scaleWidth + accumulatedTransform.translateWidth;
			ret.height = accumulatedTransform.scaleHeight + accumulatedTransform.translateHeight;
			ret.depth = std::max(ret.depth, dim.extent.depth);
		} else {
			accumulatedTransform.fixedWidth = std::max(accumulatedTransform.fixedWidth, t.fixedWidth);
			accumulatedTransform.fixedHeight = std::max(accumulatedTransform.fixedHeight, t.fixedHeight);
			accumulatedTransform.fixedDepth = std::max(accumulatedTransform.fixedDepth, t.fixedDepth);
			accumulatedTransform.fixedBatch = std::max(accumulatedTransform.fixedBatch, t.fixedBatch);
			ret.width = accumulatedTransform.fixedWidth + accumulatedTransform.translateWidth;
			ret.height = accumulatedTransform.fixedHeight + accumulatedTransform.translateHeight;
			ret.depth = std::max(ret.depth, dim.extent.depth);
		}
	}
	ret.depth = (_numOutputPlanes + 3) / 4;
	return ret;
}

Extent3 Layer::getOutputExtent(const ModelSpecialization &spec) const {
	Extent3 ret;
	LayerTransformInfo accumulatedTransform( { 0, { { 0.0f, 0.0f, 0.0f, 0.0f } } });
	auto t = getOutputTransform();
	for (auto &d : _inputs) {
		auto iit = spec.attachments.find(d.attachment);
		if (iit != spec.attachments.end()) {
			auto extent = iit->second;
			if (!t.isFixed) {
				accumulatedTransform.scaleWidth = std::max(accumulatedTransform.scaleWidth, t.scaleWidth * extent.width);
				accumulatedTransform.translateWidth = std::max(accumulatedTransform.translateWidth, t.translateWidth);
				accumulatedTransform.scaleHeight = std::max(accumulatedTransform.scaleHeight, t.scaleHeight * extent.height);
				accumulatedTransform.translateHeight = std::max(accumulatedTransform.translateHeight, t.translateHeight);
				ret.width = accumulatedTransform.scaleWidth + accumulatedTransform.translateWidth;
				ret.height = accumulatedTransform.scaleHeight + accumulatedTransform.translateHeight;
				ret.depth = std::max(ret.depth, extent.depth);
			} else {
				accumulatedTransform.fixedWidth = std::max(accumulatedTransform.fixedWidth, t.fixedWidth);
				accumulatedTransform.fixedHeight = std::max(accumulatedTransform.fixedHeight, t.fixedHeight);
				accumulatedTransform.fixedDepth = std::max(accumulatedTransform.fixedDepth, t.fixedDepth);
				accumulatedTransform.fixedBatch = std::max(accumulatedTransform.fixedBatch, t.fixedBatch);
				ret.width = accumulatedTransform.fixedWidth + accumulatedTransform.translateWidth;
				ret.height = accumulatedTransform.fixedHeight + accumulatedTransform.translateHeight;
				ret.depth = std::max(ret.depth, extent.depth);
			}
		} else {
			log::error("snn::Layer", "Extent is not defined for layer : ", _name);
		}
	}
	ret.depth = (_numOutputPlanes + 3) / 4;
	return ret;
}

}
