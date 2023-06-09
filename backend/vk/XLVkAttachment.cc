/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLVkAttachment.h"
#include "XLVkDevice.h"
#include "XLVkTextureSet.h"
#include "XLCoreFrameQueue.h"

namespace stappler::xenolith::vk {

auto ImageAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<ImageAttachmentHandle>::create(this, handle);
}

const Rc<core::ImageStorage> &ImageAttachmentHandle::getImage() const {
	return _queueData->image;
}

bool ImageAttachmentHandle::writeDescriptor(const core::QueuePassHandle &queue, DescriptorImageInfo &info) {
	auto &image = _queueData->image;

	bool allowSwizzle = (info.descriptor->type == core::DescriptorType::SampledImage);
	ImageViewInfo viewInfo(image->getInfo());
	viewInfo.setup(info.descriptor->attachment->colorMode, allowSwizzle);
	if (auto view = image->getView(viewInfo)) {
		info.layout = VkImageLayout(info.descriptor->layout);
		info.imageView = (ImageView *)view.get();
		return true;
	}

	return false;
}

bool ImageAttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &d, uint32_t, bool isExternal) const {
	return getImage();
}

}
