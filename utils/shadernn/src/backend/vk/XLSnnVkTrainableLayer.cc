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

#include "XLSnnVkTrainableLayer.h"

namespace stappler::xenolith::vk::shadernn {

TrainableLayer::~TrainableLayer() { }

void TrainableLayer::initPropagation(Queue::Builder &queueBuilder, QueuePassBuilder &builder, const core::AttachmentData *source, uint32_t idx) {
	using namespace core;

	auto paramsBuf = queueBuilder.addBuffer(toString(getName(), "_trainingParams"), BufferInfo(BufferUsage::StorageBuffer, size_t(TV_Count * sizeof(float))),
			[this] (uint8_t *buf, uint64_t size, const BufferData::DataCallback &) {
		auto target = (float *)buf;

		float rate = _learningRate;
		float regL1 = _regularizationL1;
		float regL2 = _regularizationL2;

		target[TV_MomentDecayRateVar] = _momentDecayRate;
		target[TV_OpMomentDecayRateVar] = 1 - _momentDecayRate;
		target[TV_OpRegL2MomentDecayRateVar] = -rate * regL2;
		target[TV_RateVar] = (-rate);
		target[TV_L1Threshold] = regL1;
		target[TV_L1Mult] = -rate;
	});

	auto trainable = getTrainableGradients(queueBuilder);
	trainable.emplace(trainable.begin(), paramsBuf);

	_staticPropagationBuffers = trainable.size();

	_propagationAttachment = queueBuilder.addAttachemnt(toString(getName(), "_BackwardOnce_data"),
			[&] (AttachmentBuilder &builder) -> Rc<core::Attachment> {
		return Rc<vk::BufferAttachment>::create(builder, sp::move(trainable));
	});

	_externalPropagationDataSource = source;
	_externalPropagationBufferIdx = idx;

	auto propagationPassAttachment = builder.addAttachment(_propagationAttachment);

	auto l = builder.addDescriptorLayout([&] (PipelineLayoutBuilder &layout) {
		layout.addSet([&] (DescriptorSetBuilder &set) {
			set.addDescriptorArray(propagationPassAttachment, getPropagationSubpassBufferCount(), DescriptorType::StorageBuffer);
		});
	});

	builder.addSubpass([&] (SubpassBuilder &subpass) {
		initPropagationSubpass(queueBuilder, builder, subpass, l);
	});

	const core::QueuePassData *pass = _inputAttachment->passes.front()->pass;
	if (_inputAttachment->passes.front()->pass->pass != this) {
		auto trainable = dynamic_cast<TrainableLayer *>(pass->pass.get());
		if (trainable) {
			trainable->initPropagation(queueBuilder, builder, _propagationAttachment, _targetPropagationIdx);
		}
	}
}

void TrainableLayer::initPropagationSubpass(Queue::Builder &queueBuilder, core::QueuePassBuilder &,
		core::SubpassBuilder &, const core::PipelineLayoutData *) {

}

Vector<const core::BufferData *> TrainableLayer::getTrainableGradients(Queue::Builder &queueBuilder) const {
	return Vector<const core::BufferData *>();
}

bool TrainableLayer::isBackwardNeeded() const {
	const core::QueuePassData *pass = _inputAttachment->passes.front()->pass;
	if (_inputAttachment->passes.front()->pass->pass != this && dynamic_cast<TrainableLayer *>(pass->pass.get())) {
		return true;
	}
	return false;
}

}
