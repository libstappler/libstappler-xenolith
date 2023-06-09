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

#ifndef XENOLITH_BACKEND_VK_XLVKATTACHMENT_H_
#define XENOLITH_BACKEND_VK_XLVKATTACHMENT_H_

#include "XLVkSync.h"
#include "XLCoreAttachment.h"
#include "XLVkDeviceQueue.h"

namespace stappler::xenolith::vk {

class BufferAttachment : public core::BufferAttachment {
public:
	virtual ~BufferAttachment() { }
};

class ImageAttachment : public core::ImageAttachment {
public:
	virtual ~ImageAttachment() { }

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};


class BufferAttachmentHandle : public core::AttachmentHandle {
public:
	virtual ~BufferAttachmentHandle() { }

	virtual bool writeDescriptor(const core::QueuePassHandle &, DescriptorBufferInfo &) { return false; }
};

class ImageAttachmentHandle : public core::AttachmentHandle {
public:
	virtual ~ImageAttachmentHandle() { }

	const Rc<core::ImageStorage> &getImage() const;

	virtual bool writeDescriptor(const core::QueuePassHandle &, DescriptorImageInfo &);
	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &, uint32_t, bool isExternal) const;
};

class TexelAttachmentHandle : public core::AttachmentHandle {
public:
	virtual ~TexelAttachmentHandle();

	virtual bool writeDescriptor(const core::QueuePassHandle &, DescriptorBufferViewInfo &) { return false; }
};

}

#endif /* XENOLITH_BACKEND_VK_XLVKATTACHMENT_H_ */
