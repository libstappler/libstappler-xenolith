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

#include "XLSnnSubpixelLayer.h"
#include "XLSnnVkSubpixelLayer.h"

namespace stappler::xenolith::shadernn {

bool SubpixelLayer::init(Model *m, StringView tag, size_t idx, const Value &data) {
	if (!Layer::init(m, tag, idx, data)) {
		return false;
	}

	return true;
}

const core::QueuePassData *SubpixelLayer::prepare(core::Queue::Builder &builder,
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
		return Rc<vk::shadernn::SubpixelLayer>::create(builder, passBuilder, this, inputIt->second, outputIt->second);
	});
}

}
