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

#include "XLSnnMatrixMulLayer.h"
#include "XLSnnVkMatrixMulLayer.h"

namespace stappler::xenolith::shadernn {

bool MatrixMulLayer::init(Model *m, StringView tag, size_t idx, const Value &data) {
	if (!Layer::init(m, tag, idx, data)) {
		return false;
	}

	_activation = getActivationValue(data.getString("activation"));
	_kernelSize = data.getInteger("kernel_size");

	return true;
}

void MatrixMulLayer::setInputExtent(uint32_t index, Attachment *a, Extent3 e) {
	Layer::setInputExtent(index, a, e);
	_input = a->getOutputBy();
	_batchSize = e.depth;
}

const core::QueuePassData *MatrixMulLayer::prepare(core::Queue::Builder &builder,
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
		return Rc<vk::shadernn::MatrixMulLayer>::create(builder, passBuilder, this, inputIt->second, outputIt->second);
	});
}

const core::AttachmentData *MatrixMulLayer::makeOutputAttachment(core::Queue::Builder &builder, bool isGlobalOutput) {
	return builder.addAttachemnt(toString(getName(), "_output"),
			[&] (core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		if (isGlobalOutput) {
			attachmentBuilder.defineAsOutput();
		}
		auto ext = getOutputExtent();
		return Rc<vk::BufferAttachment>::create(attachmentBuilder,
			core::BufferInfo(size_t(ext.width * ext.height * ext.depth * sizeof(float)),
					core::BufferUsage::StorageBuffer | core::BufferUsage::TransferDst, core::PassType::Compute)
		);
	});
}

void MatrixMulLayer::generateWeights(uint8_t *buf, uint64_t size, const core::BufferData::DataCallback &cb) const {
	auto &rand = _model->getRand();
	auto ext = _input->getOutputExtent();
	auto inputCount = ext.width * ext.height / 2;

	size /= sizeof(float);
	auto target = (float *)buf;

	double deviation = std::sqrt(1. / std::max(inputCount, uint32_t(1)));

	while (size) {
		*(target ++) = rand.normal(0, deviation);
		-- size;
	}
}

void MatrixMulLayer::generateFreeTerms(uint8_t *buf, uint64_t size, const core::BufferData::DataCallback &cb) const {
	size /= sizeof(float);
	auto target = (float *)buf;

	while (size) {
		*(target ++) = 0.0f;
		-- size;
	}
}


}
