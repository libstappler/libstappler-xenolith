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

#include "XLSnnGenTest.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameRequest.h"
#include "XLVkLoop.h"
#include "XLApplication.h"
#include "SPBitmap.h"

namespace stappler::xenolith::vk::shadernn {

GenQueue::~GenQueue() { }

bool GenQueue::init() {
	using namespace core;
	Queue::Builder builder("Gen");

	auto imageAttachment = builder.addAttachemnt("OutputAttachment",
			[&] (AttachmentBuilder &attachmentBuilder) -> Rc<Attachment> {
		attachmentBuilder.defineAsOutput();
		return Rc<vk::ImageAttachment>::create(attachmentBuilder,
			ImageInfo(Extent3(16, 16, 16),
					ImageUsage::Storage | ImageUsage::TransferSrc,
					ImageTiling::Optimal, ImageFormat::R16G16B16A16_SFLOAT),
			ImageAttachment::AttachmentInfo{
				.initialLayout = AttachmentLayout::General,
				.finalLayout = AttachmentLayout::General,
				.clearOnLoad = true,
				.clearColor = Color4F(0.0f, 0.0f, 0.0f, 0.0f)}
		);
	});

	_genLayer = builder.addPass("GenLayer", PassType::Compute, RenderOrdering(0),
			[&] (QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<GenerationLayer>::create(builder, passBuilder, imageAttachment);
	});

	_activationLayer = builder.addPass("ActivationLayer", PassType::Compute, RenderOrdering(1),
			[&] (QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<ActivationLayer>::create(builder, passBuilder, imageAttachment, imageAttachment);
	});

	if (core::Queue::init(move(builder))) {
		_imageAttachment = imageAttachment;
		return true;
	}
	return false;
}

const core::AttachmentData *GenQueue::getGenDataAttachment() const {
	return static_cast<GenerationLayer *>(_genLayer->pass.get())->getDataAttachment();
}

const core::AttachmentData *GenQueue::getActivationDataAttachment() const {
	return static_cast<ActivationLayer *>(_activationLayer->pass.get())->getDataAttachment();
}

void GenQueue::run(Application *app, Extent3 e) {
	auto req = Rc<core::FrameRequest>::create(Rc<core::Queue>(this), core::FrameContraints{e});

	auto genInputData = Rc<GenerationDataInput>::alloc();
	genInputData->data = GenerationData{UVec3{0, 0, 0}, maxOf<uint16_t>(), -1.2f, 1.2f};

	req->addInput(getGenDataAttachment(), move(genInputData));

	auto activationInputData = Rc<ActivationDataInput>::alloc();
	activationInputData->data = ActivationData{UVec4{e.width, e.height, e.depth, 1}, Activation::SILU, -5.0f};

	req->addInput(getActivationDataAttachment(), move(activationInputData));

	req->setOutput(getImageAttachment(), [app] (core::FrameAttachmentData &data, bool success, Ref *) {
		app->getGlLoop()->captureImage([app] (core::ImageInfoData info, BytesView view) {
			if (!view.empty()) {
				for (size_t i = 0; i < info.extent.width; ++ i) {
					for (size_t i = 0; i < info.extent.height; ++ i) {
						for (size_t i = 0; i < info.extent.depth; ++ i) {
							std::cout << " " << view.readFloat16();
						}
						std::cout << "\n";
					}
					std::cout << "\n";
				}
			}
			app->end();
		}, data.image->getImage(), core::AttachmentLayout::General);
		return true;
	});

	app->getGlLoop()->runRenderQueue(move(req), 0);
}

}
