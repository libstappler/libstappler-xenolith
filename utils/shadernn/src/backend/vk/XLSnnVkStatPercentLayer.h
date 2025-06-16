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

#ifndef SRC_BACKEND_VK_XLSNNVKSTATPERCENTLAYER_H_
#define SRC_BACKEND_VK_XLSNNVKSTATPERCENTLAYER_H_

#include "XLVkRenderPass.h"
#include "XLVkQueuePass.h"
#include "XLVkAttachment.h"
#include "XLSnnStatPercentLayer.h"

namespace stappler::xenolith::vk::shadernn {

class StatPercentLayer : public vk::QueuePass {
public:
	using Front = xenolith::shadernn::StatPercentLayer;

	static constexpr auto StatPercentLayerClassesPipeline = "StatPercentLayerClassesPipeline";
	static constexpr auto StatPercentLayerPercentPipeline = "StatPercentLayerPercentPipeline";

	virtual ~StatPercentLayer();

	virtual bool init(Queue::Builder &queueBuilder, QueuePassBuilder &, Front *,
			const AttachmentData *input, const AttachmentData *output);

	const AttachmentData *getInputAttachment() const { return _inputAttachment; }
	const AttachmentData *getOutputAttachment() const { return _outputAttachment; }
	const AttachmentData *getClassesAttachment() const { return _classesAttachment; }

	const Front *getFront() const { return _front; }

	class LayerHandle : public vk::QueuePassHandle {
	public:
		virtual ~LayerHandle() = default;

		virtual bool prepare(FrameQueue &q, Function<void(bool)> &&cb) override;

	protected:
		virtual Vector<const core::CommandBuffer *> doPrepareCommands(FrameHandle &) override;

		virtual void doSubmitted(FrameHandle &, Function<void(bool)> &&, bool,
				Rc<core::Fence> &&) override;

		vk::BufferAttachmentHandle *_inputBuffer = nullptr;
		vk::BufferAttachmentHandle *_outputBuffer = nullptr;
		vk::BufferAttachmentHandle *_classesBuffer = nullptr;
		const Front *_front = nullptr;

		Rc<Buffer> _classesSizes;
		Rc<Buffer> _classesIndexes;
		Rc<Buffer> _output;
	};

protected:
	using QueuePass::init;

	const AttachmentData *_inputAttachment = nullptr;
	const AttachmentData *_outputAttachment = nullptr;
	const AttachmentData *_classesAttachment = nullptr;

	Rc<Front> _front;
};

class StatAnalysisLayer : public vk::QueuePass {
public:
	using Front = xenolith::shadernn::StatAnalysisLayer;

	virtual ~StatAnalysisLayer();

	virtual bool init(Queue::Builder &queueBuilder, QueuePassBuilder &, Front *,
			const AttachmentData *inputData, const AttachmentData *inputClasses,
			const AttachmentData *output);

	const AttachmentData *getInputDataAttachment() const { return _inputDataAttachment; }
	const AttachmentData *getInputClassesAttachment() const { return _inputClassesAttachment; }
	const AttachmentData *getOutputAttachment() const { return _outputAttachment; }

	const Front *getFront() const { return _front; }

	class LayerHandle : public vk::QueuePassHandle {
	public:
		virtual ~LayerHandle() = default;

		virtual bool prepare(FrameQueue &q, Function<void(bool)> &&cb) override;

	protected:
		virtual Vector<const core::CommandBuffer *> doPrepareCommands(FrameHandle &) override;

		virtual void doSubmitted(FrameHandle &, Function<void(bool)> &&, bool,
				Rc<core::Fence> &&) override;

		vk::BufferAttachmentHandle *_inputDataBuffer = nullptr;
		vk::BufferAttachmentHandle *_inputClassesBuffer = nullptr;
		vk::BufferAttachmentHandle *_outputBuffer = nullptr;
		const Front *_front = nullptr;

		Rc<Buffer> _output;
	};

protected:
	using QueuePass::init;

	const AttachmentData *_inputDataAttachment = nullptr;
	const AttachmentData *_inputClassesAttachment = nullptr;
	const AttachmentData *_outputAttachment = nullptr;

	Rc<Front> _front;
};

} // namespace stappler::xenolith::vk::shadernn

#endif /* SRC_BACKEND_VK_XLSNNVKSTATPERCENTLAYER_H_ */
