/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "XLVkTextureSet.h"
#include "XLVkLoop.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

bool TextureSetLayout::init(Device &dev, const core::TextureSetLayoutData &data) {
	auto &devInfo = dev.getInfo();

	uint32_t maxImageCount = data.imageCount;
	uint32_t maxBufferCount = data.bufferCount;
	if (dev.getInfo().features.deviceDescriptorIndexing.descriptorBindingPartiallyBound) {
		maxImageCount = data.imageCountIndexed;
		maxBufferCount = data.bufferCountIndexed;
	}

	auto &limits = devInfo.properties.device10.properties.limits;

	auto maxResources = limits.maxPerStageResources - 8;

	auto imageLimit = std::min(limits.maxPerStageDescriptorSampledImages,
			limits.maxDescriptorSetSampledImages);
	auto bufferLimit = std::min(limits.maxPerStageDescriptorStorageBuffers,
			limits.maxDescriptorSetStorageBuffers);

	_imageCount = imageLimit = std::min(imageLimit - 2, maxImageCount);
	_bufferCount = bufferLimit = std::min(bufferLimit - 4, maxBufferCount);

	if (_imageCount + _bufferCount > maxResources) {
		auto v = maxResources / 4;
		_imageCount = imageLimit = v * 3;
		_bufferCount = bufferLimit = v;
	}

	if (!devInfo.features.device10.features.shaderSampledImageArrayDynamicIndexing) {
		_imageCount = imageLimit = 1;
	}

	Vector<VkSampler> vkSamplers;
	for (auto &it : data.compiledSamplers) {
		vkSamplers.emplace_back(it.get_cast<Sampler>()->getSampler());
	}

	_samplersCount = uint32_t(data.compiledSamplers.size());

	VkDescriptorSetLayoutBinding b[] = {
		VkDescriptorSetLayoutBinding{uint32_t(0), VK_DESCRIPTOR_TYPE_SAMPLER,
			uint32_t(vkSamplers.size()), VK_SHADER_STAGE_FRAGMENT_BIT, vkSamplers.data()},
		VkDescriptorSetLayoutBinding{uint32_t(1), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			uint32_t(_imageCount), VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		VkDescriptorSetLayoutBinding{uint32_t(2), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			uint32_t(_bufferCount), VK_SHADER_STAGE_VERTEX_BIT, nullptr},
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;
	layoutInfo.bindingCount = 3;
	layoutInfo.pBindings = b;
	layoutInfo.flags = 0;

	if (dev.getInfo().features.deviceDescriptorIndexing.descriptorBindingPartiallyBound) {
		Vector<VkDescriptorBindingFlags> flags;
		flags.emplace_back(0);
		flags.emplace_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT);
		flags.emplace_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT);

		VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags;
		bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
		bindingFlags.pNext = nullptr;
		bindingFlags.bindingCount = uint32_t(flags.size());
		bindingFlags.pBindingFlags = flags.data();
		layoutInfo.pNext = &bindingFlags;

		if (dev.getTable()->vkCreateDescriptorSetLayout(dev.getDevice(), &layoutInfo, nullptr,
					&_layout)
				!= VK_SUCCESS) {
			return false;
		}

		_partiallyBound = true;

	} else {
		if (dev.getTable()->vkCreateDescriptorSetLayout(dev.getDevice(), &layoutInfo, nullptr,
					&_layout)
				!= VK_SUCCESS) {
			return false;
		}
	}

	_emptyImage = data.emptyImage;
	_solidImage = data.solidImage;
	_emptyBuffer = data.emptyBuffer;

	return core::Object::init(dev,
			[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *data) {
		auto d = ((Device *)dev);
		if (d->isPortabilityMode()) {
			auto pool = memory::pool::acquire();
			memory::pool::pre_cleanup_register(pool, [d, ptr = (VkDescriptorSetLayout)ptr.get()]() {
				d->getTable()->vkDestroyDescriptorSetLayout(d->getDevice(), ptr, nullptr);
			});
		} else {
			d->getTable()->vkDestroyDescriptorSetLayout(d->getDevice(),
					(VkDescriptorSetLayout)ptr.get(), nullptr);
		}
	}, core::ObjectType::DescriptorSetLayout, ObjectHandle(_layout));

	return true;
}

bool TextureSet::init(Device &dev, const core::TextureSetLayout &layout) {
	_imageCount = layout.getImageCount();
	_bufferCount = layout.getBuffersCount();

	VkDescriptorPoolSize poolSizes[] = {
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, _imageCount},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, layout.getSamplersCount()},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _bufferCount},
	};

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.flags = 0;
	poolInfo.poolSizeCount = sizeof(poolSizes) / sizeof(VkDescriptorPoolSize);
	poolInfo.pPoolSizes = poolSizes;
	poolInfo.maxSets = 1;

	if (dev.getTable()->vkCreateDescriptorPool(dev.getDevice(), &poolInfo, nullptr, &_pool)
			!= VK_SUCCESS) {
		return false;
	}

	auto descriptorSetLayout = static_cast<const TextureSetLayout &>(layout).getLayout();

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.descriptorPool = _pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptorSetLayout;

	auto err = dev.getTable()->vkAllocateDescriptorSets(dev.getDevice(), &allocInfo, &_set);
	if (err != VK_SUCCESS) {
		dev.getTable()->vkDestroyDescriptorPool(dev.getDevice(), _pool, nullptr);
		return false;
	}

	_layout = &static_cast<const TextureSetLayout &>(layout);
	_partiallyBound = layout.isPartiallyBound();
	return core::Object::init(dev,
			[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *) {
		auto d = ((Device *)dev);
		if (d->isPortabilityMode()) {
			auto pool = memory::pool::acquire();
			memory::pool::pre_cleanup_register(pool, [d, ptr = (VkDescriptorPool)ptr.get()]() {
				d->getTable()->vkDestroyDescriptorPool(d->getDevice(), ptr, nullptr);
			});
		} else {
			d->getTable()->vkDestroyDescriptorPool(d->getDevice(), (VkDescriptorPool)ptr.get(),
					nullptr);
		}
	}, core::ObjectType::DescriptorPool, ObjectHandle(_pool));
}

void TextureSet::write(const core::MaterialLayout &set) {
	auto table = ((Device *)_object.device)->getTable();
	auto dev = ((Device *)_object.device)->getDevice();

	std::forward_list<Vector<VkDescriptorImageInfo>> imagesList;
	std::forward_list<Vector<VkDescriptorBufferInfo>> buffersList;
	Vector<VkWriteDescriptorSet> writes;

	writeImages(writes, set, imagesList);
	writeBuffers(writes, set, buffersList);

	table->vkUpdateDescriptorSets(dev, uint32_t(writes.size()), writes.data(), 0, nullptr);
}

Vector<const ImageMemoryBarrier *> TextureSet::getPendingImageBarriers() const {
	Vector<const ImageMemoryBarrier *> ret;
	for (auto &it : _pendingImageBarriers) {
		if (auto b = it->getPendingBarrier()) {
			ret.emplace_back(b);
		}
	}
	return ret;
}

Vector<const BufferMemoryBarrier *> TextureSet::getPendingBufferBarriers() const {
	Vector<const BufferMemoryBarrier *> ret;
	for (auto &it : _pendingBufferBarriers) {
		if (auto b = it->getPendingBarrier()) {
			ret.emplace_back(b);
		}
	}
	return ret;
}

void TextureSet::foreachPendingImageBarriers(const Callback<void(const ImageMemoryBarrier &)> &cb,
		bool drain) const {
	for (auto &it : _pendingImageBarriers) {
		if (auto b = it->getPendingBarrier()) {
			cb(*b);
			if (drain) {
				it->dropPendingBarrier();
			}
		}
	}
}

void TextureSet::foreachPendingBufferBarriers(const Callback<void(const BufferMemoryBarrier &)> &cb,
		bool drain) const {
	for (auto &it : _pendingBufferBarriers) {
		if (auto b = it->getPendingBarrier()) {
			cb(*b);
			if (drain) {
				it->dropPendingBarrier();
			}
		}
	}
}

void TextureSet::dropPendingBarriers() {
	for (auto &it : _pendingImageBarriers) { it->dropPendingBarrier(); }
	for (auto &it : _pendingBufferBarriers) { it->dropPendingBarrier(); }
	_pendingImageBarriers.clear();
	_pendingBufferBarriers.clear();
}

Device *TextureSet::getDevice() const { return (Device *)_object.device; }

void TextureSet::writeImages(Vector<VkWriteDescriptorSet> &writes, const core::MaterialLayout &set,
		std::forward_list<Vector<VkDescriptorImageInfo>> &imagesList) {
	Vector<VkDescriptorImageInfo> *localImages = nullptr;

	VkWriteDescriptorSet imageWriteData({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
		_set, // set
		1, // descriptor
		0, // index
		0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, nullptr, VK_NULL_HANDLE, VK_NULL_HANDLE});

	if (_partiallyBound) {
		_layoutIndexes.resize(set.usedImageSlots, 0);
	} else {
		_layoutIndexes.resize(set.imageSlots.size(), 0);
	}

	auto pushWritten = [&, this] {
		imageWriteData.descriptorCount = uint32_t(localImages->size());
		imageWriteData.pImageInfo = localImages->data();
		writes.emplace_back(move(imageWriteData));
		imageWriteData = VkWriteDescriptorSet({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
			_set, // set
			1, // descriptor
			0, // start from next index
			0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, nullptr, VK_NULL_HANDLE, VK_NULL_HANDLE});
		localImages = nullptr;
	};

	auto emptyImageView = _layout->getEmptyImage()->views.front();

	for (uint32_t i = 0; i < set.usedImageSlots; ++i) {
		if (set.imageSlots[i].image && _layoutIndexes[i] != set.imageSlots[i].image->getIndex()) {
			if (!localImages) {
				localImages = &imagesList.emplace_front(Vector<VkDescriptorImageInfo>());
			}
			localImages->emplace_back(VkDescriptorImageInfo(
					{VK_NULL_HANDLE, set.imageSlots[i].image.get_cast<ImageView>()->getImageView(),
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}));
			auto image = (Image *)set.imageSlots[i].image->getImage().get();
			if (image->getPendingBarrier()) {
				_pendingImageBarriers.emplace_back(image);
			}
			_layoutIndexes[i] = set.imageSlots[i].image->getIndex();
			++imageWriteData.descriptorCount;
		} else if (!_partiallyBound && !set.imageSlots[i].image
				&& _layoutIndexes[i] != emptyImageView->view->getIndex()) {
			if (!localImages) {
				localImages = &imagesList.emplace_front(Vector<VkDescriptorImageInfo>());
			}
			localImages->emplace_back(VkDescriptorImageInfo(
					{VK_NULL_HANDLE, emptyImageView->view.get_cast<ImageView>()->getImageView(),
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}));
			_layoutIndexes[i] = emptyImageView->view->getIndex();
			++imageWriteData.descriptorCount;
		} else {
			// no need to write, push written
			if (imageWriteData.descriptorCount > 0) {
				pushWritten();
			}
			imageWriteData.dstArrayElement = i + 1; // start from next index
		}
	}

	if (!_partiallyBound) {
		// write empty
		for (uint32_t i = set.usedImageSlots; i < _imageCount; ++i) {
			if (_layoutIndexes[i] != emptyImageView->view->getIndex()) {
				if (!localImages) {
					localImages = &imagesList.emplace_front(Vector<VkDescriptorImageInfo>());
				}
				localImages->emplace_back(VkDescriptorImageInfo(
						{VK_NULL_HANDLE, emptyImageView->view.get_cast<ImageView>()->getImageView(),
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}));
				_layoutIndexes[i] = emptyImageView->view->getIndex();
				++imageWriteData.descriptorCount;
			} else {
				// no need to write, push written
				if (imageWriteData.descriptorCount > 0) {
					pushWritten();
				}
				imageWriteData.dstArrayElement = i + 1; // start from next index
			}
		}
	}

	if (imageWriteData.descriptorCount > 0) {
		imageWriteData.descriptorCount = uint32_t(localImages->size());
		imageWriteData.pImageInfo = localImages->data();
		writes.emplace_back(move(imageWriteData));
	}
}

void TextureSet::writeBuffers(Vector<VkWriteDescriptorSet> &writes, const core::MaterialLayout &set,
		std::forward_list<Vector<VkDescriptorBufferInfo>> &bufferList) {
	Vector<VkDescriptorBufferInfo> *localBuffers = nullptr;

	VkWriteDescriptorSet bufferWriteData({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
		_set, // set
		2, // descriptor
		0, // index
		0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, VK_NULL_HANDLE, VK_NULL_HANDLE});

	if (_partiallyBound) {
		_layoutBuffers.resize(set.usedBufferSlots, nullptr);
	} else {
		_layoutBuffers.resize(set.bufferSlots.size(), nullptr);
	}

	auto pushWritten = [&, this] {
		bufferWriteData.descriptorCount = uint32_t(localBuffers->size());
		bufferWriteData.pBufferInfo = localBuffers->data();
		writes.emplace_back(move(bufferWriteData));
		bufferWriteData = VkWriteDescriptorSet({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
			_set, // set
			2, // descriptor
			0, // start from next index
			0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, VK_NULL_HANDLE, VK_NULL_HANDLE});
		localBuffers = nullptr;
	};

	auto emptyBuffer = _layout->getEmptyBuffer()->buffer.get_cast<Buffer>();

	for (uint32_t i = 0; i < set.usedBufferSlots; ++i) {
		if (set.bufferSlots[i].buffer && _layoutBuffers[i] != set.bufferSlots[i].buffer) {
			// replace old buffer in descriptor
			if (!localBuffers) {
				localBuffers = &bufferList.emplace_front(Vector<VkDescriptorBufferInfo>());
			}
			localBuffers->emplace_back(VkDescriptorBufferInfo(
					{((Buffer *)set.bufferSlots[i].buffer.get())->getBuffer(), 0, VK_WHOLE_SIZE}));
			auto buffer = (Buffer *)set.bufferSlots[i].buffer.get();

			// propagate barrier, if any
			if (buffer->getPendingBarrier()) {
				_pendingBufferBarriers.emplace_back(buffer);
			}
			_layoutBuffers[i] = set.bufferSlots[i].buffer;
			++bufferWriteData.descriptorCount;
		} else if (!_partiallyBound && !set.bufferSlots[i].buffer
				&& _layoutBuffers[i] != emptyBuffer) {
			// if partiallyBound feature is not available, drop old buffers to preallocated empty buffer
			if (!localBuffers) {
				localBuffers = &bufferList.emplace_front(Vector<VkDescriptorBufferInfo>());
			}
			localBuffers->emplace_back(
					VkDescriptorBufferInfo({emptyBuffer->getBuffer(), 0, VK_WHOLE_SIZE}));
			_layoutBuffers[i] = emptyBuffer;
			++bufferWriteData.descriptorCount;
		} else {
			// descriptor was not changed, no need to write, push written
			if (bufferWriteData.descriptorCount > 0) {
				pushWritten();
			}
			bufferWriteData.dstArrayElement = i + 1; // start from next index
		}
	}

	if (!_partiallyBound) {
		// write empty buffers into empty descriptors
		for (uint32_t i = set.usedBufferSlots; i < _bufferCount; ++i) {
			if (_layoutBuffers[i] != emptyBuffer) {
				if (!localBuffers) {
					localBuffers = &bufferList.emplace_front(Vector<VkDescriptorBufferInfo>());
				}
				localBuffers->emplace_back(
						VkDescriptorBufferInfo({emptyBuffer->getBuffer(), 0, VK_WHOLE_SIZE}));
				_layoutBuffers[i] = emptyBuffer;
				++bufferWriteData.descriptorCount;
			} else {
				// no need to write, push written
				if (bufferWriteData.descriptorCount > 0) {
					pushWritten();
				}
				bufferWriteData.dstArrayElement = i + 1; // start from next index
			}
		}
	}

	if (bufferWriteData.descriptorCount > 0) {
		bufferWriteData.descriptorCount = uint32_t(localBuffers->size());
		bufferWriteData.pBufferInfo = localBuffers->data();
		writes.emplace_back(move(bufferWriteData));
	}
}

} // namespace stappler::xenolith::vk
