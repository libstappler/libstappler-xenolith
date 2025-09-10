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

#include "XLSnnModelProcessor.h"
#include "XLSnnInputLayer.h"
#include "XLSnnConvLayer.h"
#include "XLSnnSubpixelLayer.h"
#include "XLSnnStatPercentLayer.h"
#include "XLSnnMatrixMulLayer.h"
#include "XLSnnLossLayer.h"

namespace stappler::xenolith::shadernn {

bool ModelProcessor::init() {
	_layers.emplace("inputlayer",
			[](Model *m, StringView tag, size_t idx, const Value &data) -> Rc<Layer> {
		return Rc<InputLayer>::create(m, tag, idx, data);
	});

	_layers.emplace("inputbufferlayer",
			[](Model *m, StringView tag, size_t idx, const Value &data) -> Rc<Layer> {
		return Rc<InputBufferLayer>::create(m, tag, idx, data);
	});

	_layers.emplace("inputcsvintlayer",
			[](Model *m, StringView tag, size_t idx, const Value &data) -> Rc<Layer> {
		return Rc<InputCsvIntLayer>::create(m, tag, idx, data);
	});

	_layers.emplace("conv2d",
			[](Model *m, StringView tag, size_t idx, const Value &data) -> Rc<Layer> {
		return Rc<Conv2DLayer>::create(m, tag, idx, data);
	});

	_layers.emplace("subpixel",
			[](Model *m, StringView tag, size_t idx, const Value &data) -> Rc<Layer> {
		return Rc<SubpixelLayer>::create(m, tag, idx, data);
	});

	_layers.emplace("statpercentlayer",
			[](Model *m, StringView tag, size_t idx, const Value &data) -> Rc<Layer> {
		return Rc<StatPercentLayer>::create(m, tag, idx, data);
	});

	_layers.emplace("statanalysislayer",
			[](Model *m, StringView tag, size_t idx, const Value &data) -> Rc<Layer> {
		return Rc<StatAnalysisLayer>::create(m, tag, idx, data);
	});

	_layers.emplace("matrixmullayer",
			[](Model *m, StringView tag, size_t idx, const Value &data) -> Rc<Layer> {
		return Rc<MatrixMulLayer>::create(m, tag, idx, data);
	});

	_layers.emplace("crossentropylosslayer",
			[](Model *m, StringView tag, size_t idx, const Value &data) -> Rc<Layer> {
		return Rc<CrossEntropyLossLayer>::create(m, tag, idx, data);
	});

	return true;
}

Rc<Model> ModelProcessor::load(const FileInfo &modelPath, ModelFlags flags) {
	Value data = data::readFile<Interface>(modelPath);
	if (!data) {
		return nullptr;
	}

	auto &numNode = data.getValue("numLayers");
	auto numLayers = numNode.getInteger("count");
	if (numLayers == 0) {
		return nullptr;
	}

	String dataFilePath;
	if (numNode.isString("bin_file_name")) {
		auto dataFile = numNode.getString("bin_file_name");
		dataFilePath = filepath::merge<Interface>(filepath::root(modelPath.path), dataFile);
	}

	auto m = Rc<Model>::create(flags, data, numLayers, dataFilePath);
	if (loadFromJson(m, move(data)) && m->link()) {
		return m;
	}
	return nullptr;
}

ModelSpecialization ModelProcessor::specializeModel(Model *model, Extent3 extent) {
	Map<const Layer *, Extent3> inputs;
	for (auto &it : model->getInputs()) { inputs.emplace(it, extent); }
	return specializeModel(model, sp::move(inputs));
}

ModelSpecialization ModelProcessor::specializeModel(Model *model,
		Map<const Layer *, Extent3> &&inputs) {
	ModelSpecialization ret;
	ret.inputs = sp::move(inputs);

	for (auto &it : ret.inputs) { ret.attachments.emplace(it.first->getOutput(), it.second); }

	for (auto &it : model->getSortedLayers()) {
		if (ret.inputs.find(it) == ret.inputs.end()) {
			auto ext = it->getOutputExtent(ret);
			ret.attachments.emplace(it->getOutput(), ext);
		}
	}

	/*for (auto &it : model->getSortedLayers()) {
		auto iit = ret.attachments.find(it->getOutput());
		if (iit != ret.attachments.end()) {
			std::cout << "Specialization: Layer " << it->getName() << " (" << it->getTag() << ") " << iit->second << "\n";
		}
	}*/

	return ret;
}

bool ModelProcessor::loadFromJson(Model *m, Value &&data) const {
	for (auto &it : data.asDict()) {
		if (!StringView(it.first).starts_with("Layer_")) {
			continue;
		}

		StringView tag(it.first);
		StringView r(tag);
		r += "Layer_"_len;

		auto idx = r.readInteger(10).get();
		if (auto l = makeLayer(m, tag, idx, move(it.second))) {
			m->addLayer(move(l));
		} else {
			log::source().error("ModelProcessor", "Fail to load layer: ", tag);
			return false;
		}
	}
	return true;
}

Rc<Layer> ModelProcessor::makeLayer(Model *m, StringView tag, size_t idx, Value &&data) const {
	auto numInbound = data.getInteger("numInputs");
	auto &inputs = data.getArray("inputId");
	if (numInbound != int64_t(inputs.size())) {
		return nullptr;
	}

	auto layerType = data.getString("type");
	if (layerType == "Lambda") {
		layerType = data.getString("name");
	}

	if (layerType == "DepthwiseConv2D" || layerType == "Depthwise") {
		layerType = "SeparableConv2D";
	}
	if (layerType == "InstanceNormalization") {
		layerType = "InstanceNorm";
	}
	if (layerType == "ZeroPadding2D") {
		layerType = "Pad";
	}
	if (layerType == "subpixel" || layerType == "depth_to_space") {
		layerType = "Subpixel";
	}

	layerType = string::tolower<Interface>(layerType);

	auto it = _layers.find(layerType);
	if (it != _layers.end()) {
		return it->second(m, tag, idx, move(data));
	}

	return nullptr;
}

} // namespace stappler::xenolith::shadernn
