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

#include "XLSnnModelTest.h"
#include "XLSnnModel.h"
#include "XLSnnLayer.h"
#include "XLSnnAttachment.h"
#include "XLSnnVkInputLayer.h"
#include "XLVkAttachment.h"
#include "XLCoreFrameRequest.h"
#include "SPFilesystem.h"
#include "SPBitmap.h"

namespace stappler::xenolith::shadernn {

bool ModelQueue::init(StringView modelPath, ModelFlags flags, StringView input) {
	_processor = Rc<ModelProcessor>::create();
	_model = _processor->load(FilePath(modelPath), flags);

	if (!_model) {
		return false;
	}

	core::Queue::Builder builder(filepath::name(modelPath));

	Extent3 frameExtent(1, 1, 1);
	if (!bitmap::getImageSize(input, frameExtent.width, frameExtent.height)) {
		log::error("InputQueue", "fail to read image: ", input);
		return false;
	}

	_image = input.str<Interface>();

	Map<Layer *, const core::AttachmentData *> inputs;
	Map<Attachment *, const core::AttachmentData *> attachments;

	auto modelInputs = _model->getInputs();
	for (auto &it : modelInputs) {
		auto attachment = builder.addAttachemnt(toString(it->getName(), "_input"),
				[&] (core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
			return Rc<vk::ImageAttachment>::create(attachmentBuilder,
				core::ImageInfo(Extent2(it->getOutputExtent().width, it->getOutputExtent().height),
						core::ImageUsage::Storage | core::ImageUsage::TransferSrc,
						core::ImageTiling::Optimal, core::ImageFormat::R8G8B8A8_UNORM, core::PassType::Compute),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = core::AttachmentLayout::Ignored,
					.finalLayout = core::AttachmentLayout::Ignored,
					.clearOnLoad = true,
					.clearColor = Color4F(0.0f, 0.0f, 0.0f, 0.0f)}
			);
		});
		inputs.emplace(it, attachment);
	}

	auto modelFormat = _model->isHalfPrecision() ? core::ImageFormat::R16G16B16A16_SFLOAT : core::ImageFormat::R32G32B32A32_SFLOAT;

	size_t i = 0;
	for (auto &it : _model->getSortedLayers()) {
		auto output = it->getOutput();
		auto attachment = builder.addAttachemnt(output->getName(),
				[&] (core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
			if (_model->getSortedLayers().size() == i + 1) {
				attachmentBuilder.defineAsOutput();
			}
			return Rc<vk::ImageAttachment>::create(attachmentBuilder,
					core::ImageInfo(output->getExtent(), core::ImageType::Image3D,
							core::ImageUsage::Storage | core::ImageUsage::TransferSrc,
							core::ImageTiling::Optimal, modelFormat, core::PassType::Compute),
					core::ImageAttachment::AttachmentInfo{
						.initialLayout = core::AttachmentLayout::Ignored,
						.finalLayout = core::AttachmentLayout::Ignored }
			);
		});
		attachments.emplace(output, attachment);
		if (_model->getSortedLayers().size() == i + 1) {
			_outputAttachment = attachment;
		}
		++ i;
	}

	for (auto &it : _model->getSortedLayers()) {
		auto ret = it->prepare(builder, inputs, attachments);
		if (!ret) {
			return false;
		}
		if (it->isInput()) {
			_inputAttachments.emplace_back(static_cast<vk::shadernn::InputLayer *>(ret->pass.get())->getDataAttachment());
		}
	}

	return Queue::init(move(builder));
}

void ModelQueue::run(Application *app) {
	Extent3 frameExtent(1, 1, 1);
	if (!bitmap::getImageSize(StringView(_image), frameExtent.width, frameExtent.height)) {
		log::error("InputQueue", "fail to read image: ", _image);
		return;
	}


	auto req = Rc<core::FrameRequest>::create(Rc<core::Queue>(this), core::FrameContraints{frameExtent});

	ModelSpecialization spec = _processor->specializeModel(_model, frameExtent);

	for (auto &it : spec.attachments) {
		if (auto a = getAttachment(it.first->getName())) {
			if (auto img = dynamic_cast<vk::ImageAttachment *>(a->attachment.get())) {
				core::ImageInfoData info = img->getImageInfo();
				info.extent = it.second;
				log::debug("ModelQueue", "Specialize attachment ", it.first->getName(), " for extent ", info.extent);
				req->addImageSpecialization(img, move(info));
			}
		}
	}

	for (auto &it : _inputAttachments) {
		auto inputData = Rc<vk::shadernn::InputDataInput>::alloc();
		inputData->norm = vk::shadernn::NormData{
			Vec4(-0.5f, -0.5f, -0.5f, -0.5f), // to -0.5 - 0.5
			Vec4(2.0f, 2.0f, 2.0f, 2.0f) // to -1.0 - 1.0
		};
		inputData->image.extent = frameExtent;
		inputData->image.stdCallback = [path = _image] (uint8_t *ptr, uint64_t size, const core::ImageData::DataCallback &dcb) {
			core::Resource::loadImageFileData(ptr, size, path, core::ImageFormat::R8G8B8A8_UNORM, dcb);
		};

		req->addInput(it, move(inputData));
	}

	req->setOutput(getOutputAttachment(), [app] (core::FrameAttachmentData &data, bool success, Ref *) {
		app->getGlLoop()->captureImage([app] (core::ImageInfoData info, BytesView view) {
			if (!view.empty()) {
				Vector<Bitmap> planes;
				for (size_t i = 0; i < info.extent.depth; ++ i) {
					auto &b = planes.emplace_back(Bitmap());
					b.alloc(info.extent.width, info.extent.height, bitmap::PixelFormat::RGBA8888, bitmap::AlphaFormat::Premultiplied);
				}

				Vector<uint8_t *> ptrs;
				for (auto &it : planes) {
					ptrs.emplace_back(it.dataPtr());
				}

				std::cout << view.size() << " ";

				for (size_t i = 0; i < info.extent.width; ++ i) {
					for (size_t j = 0; j < info.extent.height; ++ j) {
						for (size_t k = 0; k < info.extent.depth; ++ k) {
							for (size_t f = 0; f < 4; ++ f) {
								*ptrs[k] = uint8_t((view.readFloat32() + 1.0f) * 127.5f);
								if (f == 3) {
									//*ptrs[k] = 255;
								}
								ptrs[k] ++;
							}
						}
					}
				}

				std::cout << view.size() << "\n";

				for (size_t i = 0; i < info.extent.depth; ++ i) {
					planes[i].save(toString(i, "_", Time::now().toMicros(), ".png"));
				}
			}
			app->end();
		}, data.image->getImage(), core::AttachmentLayout::General);
		return true;
	});

	app->getGlLoop()->runRenderQueue(move(req), 0);
}

}
