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

#ifndef SRC_BACKEND_VK_XLSNNVKACTIVATIONLAYER_H_
#define SRC_BACKEND_VK_XLSNNVKACTIVATIONLAYER_H_

#include "XLSnnVkShaders.h"
#include "XLVkRenderPass.h"
#include "XLVkQueuePass.h"
#include "XLVkAttachment.h"

namespace stappler::xenolith::vk::shadernn {

struct ActivationData {
	UVec4 inputSize;
	Activation activationType;
	float leakyValue;
};

struct ActivationDataInput : core::AttachmentInputData {
	ActivationData data;
};

class ActivationLayer : public vk::QueuePass {
public:
	virtual ~ActivationLayer();

	virtual bool init(Queue::Builder &queueBuilder, QueuePassBuilder &, const AttachmentData *input, const AttachmentData *output);

	const AttachmentData *getInputAttachment() const { return _inputAttachment; }
	const AttachmentData *getOutputAttachment() const { return _outputAttachment; }
	const AttachmentData *getDataAttachment() const { return _dataAttachment; }

	class LayerHandle : public vk::QueuePassHandle {
	public:
		virtual ~LayerHandle();

		virtual bool prepare(FrameQueue &q, Function<void(bool)> &&cb) override;

	protected:
		virtual Vector<const vk::CommandBuffer *> doPrepareCommands(FrameHandle &) override;

		const vk::ImageAttachmentHandle *_inputImage = nullptr;
		const vk::ImageAttachmentHandle *_outputImage = nullptr;
		const core::AttachmentHandle *_dataBuffer = nullptr;
	};

protected:
	using QueuePass::init;

	const AttachmentData *_inputAttachment = nullptr;
	const AttachmentData *_outputAttachment = nullptr;
	const AttachmentData *_dataAttachment = nullptr;
};

}

#endif /* SRC_BACKEND_VK_XLSNNVKACTIVATIONLAYER_H_ */
