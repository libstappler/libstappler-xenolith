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

#include "XLSnnInputTest.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameRequest.h"
#include "XLVkLoop.h"
#include "XLApplication.h"
#include "SPBitmap.h"

namespace stappler::xenolith::vk::shadernn {

InputQueue::~InputQueue() { }

bool InputQueue::init() {
	using namespace core;
	Queue::Builder builder("Input");

	auto imageAttachment = builder.addAttachemnt("ImageAttachment",
			[&] (AttachmentBuilder &attachmentBuilder) -> Rc<Attachment> {
		return Rc<vk::ImageAttachment>::create(attachmentBuilder,
			ImageInfo(Extent2(1024, 1024),
					ImageUsage::Storage | ImageUsage::TransferSrc,
					ImageTiling::Optimal, ImageFormat::R8G8B8A8_UNORM, PassType::Compute),
			ImageAttachment::AttachmentInfo{
				.initialLayout = AttachmentLayout::Ignored,
				.finalLayout = AttachmentLayout::Ignored,
				.clearOnLoad = true,
				.clearColor = Color4F(0.0f, 0.0f, 0.0f, 0.0f)}
		);
	});

	auto outputAttachment = builder.addAttachemnt("OutputAttachment",
			[&] (AttachmentBuilder &attachmentBuilder) -> Rc<Attachment> {
		attachmentBuilder.defineAsOutput();
		return Rc<vk::ImageAttachment>::create(attachmentBuilder,
			ImageInfo(Extent3(1024, 1024, 1), ImageType::Image3D,
					ImageUsage::Storage | ImageUsage::TransferSrc,
					ImageTiling::Optimal, ImageFormat::R16G16B16A16_SFLOAT, PassType::Compute),
			ImageAttachment::AttachmentInfo{
				.initialLayout = AttachmentLayout::Ignored,
				.finalLayout = AttachmentLayout::Ignored,
				.clearOnLoad = true,
				.clearColor = Color4F(0.0f, 0.0f, 0.0f, 0.0f)}
		);
	});

	_inputLayer = builder.addPass("InputLayer", PassType::Compute, RenderOrdering(0),
			[&] (QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<InputLayer>::create(builder, passBuilder, imageAttachment, outputAttachment);
	});

	if (core::Queue::init(move(builder))) {
		_imageAttachment = imageAttachment;
		_outputAttachment = outputAttachment;
		return true;
	}
	return false;
}

const core::AttachmentData *InputQueue::getDataAttachment() const {
	return static_cast<InputLayer *>(_inputLayer->pass.get())->getDataAttachment();
}

void InputQueue::run(Application *app, StringView image) {
	Extent3 frameExtent(1, 1, 1);
	if (!bitmap::getImageSize(image, frameExtent.width, frameExtent.height)) {
		log::error("InputQueue", "fail to read image: ", image);
		return;
	}

	auto req = Rc<core::FrameRequest>::create(Rc<core::Queue>(this), core::FrameContraints{frameExtent});

	auto inputData = Rc<InputDataInput>::alloc();
	inputData->norm = NormData{
		Vec4(-0.5f, -0.5f, -0.5f, -0.5f), // to -0.5 - 0.5
		Vec4(2.0f, 2.0f, 2.0f, 2.0f) // to -1.0 - 1.0
	};
	inputData->image.extent = frameExtent;
	inputData->image.stdCallback = [path = image.str<Interface>()] (uint8_t *ptr, uint64_t size, const core::ImageData::DataCallback &dcb) {
		core::Resource::loadImageFileData(ptr, size, path, core::ImageFormat::R8G8B8A8_UNORM, dcb);
	};

	req->addInput(getDataAttachment(), move(inputData));

	req->setOutput(getOutputAttachment(), [app] (core::FrameAttachmentData &data, bool success, Ref *) {
		app->getGlLoop()->captureImage([app] (core::ImageInfoData info, BytesView view) {
			if (!view.empty()) {
				Bitmap bmp;
				bmp.alloc(info.extent.width, info.extent.height, bitmap::PixelFormat::RGBA8888, bitmap::AlphaFormat::Premultiplied);

				std::cout << view.size() << "\n";
				auto data = bmp.dataPtr();
				for (size_t i = 0; i < info.extent.width; ++ i) {
					for (size_t j = 0; j < info.extent.height; ++ j) {
						for (size_t k = 0; k < info.extent.depth; ++ k) {
							for (size_t f = 0; f < 4; ++ f) {
								*(data ++) = uint8_t((view.readFloat16() + 1.0f) * 127.5f);
							}
						}
					}
				}

				// bmp.save(toString(Time::now().toMicros(), ".png"));
			}
			app->end();
		}, data.image->getImage(), core::AttachmentLayout::General);
		return true;
	});

	app->getGlLoop()->runRenderQueue(move(req), 0);
}

}
