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

#include "XLSnnInputLayer.h"
#include "XLSnnVkInputLayer.h"

namespace stappler::xenolith::shadernn {

bool InputLayer::init(Model *m, StringView tag, size_t idx, const Value &data) {
	if (!Layer::init(m, tag, idx, data)) {
		return false;
	}

	_inputWidth = uint32_t(data.getInteger("Input Width", 1));
	_inputHeight = uint32_t(data.getInteger("Input Height", 1));
	_inputChannels = uint32_t(data.getInteger("outputPlanes", 1));
	if (data.hasValue("inputIndex")) {
		_inputIndex = uint32_t(data.getInteger("inputIndex", 0));
	}

	_isInputLayer = true;

	return true;
}

const core::QueuePassData *InputLayer::prepare(core::Queue::Builder &builder,
		Map<Layer *, const core::AttachmentData *> inputs,
		Map<Attachment *, const core::AttachmentData *> attachments) {

	auto inputIt = inputs.find(this);
	auto outputIt = attachments.find(getOutput());

	if (inputIt == inputs.end() || outputIt == attachments.end()) {
		log::error("snn::InputLayer", "No attachments specified");
		return nullptr;
	}

	return builder.addPass(getName(), core::PassType::Compute, core::RenderOrdering(_inputIndex),
			[&] (core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<vk::shadernn::InputLayer>::create(builder, passBuilder, inputIt->second, outputIt->second);
	});
}

}
