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

#include "XLSnnStatPercentLayer.h"
#include "XLSnnVkStatPercentLayer.h"

namespace stappler::xenolith::shadernn {

bool StatPercentLayer::init(Model *m, StringView tag, size_t idx, const Value &data) {
	if (!Layer::init(m, tag, idx, data)) {
		return false;
	}

	_fieldClass = data.getInteger("fieldClass");
	_fieldSource = data.getInteger("fieldSource");
	_fieldTarget = data.getInteger("fieldTarget");
	_classMin = data.getInteger("classMin");
	_classCount = data.getInteger("classCount");

	return true;
}

const core::QueuePassData *StatPercentLayer::prepare(core::Queue::Builder &builder,
		Map<Layer *, const core::AttachmentData *> inputs,
		Map<Attachment *, const core::AttachmentData *> attachments) {
	auto inputIt = attachments.find(_inputs.front().attachment);
	auto outputIt = attachments.find(getOutput());

	if (inputIt == attachments.end() || outputIt == attachments.end()) {
		log::source().error("snn::InputLayer", "No attachments specified");
		return nullptr;
	}

	return builder.addPass(getName(), core::PassType::Compute, core::RenderOrdering(_inputIndex),
			[&](core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<vk::shadernn::StatPercentLayer>::create(builder, passBuilder, this,
				inputIt->second, outputIt->second);
	});
}

const core::AttachmentData *StatPercentLayer::makeOutputAttachment(core::Queue::Builder &builder,
		bool isGlobalOutput) {
	return builder.addAttachemnt(toString(getName(), "_output"),
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		if (isGlobalOutput) {
			attachmentBuilder.defineAsOutput();
		}
		return Rc<vk::BufferAttachment>::create(attachmentBuilder,
				core::BufferInfo(size_t(_classCount * 4 * sizeof(float)),
						core::BufferUsage::StorageBuffer | core::BufferUsage::TransferDst,
						core::PassType::Compute));
	});
}

bool StatAnalysisLayer::init(Model *m, StringView tag, size_t idx, const Value &data) {
	if (!Layer::init(m, tag, idx, data)) {
		return false;
	}

	_threshold = float(data.getDouble("threshold"));

	return true;
}

void StatAnalysisLayer::setInputExtent(uint32_t index, Attachment *a, Extent3 e) {
	Layer::setInputExtent(index, a, e);

	auto outBy = a->getOutputBy();
	if (auto l = dynamic_cast<StatPercentLayer *>(outBy)) {
		_percent = l;
	}
}

const core::QueuePassData *StatAnalysisLayer::prepare(core::Queue::Builder &builder,
		Map<Layer *, const core::AttachmentData *> inputs,
		Map<Attachment *, const core::AttachmentData *> attachments) {
	auto inputDataIt = attachments.find(_inputs[0].attachment);
	auto inputClasseIt = attachments.find(_inputs[1].attachment);
	auto outputIt = attachments.find(getOutput());

	if (inputDataIt == attachments.end() || inputClasseIt == attachments.end()
			|| outputIt == attachments.end()) {
		log::source().error("snn::InputLayer", "No attachments specified");
		return nullptr;
	}

	return builder.addPass(getName(), core::PassType::Compute, core::RenderOrdering(_inputIndex),
			[&](core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<vk::shadernn::StatAnalysisLayer>::create(builder, passBuilder, this,
				inputDataIt->second, inputClasseIt->second, outputIt->second);
	});
}

const core::AttachmentData *StatAnalysisLayer::makeOutputAttachment(core::Queue::Builder &builder,
		bool isGlobalOutput) {
	return builder.addAttachemnt(toString(getName(), "_output"),
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		if (isGlobalOutput) {
			attachmentBuilder.defineAsOutput();
		}
		return Rc<vk::BufferAttachment>::create(attachmentBuilder,
				core::BufferInfo(size_t(1 * 4 * sizeof(float)),
						core::BufferUsage::StorageBuffer | core::BufferUsage::TransferDst,
						core::PassType::Compute));
	});
}

} // namespace stappler::xenolith::shadernn
