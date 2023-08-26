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

#include "XLVkObject.h"

namespace stappler::xenolith::vk {

bool DeviceMemory::init(Device &dev, VkDeviceMemory memory) {
	_memory = memory;

	return core::Object::init(dev, [] (core::Device *dev, core::ObjectType, ObjectHandle ptr) {
		auto d = ((Device *)dev);
		d->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
			table.vkFreeMemory(device, (VkDeviceMemory)ptr.get(), nullptr);
		});
	}, core::ObjectType::DeviceMemory, ObjectHandle(_memory));
}

bool Image::init(Device &dev, VkImage image, const ImageInfoData &info, uint32_t idx) {
	_info = info;
	_image = image;

	auto ret = core::ImageObject::init(dev, [] (core::Device *dev, core::ObjectType, ObjectHandle ptr) {
		// do nothing
	}, core::ObjectType::Image, ObjectHandle(_image));
	if (ret) {
		_index = idx;
	}
	return ret;
}

bool Image::init(Device &dev, VkImage image, const ImageInfoData &info, Rc<DeviceMemory> &&mem, Rc<core::DataAtlas> &&atlas) {
	_info = info;
	_image = image;
	_atlas = atlas;
	_memory = move(mem);

	return core::ImageObject::init(dev, [] (core::Device *dev, core::ObjectType, ObjectHandle ptr) {
		auto d = ((Device *)dev);
		d->getTable()->vkDestroyImage(d->getDevice(), (VkImage)ptr.get(), nullptr);
	}, core::ObjectType::Image, ObjectHandle(_image));
}

bool Image::init(Device &dev, uint64_t idx, VkImage image, const ImageInfoData &info, Rc<DeviceMemory> &&mem, Rc<core::DataAtlas> &&atlas) {
	_info = info;
	_image = image;
	_atlas = atlas;
	_memory = move(mem);

	return core::ImageObject::init(dev, [] (core::Device *dev, core::ObjectType, ObjectHandle ptr) {
		auto d = ((Device *)dev);
		d->getTable()->vkDestroyImage(d->getDevice(), (VkImage)ptr.get(), nullptr);
	}, core::ObjectType::Image, ObjectHandle(_image), idx);
}

void Image::setPendingBarrier(const ImageMemoryBarrier &barrier) {
	_barrier = barrier;
	_barrier->image = this;
}

const ImageMemoryBarrier *Image::getPendingBarrier() const {
	if (_barrier) {
		return &_barrier.value();
	} else {
		return nullptr;
	}
}

void Image::dropPendingBarrier() {
	_barrier.reset();
}

VkImageAspectFlags Image::getAspectMask() const {
	switch (core::getImagePixelFormat(_info.format)) {
	case core::PixelFormat::D: return VK_IMAGE_ASPECT_DEPTH_BIT; break;
	case core::PixelFormat::DS: return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT; break;
	case core::PixelFormat::S: return VK_IMAGE_ASPECT_STENCIL_BIT; break;
	default: return VK_IMAGE_ASPECT_COLOR_BIT; break;
	}
	return VK_IMAGE_ASPECT_NONE_KHR;
}

void Image::bindMemory(Rc<DeviceMemory> &&mem, VkDeviceSize offset) {
	auto dev = (Device *)_device;
	dev->getTable()->vkBindImageMemory(dev->getDevice(), _image, mem->getMemory(), offset);
	_memory = move(mem);
}

bool Buffer::init(Device &dev, VkBuffer buffer, const BufferInfo &info, Rc<DeviceMemory> &&mem) {
	_info = info;
	_buffer = buffer;
	_memory = move(mem);

	return core::BufferObject::init(dev, [] (core::Device *dev, core::ObjectType, ObjectHandle ptr) {
		auto d = ((Device *)dev);
		d->getTable()->vkDestroyBuffer(d->getDevice(), (VkBuffer)ptr.get(), nullptr);
	}, core::ObjectType::Buffer, ObjectHandle(_buffer));
}

void Buffer::setPendingBarrier(const BufferMemoryBarrier &barrier) {
	_barrier = barrier;
	_barrier->buffer = this;
}

const BufferMemoryBarrier *Buffer::getPendingBarrier() const {
	if (_barrier) {
		return &_barrier.value();
	} else {
		return nullptr;
	}
}

void Buffer::dropPendingBarrier() {
	_barrier.reset();
}

void Buffer::bindMemory(Rc<DeviceMemory> &&mem, VkDeviceSize offset) {
	auto dev = (Device *)_device;
	dev->getTable()->vkBindBufferMemory(dev->getDevice(), _buffer, mem->getMemory(), offset);
	_memory = move(mem);
}

bool ImageView::init(Device &dev, VkImage image, VkFormat format) {
	VkImageViewCreateInfo createInfo{}; sanitizeVkStruct(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = format;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;

	if (dev.getTable()->vkCreateImageView(dev.getDevice(), &createInfo, nullptr, &_imageView) == VK_SUCCESS) {
		return core::ImageView::init(dev, [] (core::Device *dev, core::ObjectType, ObjectHandle ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyImageView(d->getDevice(), (VkImageView)ptr.get(), nullptr);
		}, core::ObjectType::ImageView, ObjectHandle(_imageView));
	}
	return false;
}

bool ImageView::init(Device &dev, Image *image, const ImageViewInfo &info) {
	VkImageViewCreateInfo createInfo{}; sanitizeVkStruct(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image->getImage();

	switch (info.type) {
	case core::ImageViewType::ImageView1D:
		if (image->getInfo().imageType != core::ImageType::Image1D) {
			log::error("Vk-ImageView", "Incompatible ImageType '", core::getImageTypeName(image->getInfo().imageType),
					"' and ImageViewType '", core::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
		break;
	case core::ImageViewType::ImageView1DArray:
		if (image->getInfo().imageType != core::ImageType::Image1D) {
			log::error("Vk-ImageView", "Incompatible ImageType '", core::getImageTypeName(image->getInfo().imageType),
					"' and ImageViewType '", core::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
		break;
	case core::ImageViewType::ImageView2D:
		if (image->getInfo().imageType != core::ImageType::Image2D && image->getInfo().imageType != core::ImageType::Image3D) {
			log::error("Vk-ImageView", "Incompatible ImageType '", core::getImageTypeName(image->getInfo().imageType),
					"' and ImageViewType '", core::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		break;
	case core::ImageViewType::ImageView2DArray:
		if (image->getInfo().imageType != core::ImageType::Image2D && image->getInfo().imageType != core::ImageType::Image3D) {
			log::error("Vk-ImageView", "Incompatible ImageType '", core::getImageTypeName(image->getInfo().imageType),
					"' and ImageViewType '", core::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		break;
	case core::ImageViewType::ImageView3D:
		if (image->getInfo().imageType != core::ImageType::Image3D) {
			log::error("Vk-ImageView", "Incompatible ImageType '", core::getImageTypeName(image->getInfo().imageType),
					"' and ImageViewType '", core::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
		break;
	case core::ImageViewType::ImageViewCube:
		if (image->getInfo().imageType != core::ImageType::Image2D) {
			log::error("Vk-ImageView", "Incompatible ImageType '", core::getImageTypeName(image->getInfo().imageType),
					"' and ImageViewType '", core::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		break;
	case core::ImageViewType::ImageViewCubeArray:
		if (image->getInfo().imageType != core::ImageType::Image2D) {
			log::error("Vk-ImageView", "Incompatible ImageType '", core::getImageTypeName(image->getInfo().imageType),
					"' and ImageViewType '", core::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
		break;
	}

	auto format = info.format;
	if (format == core::ImageFormat::Undefined) {
		format = image->getInfo().format;
	}
	createInfo.format = VkFormat(format);

	createInfo.components.r = VkComponentSwizzle(info.r);
	createInfo.components.g = VkComponentSwizzle(info.g);
	createInfo.components.b = VkComponentSwizzle(info.b);
	createInfo.components.a = VkComponentSwizzle(info.a);

	switch (format) {
	case core::ImageFormat::D16_UNORM:
	case core::ImageFormat::X8_D24_UNORM_PACK32:
	case core::ImageFormat::D32_SFLOAT:
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	case core::ImageFormat::S8_UINT:
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
		break;
	case core::ImageFormat::D16_UNORM_S8_UINT:
	case core::ImageFormat::D24_UNORM_S8_UINT:
	case core::ImageFormat::D32_SFLOAT_S8_UINT:
		createInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	default:
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		break;
	}

	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = image->getInfo().mipLevels.get();
	createInfo.subresourceRange.baseArrayLayer = info.baseArrayLayer.get();
	if (info.layerCount.get() == maxOf<uint32_t>()) {
		createInfo.subresourceRange.layerCount = image->getInfo().arrayLayers.get() - info.baseArrayLayer.get();
	} else {
		createInfo.subresourceRange.layerCount = info.layerCount.get();
	}

	if (dev.getTable()->vkCreateImageView(dev.getDevice(), &createInfo, nullptr, &_imageView) == VK_SUCCESS) {
		_info = info;
		_info.format = format;
		_info.baseArrayLayer = core::BaseArrayLayer(createInfo.subresourceRange.baseArrayLayer);
		_info.layerCount = core::ArrayLayers(createInfo.subresourceRange.layerCount);

		_image = image;
		return core::ImageView::init(dev, [] (core::Device *dev, core::ObjectType, ObjectHandle ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyImageView(d->getDevice(), (VkImageView)ptr.get(), nullptr);
		}, core::ObjectType::ImageView, ObjectHandle(_imageView));
	}
	return false;
}

bool Sampler::init(Device &dev, const SamplerInfo &info) {
	VkSamplerCreateInfo createInfo; sanitizeVkStruct(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.magFilter = VkFilter(info.magFilter);
	createInfo.minFilter = VkFilter(info.minFilter);
	createInfo.mipmapMode = VkSamplerMipmapMode(info.mipmapMode);
	createInfo.addressModeU = VkSamplerAddressMode(info.addressModeU);
	createInfo.addressModeV = VkSamplerAddressMode(info.addressModeV);
	createInfo.addressModeW = VkSamplerAddressMode(info.addressModeW);
	createInfo.mipLodBias = info.mipLodBias;
	createInfo.anisotropyEnable = info.anisotropyEnable;
	createInfo.maxAnisotropy = info.maxAnisotropy;
	createInfo.compareEnable = info.compareEnable;
	createInfo.compareOp = VkCompareOp(info.compareOp);
	createInfo.minLod = info.minLod;
	createInfo.maxLod = info.maxLod;
	createInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	createInfo.unnormalizedCoordinates = false;

	if (dev.getTable()->vkCreateSampler(dev.getDevice(), &createInfo, nullptr, &_sampler) == VK_SUCCESS) {
		_info = info;
		return core::Object::init(dev, [] (core::Device *dev, core::ObjectType, ObjectHandle ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroySampler(d->getDevice(), (VkSampler)ptr.get(), nullptr);
		}, core::ObjectType::Sampler, ObjectHandle(_sampler));
	}
	return false;
}

}
