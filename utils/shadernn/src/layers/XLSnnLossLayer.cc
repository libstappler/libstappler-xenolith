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

#include "XLSnnLossLayer.h"
#include "XLSnnAttachment.h"
#include "XLSnnVkLossLayer.h"

namespace stappler::xenolith::shadernn {

static constexpr float MaxGradient = 1e+06;

void LossLayer::setParameter(ParameterIndex idx, float val) {
	if (idx < P_Count) {
		_params[idx] = val;
	}
}

float LossLayer::getParameter(ParameterIndex idx) const {
	if (idx < P_Count) {
		return _params[idx];
	}
	return 0.0f;
}

void LossLayer::synchronizeParameters(SpanView<float> data) {
	memcpy(_params.data(), data.data(), _params.size() * sizeof(float));
}

bool CrossEntropyLossLayer::init(Model *m, StringView tag, size_t idx, const Value &data) {
	if (!LossLayer::init(m, tag, idx, data)) {
		return false;
	}

	_labelsInputName = data.getString("labels");
	_batchSize = data.getInteger("batch_size");
	_classesCount = data.getInteger("classes_count");

	setParameter(P_LossWeight, 1.0f);
	setParameter(P_Loss, 0.0f);
	setParameter(P_LossDivider, 1.f / float(_batchSize));
	setParameter(P_LossGradientDivider, getParameter(P_LossDivider) * getParameter(P_LossWeight));
	setParameter(P_MinGradient, -MaxGradient);
	setParameter(P_MaxGradient, MaxGradient);

	return true;
}

void CrossEntropyLossLayer::setInputExtent(uint32_t index, Attachment *a, Extent3 e) {
	Layer::setInputExtent(index, a, e);
	if (a->getOutputBy()->getName() == _labelsInputName) {
		_inputLabels = a->getOutputBy();
	} else {
		_inputNetwork = a->getOutputBy();
	}
}

const core::QueuePassData *CrossEntropyLossLayer::prepare(core::Queue::Builder &builder,
			Map<Layer *, const core::AttachmentData *> inputs,
			Map<Attachment *, const core::AttachmentData *> attachments) {
	auto inputLabelsIt = attachments.find(_inputs[0].attachment);
	auto inputNetworkIt = attachments.find(_inputs[1].attachment);
	auto outputIt = attachments.find(getOutput());

	if (inputLabelsIt == attachments.end() || inputNetworkIt == attachments.end() || outputIt == attachments.end()) {
		log::error("snn::InputLayer", "No attachments specified");
		return nullptr;
	}

	return builder.addPass(getName(), core::PassType::Compute, core::RenderOrdering(_inputIndex),
			[&] (core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<vk::shadernn::CrossEntropyLossLayer>::create(builder, passBuilder, this, inputLabelsIt->second, inputNetworkIt->second, outputIt->second);
	});
}

const core::AttachmentData *CrossEntropyLossLayer::makeOutputAttachment(core::Queue::Builder &builder, bool isGlobalOutput) {
	return builder.addAttachemnt(toString(getName(), "_output"),
			[&] (core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		if (isGlobalOutput) {
			attachmentBuilder.defineAsOutput(core::FrameRenderPassState::Complete);
		}
		return Rc<vk::BufferAttachment>::create(attachmentBuilder,
			core::BufferInfo(size_t(1 * 4 * sizeof(float)),
					core::BufferUsage::StorageBuffer | core::BufferUsage::TransferDst, core::PassType::Compute)
		);
	});
}

}
