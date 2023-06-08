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

#ifndef XENOLITH_BACKEND_VK_XLVKOBJECT_H_
#define XENOLITH_BACKEND_VK_XLVKOBJECT_H_

#include "XLVkDevice.h"
#include "XLCoreObject.h"
#include "XLCoreImageStorage.h"

namespace stappler::xenolith::vk {

class Surface;
class SwapchainImage;

class DeviceMemory : public core::Object {
public:
	virtual ~DeviceMemory() { }

	bool init(Device &dev, VkDeviceMemory);

	VkDeviceMemory getMemory() const { return _memory; }

protected:
	using core::Object::init;

	VkDeviceMemory _memory = VK_NULL_HANDLE;
};

class Image : public core::ImageObject {
public:
	virtual ~Image() { }

	// non-owining image wrapping
	bool init(Device &dev, VkImage, const ImageInfo &, uint32_t);

	// owning image wrapping
	bool init(Device &dev, VkImage, const ImageInfo &, Rc<DeviceMemory> &&, Rc<core::DataAtlas> && = Rc<core::DataAtlas>());
	bool init(Device &dev, uint64_t, VkImage, const ImageInfo &, Rc<DeviceMemory> &&, Rc<core::DataAtlas> && = Rc<core::DataAtlas>());

	VkImage getImage() const { return _image; }
	DeviceMemory *getMemory() const { return _memory; }

	void setPendingBarrier(const ImageMemoryBarrier &);
	const ImageMemoryBarrier *getPendingBarrier() const;
	void dropPendingBarrier();

	VkImageAspectFlags getAspectMask() const;

	void bindMemory(Rc<DeviceMemory> &&, VkDeviceSize = 0);

protected:
	using core::ImageObject::init;

	Rc<DeviceMemory> _memory;
	VkImage _image = VK_NULL_HANDLE;
	std::optional<ImageMemoryBarrier> _barrier;
};

class Buffer : public core::BufferObject {
public:
	virtual ~Buffer() { }

	bool init(Device &dev, VkBuffer, const BufferInfo &, Rc<DeviceMemory> &&);

	VkBuffer getBuffer() const { return _buffer; }
	DeviceMemoryPool *getPool() const { return _pool; }
	DeviceMemory *getMemory() const { return _memory; }

	void setPendingBarrier(const BufferMemoryBarrier &);
	const BufferMemoryBarrier *getPendingBarrier() const;
	void dropPendingBarrier();

	void bindMemory(Rc<DeviceMemory> &&, VkDeviceSize = 0);

protected:
	using core::BufferObject::init;

	Rc<DeviceMemory> _memory;
	VkBuffer _buffer = VK_NULL_HANDLE;
	std::optional<BufferMemoryBarrier> _barrier;

	DeviceMemoryPool *_pool = nullptr;
};

class ImageView : public core::ImageView {
public:
	virtual ~ImageView() { }

	bool init(Device &dev, VkImage, VkFormat format);
	bool init(Device &dev, const core::AttachmentPassData &desc, Image *);
	bool init(Device &dev, Image *, const ImageViewInfo &);

	VkImageView getImageView() const { return _imageView; }

protected:
	using core::ImageView::init;

	VkImageView _imageView = VK_NULL_HANDLE;
};

class Sampler : public core::Sampler {
public:
	virtual ~Sampler() { }

	bool init(Device &dev, const SamplerInfo &);

	VkSampler getSampler() const { return _sampler; }

protected:
	using core::Sampler::init;

	VkSampler _sampler = VK_NULL_HANDLE;
};

}

#endif /* XENOLITH_BACKEND_VK_XLVKOBJECT_H_ */
