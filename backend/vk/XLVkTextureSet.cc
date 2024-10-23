/**
Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLVkTextureSet.h"
#include "XLVkLoop.h"
#include <forward_list>

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

bool TextureSetLayout::init(Device &dev, uint32_t imageLimit, uint32_t bufferLimit) {
	_imageCount = imageLimit;
	_bufferCount = bufferLimit;

	// create dummy image
	auto alloc = dev.getAllocator();
	_emptyImage = alloc->preallocate(ImageInfo(
			Extent2(1, 1), core::ImageUsage::Sampled, core::ImageFormat::R8_UNORM, core::EmptyTextureName), false);
	_solidImage = alloc->preallocate(ImageInfo(
			Extent2(1, 1), core::ImageUsage::Sampled, core::ImageFormat::R8_UNORM, core::SolidTextureName, core::ImageHints::Opaque), false);
	_emptyBuffer = alloc->preallocate(BufferInfo(uint64_t(8), core::BufferUsage::StorageBuffer));

	Rc<Image> images[] = {
		_emptyImage, _solidImage
	};

	alloc->emplaceObjects(AllocationUsage::DeviceLocal, makeSpanView(images, 2), SpanView<Rc<Buffer>>(&_emptyBuffer, 1));

	_emptyImageView = Rc<ImageView>::create(dev, _emptyImage, ImageViewInfo());
	_solidImageView = Rc<ImageView>::create(dev, _solidImage, ImageViewInfo());

	return true;
}

void TextureSetLayout::invalidate(Device &dev) {
	if (_layout) {
		dev.getTable()->vkDestroyDescriptorSetLayout(dev.getDevice(), _layout, nullptr);
		_layout = VK_NULL_HANDLE;
	}

	_emptyImage = nullptr;
	_emptyImageView = nullptr;
	_solidImage = nullptr;
	_solidImageView = nullptr;
}

bool TextureSetLayout::compile(Device &dev, const Vector<VkSampler> &samplers) {
	VkDescriptorSetLayoutBinding b[] = {
		VkDescriptorSetLayoutBinding{
			uint32_t(0),
			VK_DESCRIPTOR_TYPE_SAMPLER,
			uint32_t(samplers.size()),
			VK_SHADER_STAGE_FRAGMENT_BIT,
			samplers.data()
		},
		VkDescriptorSetLayoutBinding{
			uint32_t(1),
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			uint32_t(_imageCount),
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		},
		VkDescriptorSetLayoutBinding{
			uint32_t(2),
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			uint32_t(_bufferCount),
			VK_SHADER_STAGE_VERTEX_BIT,
			nullptr
		},
	};

	_samplersCount = uint32_t(samplers.size());

	VkDescriptorSetLayoutCreateInfo layoutInfo { };
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

		if (dev.getTable()->vkCreateDescriptorSetLayout(dev.getDevice(), &layoutInfo, nullptr, &_layout) != VK_SUCCESS) {
			return false;
		}

		_partiallyBound = true;

	} else {
		if (dev.getTable()->vkCreateDescriptorSetLayout(dev.getDevice(), &layoutInfo, nullptr, &_layout) != VK_SUCCESS) {
			return false;
		}
	}

	return true;
}

Rc<TextureSet> TextureSetLayout::acquireSet(Device &dev) {
	std::unique_lock<Mutex> lock(_mutex);
	if (_sets.empty()) {
		lock.unlock();
		return Rc<TextureSet>::create(dev, *this);
	} else {
		auto v = move(_sets.back());
		_sets.pop_back();
		return v;
	}
}

void TextureSetLayout::releaseSet(Rc<TextureSet> &&set) {
	std::unique_lock<Mutex> lock(_mutex);
	_sets.emplace_back(move(set));
}

void TextureSetLayout::initDefault(Device &dev, Loop &loop, Function<void(bool)> &&cb) {
	struct CompileTask : public Ref {
		Function<void(bool)> callback;
		Rc<Loop> loop;
		Device *device;

		Rc<CommandPool> pool;
		Rc<DeviceQueue> queue;
		Rc<Fence> fence;
	};

	auto task = new CompileTask();
	task->callback = move(cb);
	task->loop = &loop;
	task->device = &dev;

	dev.acquireQueue(QueueOperations::Graphics, loop, [this, task] (Loop &loop, const Rc<DeviceQueue> &queue) {
		task->queue = queue;
		task->fence = task->loop->acquireFence(0);
		task->pool = task->device->acquireCommandPool(QueueOperations::Graphics);

		auto refId = task->retain();
		task->fence->addRelease([task, refId] (bool success) {
			task->device->releaseCommandPool(*task->loop, move(task->pool));
			task->callback(success);
			task->release(refId);
		}, this, "TextureSetLayout::initDefault releaseCommandPool");

		loop.performInQueue(Rc<thread::Task>::create([this, task] (const thread::Task &) -> bool {
			auto buf = task->pool->recordBuffer(*task->device, [&, this] (CommandBuffer &buf) {
				writeDefaults(buf);
				return true;
			});

			if (task->queue->submit(*task->fence, buf)) {
				return true;
			}
			return false;
		}, [task] (const thread::Task &, bool success) mutable {
			if (task->queue) {
				task->device->releaseQueue(move(task->queue));
			}

			task->fence->schedule(*task->loop);
			task->fence = nullptr;
			task->release(0);
		}, this));
	}, [task] (Loop &) {
		task->callback(false);
		task->release(0);
	}, this);
}

Rc<Image> TextureSetLayout::getEmptyImageObject() const {
	return _emptyImage;
}

Rc<Image> TextureSetLayout::getSolidImageObject() const {
	return _solidImage;
}

void TextureSetLayout::readImage(Device &dev, Loop &loop, const Rc<Image> &image,
		AttachmentLayout l, Function<void(const ImageInfoData &, BytesView)> &&cb) {
	struct ReadImageTask : public Ref {
		Function<void(const ImageInfoData &, BytesView)> callback;
		Rc<Image> image;
		Rc<Loop> loop;
		Device *device;
		AttachmentLayout layout;

		Rc<Buffer> transferBuffer;
		Rc<CommandPool> pool;
		Rc<DeviceQueue> queue;
		Rc<Fence> fence;
		Rc<DeviceMemoryPool> mempool;
	};

	auto task = new ReadImageTask();
	task->callback = move(cb);
	task->image = image;
	task->loop = &loop;
	task->device = &dev;
	task->layout = l;

	task->loop->performOnGlThread([this, task] {
		task->device->acquireQueue(getQueueOperations(task->image->getInfo().type), *task->loop, [this, task] (Loop &loop, const Rc<DeviceQueue> &queue) {
			task->fence = loop.acquireFence(0);
			task->pool = task->device->acquireCommandPool(getQueueOperations(task->image->getInfo().type));
			task->queue = move(queue);
			task->mempool = Rc<DeviceMemoryPool>::create(task->device->getAllocator(), true);

			auto &info = task->image->getInfo();
			auto &extent = info.extent;

			task->transferBuffer = task->mempool->spawn(AllocationUsage::HostTransitionDestination, BufferInfo(
				core::ForceBufferUsage(core::BufferUsage::TransferDst),
				size_t(extent.width * extent.height * extent.depth * core::getFormatBlockSize(info.format)),
				task->image->getInfo().type
			));

			auto refId = task->retain();
			task->fence->addRelease([task, refId] (bool) {
				task->device->releaseCommandPool(*task->loop, move(task->pool));

				task->transferBuffer->map([&] (uint8_t *buf, VkDeviceSize size) {
					task->callback(task->image->getInfo(), BytesView(buf, size));
				});

				task->release(refId);
			}, this, "TextureSetLayout::readImage transferBuffer->dropPendingBarrier");

			loop.performInQueue(Rc<thread::Task>::create([this, task] (const thread::Task &) -> bool {
				auto buf = task->pool->recordBuffer(*task->device, [&, this] (CommandBuffer &buf) {
					writeImageRead(*task->device, buf, task->pool->getFamilyIdx(), task->image, task->layout, task->transferBuffer);
					return true;
				});

				if (task->queue->submit(*task->fence, buf)) {
					return true;
				}
				return false;
			}, [task] (const thread::Task &, bool success) {
				if (task->queue) {
					task->device->releaseQueue(move(task->queue));
				}
				if (!success) {
					task->callback(ImageInfo(), BytesView());
				}
				task->fence->schedule(*task->loop);
				task->fence = nullptr;
				task->release(0);
			}));
		}, [task] (Loop &) {
			task->callback(ImageInfo(), BytesView());
			task->release(0);
		});
	}, task, true);
}

void TextureSetLayout::readBuffer(Device &dev, Loop &loop, const Rc<Buffer> &buf, Function<void(const BufferInfo &, BytesView)> &&cb) {
	if (!buf) {
		log::error("vk::TextureSetLayout", "Buffer is null");
		return;
	}

	struct ReadBufferTask : public Ref {
		Function<void(const BufferInfo &, BytesView)> callback;
		Rc<Buffer> buffer;
		Rc<Loop> loop;
		Device *device;

		Rc<Buffer> transferBuffer;
		Rc<CommandPool> pool;
		Rc<DeviceQueue> queue;
		Rc<Fence> fence;
		Rc<DeviceMemoryPool> mempool;
	};

	auto task = new ReadBufferTask();
	task->callback = move(cb);
	task->buffer = buf;
	task->loop = &loop;
	task->device = &dev;
	task->mempool = buf->getMemory()->getPool();

	task->loop->performOnGlThread([this, task] {
		task->device->acquireQueue(getQueueOperations(task->buffer->getInfo().type), *task->loop, [this, task] (Loop &loop, const Rc<DeviceQueue> &queue) {
			task->fence = loop.acquireFence(0);
			task->pool = task->device->acquireCommandPool(getQueueOperations(task->buffer->getInfo().type));
			task->queue = move(queue);
			if (!task->mempool) {
				task->mempool = Rc<DeviceMemoryPool>::create(task->device->getAllocator(), true);
			}

			auto &info = task->buffer->getInfo();

			task->transferBuffer = task->mempool->spawn(AllocationUsage::HostTransitionDestination, BufferInfo(
				core::ForceBufferUsage(core::BufferUsage::TransferDst),
				size_t(info.size),
				info.type
			));

			auto refId = task->retain();
			task->fence->addRelease([task, refId] (bool) {
				task->device->releaseCommandPool(*task->loop, move(task->pool));

				task->transferBuffer->map([&] (uint8_t *buf, VkDeviceSize size) {
					task->callback(task->buffer->getInfo(), BytesView(buf, size));
				});

				task->release(refId);
			}, this, "TextureSetLayout::readImage transferBuffer->dropPendingBarrier");

			loop.performInQueue(Rc<thread::Task>::create([this, task] (const thread::Task &) -> bool {
				auto buf = task->pool->recordBuffer(*task->device, [&, this] (CommandBuffer &buf) {
					writeBufferRead(*task->device, buf, task->pool->getFamilyIdx(), task->buffer, task->transferBuffer);
					return true;
				});

				if (task->queue->submit(*task->fence, buf)) {
					return true;
				}
				return false;
			}, [task] (const thread::Task &, bool success) {
				if (task->queue) {
					task->device->releaseQueue(move(task->queue));
				}
				if (!success) {
					task->callback(BufferInfo(), BytesView());
				}
				task->fence->schedule(*task->loop);
				task->fence = nullptr;
				task->release(0);
			}));
		}, [task] (Loop &) {
			task->callback(BufferInfo(), BytesView());
			task->release(0);
		});
	}, task, true);
}

void TextureSetLayout::writeDefaults(CommandBuffer &buf) {
	std::array<ImageMemoryBarrier, 2> inImageBarriers;
	// define input barrier/transition (Undefined -> TransferDst)
	inImageBarriers[0] = ImageMemoryBarrier(_emptyImage,
		0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	inImageBarriers[1] = ImageMemoryBarrier(_solidImage,
		0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, inImageBarriers);

	buf.cmdClearColorImage(_emptyImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Color4F::ZERO);
	buf.cmdClearColorImage(_solidImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Color4F::ONE);
	buf.cmdFillBuffer(_emptyBuffer, 0xffffffffU);

	std::array<ImageMemoryBarrier, 2> outImageBarriers;
	outImageBarriers[0] = ImageMemoryBarrier(_emptyImage,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	outImageBarriers[1] = ImageMemoryBarrier(_solidImage,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	BufferMemoryBarrier outBufferBarrier(_emptyBuffer,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

	buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			makeSpanView(&outBufferBarrier, 1), outImageBarriers);
}

void TextureSetLayout::writeImageTransfer(Device &dev, CommandBuffer &buf, uint32_t qidx, const Rc<Buffer> &buffer, const Rc<Image> &image) {
	auto f = dev.getQueueFamily(image->getInfo().type);
	buf.writeImageTransfer(qidx, f ? f->index : VK_QUEUE_FAMILY_IGNORED, buffer, image);
}

void TextureSetLayout::writeImageRead(Device &dev, CommandBuffer &buf, uint32_t qidx, const Rc<Image> &image,
		AttachmentLayout layout, const Rc<Buffer> &target) {
	auto inImageBarrier = ImageMemoryBarrier(image,
		VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VkImageLayout(layout), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, makeSpanView(&inImageBarrier, 1));

	buf.cmdCopyImageToBuffer(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target, 0);

	BufferMemoryBarrier bufferOutBarrier(target, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

	buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, makeSpanView(&bufferOutBarrier, 1));
}

void TextureSetLayout::writeBufferRead(Device &dev, CommandBuffer &buf, uint32_t qidx, const Rc<Buffer> &source,
		const Rc<Buffer> &target) {
	buf.cmdCopyBuffer(source, target);
}

bool TextureSet::init(Device &dev, const TextureSetLayout &layout) {
	_imageCount = layout.getImageCount();
	_bufferCount = layout.getBuffersCount();

	VkDescriptorPoolSize poolSizes[] = {
		VkDescriptorPoolSize{
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			_imageCount
		},
		VkDescriptorPoolSize{
			VK_DESCRIPTOR_TYPE_SAMPLER,
			layout.getSamplersCount()
		},
		VkDescriptorPoolSize{
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			_bufferCount
		},
	};

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.flags = 0;
	poolInfo.poolSizeCount = sizeof(poolSizes) / sizeof(VkDescriptorPoolSize);
	poolInfo.pPoolSizes = poolSizes;
	poolInfo.maxSets = 1;

	if (dev.getTable()->vkCreateDescriptorPool(dev.getDevice(), &poolInfo, nullptr, &_pool) != VK_SUCCESS) {
		return false;
	}

	auto descriptorSetLayout = layout.getLayout();

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

	_layout = &layout;
	_partiallyBound = layout.isPartiallyBound();
	return core::Object::init(dev, [] (core::Device *dev, core::ObjectType, ObjectHandle ptr, void *) {
		auto d = ((Device *)dev);
		if (d->isPortabilityMode()) {
			auto pool = memory::pool::acquire();
			memory::pool::pre_cleanup_register(pool, [d, ptr = (VkDescriptorPool)ptr.get()] () {
				d->getTable()->vkDestroyDescriptorPool(d->getDevice(), ptr, nullptr);
			});
		} else {
			d->getTable()->vkDestroyDescriptorPool(d->getDevice(), (VkDescriptorPool)ptr.get(), nullptr);
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

void TextureSet::foreachPendingImageBarriers(const Callback<void(const ImageMemoryBarrier &)> &cb) const {
	for (auto &it : _pendingImageBarriers) {
		if (auto b = it->getPendingBarrier()) {
			cb(*b);
		}
	}
}

void TextureSet::foreachPendingBufferBarriers(const Callback<void(const BufferMemoryBarrier &)> &cb) const {
	for (auto &it : _pendingBufferBarriers) {
		if (auto b = it->getPendingBarrier()) {
			cb(*b);
		}
	}
}

void TextureSet::dropPendingBarriers() {
	for (auto &it : _pendingImageBarriers) {
		it->dropPendingBarrier();
	}
	for (auto &it : _pendingBufferBarriers) {
		it->dropPendingBarrier();
	}
	_pendingImageBarriers.clear();
	_pendingBufferBarriers.clear();
}

Device *TextureSet::getDevice() const {
	return (Device *)_object.device;
}

void TextureSet::writeImages(Vector<VkWriteDescriptorSet> &writes, const core::MaterialLayout &set,
		std::forward_list<Vector<VkDescriptorImageInfo>> &imagesList) {
	Vector<VkDescriptorImageInfo> *localImages = nullptr;

	VkWriteDescriptorSet imageWriteData({
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
		_set, // set
		1, // descriptor
		0, // index
		0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, nullptr,
		VK_NULL_HANDLE, VK_NULL_HANDLE
	});

	if (_partiallyBound) {
		_layoutIndexes.resize(set.usedImageSlots, 0);
	} else {
		_layoutIndexes.resize(set.imageSlots.size(), 0);
	}

	auto pushWritten = [&, this] {
		imageWriteData.descriptorCount = uint32_t(localImages->size());
		imageWriteData.pImageInfo = localImages->data();
		writes.emplace_back(move(imageWriteData));
		imageWriteData = VkWriteDescriptorSet ({
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
			_set, // set
			1, // descriptor
			0, // start from next index
			0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, nullptr,
			VK_NULL_HANDLE, VK_NULL_HANDLE
		});
		localImages = nullptr;
	};

	for (uint32_t i = 0; i < set.usedImageSlots; ++ i) {
		if (set.imageSlots[i].image && _layoutIndexes[i] != set.imageSlots[i].image->getIndex()) {
			if (!localImages) {
				localImages = &imagesList.emplace_front(Vector<VkDescriptorImageInfo>());
			}
			localImages->emplace_back(VkDescriptorImageInfo({
				VK_NULL_HANDLE, ((ImageView *)set.imageSlots[i].image.get())->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			}));
			auto image = (Image *)set.imageSlots[i].image->getImage().get();
			if (image->getPendingBarrier()) {
				_pendingImageBarriers.emplace_back(image);
			}
			_layoutIndexes[i] = set.imageSlots[i].image->getIndex();
			++ imageWriteData.descriptorCount;
		} else if (!_partiallyBound && !set.imageSlots[i].image && _layoutIndexes[i] != _layout->getEmptyImageView()->getIndex()) {
			if (!localImages) {
				localImages = &imagesList.emplace_front(Vector<VkDescriptorImageInfo>());
			}
			localImages->emplace_back(VkDescriptorImageInfo({
				VK_NULL_HANDLE, _layout->getEmptyImageView()->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			}));
			_layoutIndexes[i] = _layout->getEmptyImageView()->getIndex();
			++ imageWriteData.descriptorCount;
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
		for (uint32_t i = set.usedImageSlots; i < _imageCount; ++ i) {
			if (_layoutIndexes[i] != _layout->getEmptyImageView()->getIndex()) {
				if (!localImages) {
					localImages = &imagesList.emplace_front(Vector<VkDescriptorImageInfo>());
				}
				localImages->emplace_back(VkDescriptorImageInfo({
					VK_NULL_HANDLE, _layout->getEmptyImageView()->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				}));
				_layoutIndexes[i] = _layout->getEmptyImageView()->getIndex();
				++ imageWriteData.descriptorCount;
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

	VkWriteDescriptorSet bufferWriteData({
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
		_set, // set
		2, // descriptor
		0, // index
		0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr,
		VK_NULL_HANDLE, VK_NULL_HANDLE
	});

	if (_partiallyBound) {
		_layoutBuffers.resize(set.usedBufferSlots, nullptr);
	} else {
		_layoutBuffers.resize(set.bufferSlots.size(), nullptr);
	}

	auto pushWritten = [&, this] {
		bufferWriteData.descriptorCount = uint32_t(localBuffers->size());
		bufferWriteData.pBufferInfo = localBuffers->data();
		writes.emplace_back(move(bufferWriteData));
		bufferWriteData = VkWriteDescriptorSet ({
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
			_set, // set
			2, // descriptor
			0, // start from next index
			0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr,
			VK_NULL_HANDLE, VK_NULL_HANDLE
		});
		localBuffers = nullptr;
	};

	for (uint32_t i = 0; i < set.usedBufferSlots; ++ i) {
		if (set.bufferSlots[i].buffer && _layoutBuffers[i] != set.bufferSlots[i].buffer) {
			// replace old buffer in descriptor
			if (!localBuffers) {
				localBuffers = &bufferList.emplace_front(Vector<VkDescriptorBufferInfo>());
			}
			localBuffers->emplace_back(VkDescriptorBufferInfo({
				((Buffer *)set.bufferSlots[i].buffer.get())->getBuffer(), 0, VK_WHOLE_SIZE
			}));
			auto buffer = (Buffer *)set.bufferSlots[i].buffer.get();

			// propagate barrier, if any
			if (buffer->getPendingBarrier()) {
				_pendingBufferBarriers.emplace_back(buffer);
			}
			_layoutBuffers[i] = set.bufferSlots[i].buffer;
			++ bufferWriteData.descriptorCount;
		} else if (!_partiallyBound && !set.bufferSlots[i].buffer && _layoutBuffers[i] != _layout->getEmptyBuffer()) {
			// if partiallyBound feature is not available, drop old buffers to preallocated empty buffer
			if (!localBuffers) {
				localBuffers = &bufferList.emplace_front(Vector<VkDescriptorBufferInfo>());
			}
			localBuffers->emplace_back(VkDescriptorBufferInfo({
				_layout->getEmptyBuffer()->getBuffer(), 0, VK_WHOLE_SIZE
			}));
			_layoutBuffers[i] = _layout->getEmptyBuffer();
			++ bufferWriteData.descriptorCount;
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
		for (uint32_t i = set.usedBufferSlots; i < _bufferCount; ++ i) {
			if (_layoutBuffers[i] != _layout->getEmptyBuffer()) {
				if (!localBuffers) {
					localBuffers = &bufferList.emplace_front(Vector<VkDescriptorBufferInfo>());
				}
				localBuffers->emplace_back(VkDescriptorBufferInfo({
					_layout->getEmptyBuffer()->getBuffer(), 0, VK_WHOLE_SIZE
				}));
				_layoutBuffers[i] = _layout->getEmptyBuffer();
				++ bufferWriteData.descriptorCount;
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

}
