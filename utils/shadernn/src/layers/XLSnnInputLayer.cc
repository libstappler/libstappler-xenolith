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
#include "XLSnnAttachment.h"

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
		log::source().error("snn::InputLayer", "No attachments specified");
		return nullptr;
	}

	return builder.addPass(getName(), core::PassType::Compute, core::RenderOrdering(_inputIndex),
			[&](core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<vk::shadernn::InputLayer>::create(builder, passBuilder, inputIt->second,
				outputIt->second);
	});
}

const core::AttachmentData *InputLayer::makeInputAttachment(core::Queue::Builder &builder) {
	return builder.addAttachemnt(toString(getName(), "_input"),
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		return Rc<vk::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(Extent2(getOutputExtent().width, getOutputExtent().height),
						core::ImageUsage::Storage | core::ImageUsage::TransferSrc,
						core::ImageTiling::Optimal, core::ImageFormat::R8G8B8A8_UNORM,
						core::PassType::Compute),
				core::ImageAttachment::AttachmentInfo{.initialLayout =
															  core::AttachmentLayout::Ignored,
					.finalLayout = core::AttachmentLayout::Ignored,
					.clearOnLoad = true,
					.clearColor = Color4F(0.0f, 0.0f, 0.0f, 0.0f)});
	});
}

const core::AttachmentData *InputLayer::makeOutputAttachment(core::Queue::Builder &builder,
		bool isGlobalOutput) {
	auto modelFormat = _model->isHalfPrecision() ? core::ImageFormat::R16G16B16A16_SFLOAT
												 : core::ImageFormat::R32G32B32A32_SFLOAT;

	auto output = getOutput();
	auto attachment = builder.addAttachemnt(output->getName(),
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		if (isGlobalOutput) {
			attachmentBuilder.defineAsOutput();
		}
		return Rc<vk::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(output->getExtent(), core::ImageType::Image3D,
						core::ImageUsage::Storage | core::ImageUsage::TransferSrc,
						core::ImageTiling::Optimal, modelFormat, core::PassType::Compute),
				core::ImageAttachment::AttachmentInfo{.initialLayout =
															  core::AttachmentLayout::Ignored,
					.finalLayout = core::AttachmentLayout::Ignored});
	});
	return attachment;
}

bool InputBufferLayer::init(Model *m, StringView tag, size_t idx, const Value &data) {
	if (!Layer::init(m, tag, idx, data)) {
		return false;
	}

	_inputWidth = uint32_t(data.getInteger("Input Width", 1));
	_inputHeight = uint32_t(data.getInteger("Input Height", 1));
	_inputObjects = uint32_t(data.getInteger("Input Batch", 1));

	_isInputLayer = true;

	return true;
}

const core::QueuePassData *InputBufferLayer::prepare(core::Queue::Builder &builder,
		Map<Layer *, const core::AttachmentData *> inputs,
		Map<Attachment *, const core::AttachmentData *> attachments) {

	auto inputIt = inputs.find(this);
	auto outputIt = attachments.find(getOutput());

	if (inputIt == inputs.end() || outputIt == attachments.end()) {
		log::source().error("snn::InputLayer", "No attachments specified");
		return nullptr;
	}

	return builder.addPass(getName(), core::PassType::Compute, core::RenderOrdering(_inputIndex),
			[&](core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<vk::shadernn::InputBufferLayer>::create(builder, passBuilder, this,
				inputIt->second, outputIt->second);
	});
}

const core::AttachmentData *InputBufferLayer::makeInputAttachment(core::Queue::Builder &builder) {
	return builder.addAttachemnt(toString(getName(), "_input"),
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		return Rc<vk::BufferAttachment>::create(attachmentBuilder,
				core::BufferInfo(size_t(_inputWidth * _inputHeight * _inputObjects * sizeof(float)),
						core::BufferUsage::StorageBuffer | core::BufferUsage::TransferDst,
						core::PassType::Compute));
	});
}

const core::AttachmentData *InputBufferLayer::makeOutputAttachment(core::Queue::Builder &builder,
		bool isGlobalOutput) {
	return builder.addAttachemnt(toString(getName(), "_output"),
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		if (isGlobalOutput) {
			attachmentBuilder.defineAsOutput();
		}
		return Rc<vk::BufferAttachment>::create(attachmentBuilder,
				core::BufferInfo(size_t(_inputWidth * _inputHeight * _inputObjects * sizeof(float)),
						core::BufferUsage::StorageBuffer, core::PassType::Compute));
	});
}

bool InputCsvIntLayer::init(Model *m, StringView tag, size_t idx, const Value &data) {
	if (!Layer::init(m, tag, idx, data)) {
		return false;
	}

	_inputObjects = data.getInteger("inputObjects");

	for (auto &it : data.getArray("fields")) { _fields.emplace_back(it.getInteger()); }

	_isInputLayer = true;

	for (auto &it : data.getArray("norm")) {
		_norm.emplace_back(NormData{uint64_t(it.getInteger(0)), uint64_t(it.getInteger(1))});
	}

	return true;
}

const core::QueuePassData *InputCsvIntLayer::prepare(core::Queue::Builder &builder,
		Map<Layer *, const core::AttachmentData *> inputs,
		Map<Attachment *, const core::AttachmentData *> attachments) {
	auto inputIt = inputs.find(this);
	auto outputIt = attachments.find(getOutput());

	if (inputIt == inputs.end() || outputIt == attachments.end()) {
		log::source().error("snn::InputLayer", "No attachments specified");
		return nullptr;
	}

	return builder.addPass(getName(), core::PassType::Compute, core::RenderOrdering(_inputIndex),
			[&](core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<vk::shadernn::InputCsvIntLayer>::create(builder, passBuilder, this,
				inputIt->second, outputIt->second);
	});
}

const core::AttachmentData *InputCsvIntLayer::makeInputAttachment(core::Queue::Builder &builder) {
	return builder.addAttachemnt(toString(getName(), "_input"),
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		return Rc<vk::BufferAttachment>::create(attachmentBuilder,
				core::BufferInfo(size_t(_inputObjects * sizeof(uint64_t)),
						core::BufferUsage::StorageBuffer | core::BufferUsage::TransferDst,
						core::PassType::Compute));
	});
}

const core::AttachmentData *InputCsvIntLayer::makeOutputAttachment(core::Queue::Builder &builder,
		bool isGlobalOutput) {
	return builder.addAttachemnt(toString(getName(), "_output"),
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		if (isGlobalOutput) {
			attachmentBuilder.defineAsOutput();
		}
		return Rc<vk::BufferAttachment>::create(attachmentBuilder,
				core::BufferInfo(size_t(_inputObjects * sizeof(uint64_t)),
						core::BufferUsage::StorageBuffer | core::BufferUsage::TransferDst,
						core::PassType::Compute));
	});
}

} // namespace stappler::xenolith::shadernn
