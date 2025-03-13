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

#include "XLVkDeviceQueue.h"
#include "XLVkDevice.h"
#include "XLVkSync.h"
#include "XLVkObject.h"
#include "XLCoreImageStorage.h"
#include "XLCoreFrameQueue.h"
#include "XLVkPipeline.h"
#include "XLVkRenderPass.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

DeviceQueue::~DeviceQueue() { }

bool DeviceQueue::init(Device &device, VkQueue queue, uint32_t index, core::QueueFlags flags) {
	if (!core::DeviceQueue::init(device, index, flags)) {
		return false;
	}
	_queue = queue;
	return true;
}

Status DeviceQueue::waitIdle() {
	VkResult result;
	static_cast<Device *>(_device)->makeApiCall([&, this] (const DeviceTable &table, VkDevice device) {
		result = table.vkQueueWaitIdle(_queue);
	});
	return getStatus(result);
}

Status DeviceQueue::doSubmit(const FrameSync *sync, core::CommandPool *commandPool, core::Fence &fence, SpanView<const core::CommandBuffer *> buffers,
		core::DeviceIdleFlags idle) {
	Vector<VkSemaphore> waitSem;
	Vector<VkPipelineStageFlags> waitStages;
	Vector<VkSemaphore> signalSem;
	Vector<VkCommandBuffer> vkBuffers; vkBuffers.reserve(buffers.size());

	for (auto &it : buffers) {
		if (it) {
			vkBuffers.emplace_back(static_cast<const CommandBuffer *>(it)->getBuffer());
		}
	}

	if (sync) {
		for (auto &it : sync->waitAttachments) {
			if (it.semaphore) {
				auto sem = ((Semaphore *)it.semaphore.get())->getSemaphore();

				if (!it.semaphore->isWaited()) {
					waitSem.emplace_back(sem);
					waitStages.emplace_back(VkPipelineStageFlags(it.stages));
				}
				if (commandPool) {
					commandPool->autorelease(it.semaphore.get());
				}
			}
		}

		for (auto &it : sync->signalAttachments) {
			if (it.semaphore) {
				auto sem = ((Semaphore *)it.semaphore.get())->getSemaphore();

				signalSem.emplace_back(sem);
				if (commandPool) {
					commandPool->autorelease(it.semaphore.get());
				}
			}
		}
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = uint32_t(waitSem.size());
	submitInfo.pWaitSemaphores = waitSem.data();
	submitInfo.pWaitDstStageMask = waitStages.data();
	submitInfo.commandBufferCount = uint32_t(vkBuffers.size());
	submitInfo.pCommandBuffers = vkBuffers.data();
	submitInfo.signalSemaphoreCount = uint32_t(signalSem.size());
	submitInfo.pSignalSemaphores = signalSem.data();

#if XL_VKAPI_DEBUG
	auto t = sp::platform::clock(ClockType::Monotonic);
	fence.addRelease([frameIdx = _frameIdx, t] (bool success) {
		XL_VKAPI_LOG("[", frameIdx,  "] vkQueueSubmit [complete]",
				" [", sp::platform::clock(ClockType::Monotonic) - t, "]");
	}, nullptr, "DeviceQueue::submit");
#endif

	VkResult result;
	auto dev = static_cast<Device *>(_device);

	dev->makeApiCall([&, this] (const DeviceTable &table, VkDevice device) {
		if (hasFlag(idle, core::DeviceIdleFlags::PreDevice)) {
			table.vkDeviceWaitIdle(dev->getDevice());
		} else if (hasFlag(idle, core::DeviceIdleFlags::PreQueue)) {
			table.vkQueueWaitIdle(_queue);
		}
#if XL_VKAPI_DEBUG
		auto t = sp::platform::clock(ClockType::Monotonic);
		result = table.vkQueueSubmit(_queue, 1, &submitInfo, fence.getFence());
		XL_VKAPI_LOG("[", _frameIdx,  "] vkQueueSubmit: ", _result, " ", (void *)_queue,
				" [", sp::platform::clock(ClockType::Monotonic) - t, "]");
#else
		result = table.vkQueueSubmit(_queue, 1, &submitInfo, static_cast<Fence &>(fence).getFence());
#endif

		if (hasFlag(idle, core::DeviceIdleFlags::PostDevice)) {
			table.vkDeviceWaitIdle(dev->getDevice());
		} else if (hasFlag(idle, core::DeviceIdleFlags::PostQueue)) {
			table.vkQueueWaitIdle(_queue);
		}
	});

	if (result == VK_SUCCESS) {
		// mark semaphores
		if (sync) {
			for (auto &it : sync->waitAttachments) {
				if (it.semaphore) {
					it.semaphore->setWaited(true);
					if (it.image && !it.image->isSemaphorePersistent()) {
						fence.addRelease([img = it.image, sem = it.semaphore.get(), t = it.semaphore->getTimeline()] (bool success) {
							sem->setInUse(false, t);
							img->releaseSemaphore(sem);
						}, it.image, "DeviceQueue::submit::!isSemaphorePersistent");
					} else {
						fence.addRelease([sem = it.semaphore.get(), t = it.semaphore->getTimeline()] (bool success) {
							sem->setInUse(false, t);
						}, it.semaphore, "DeviceQueue::submit::isSemaphorePersistent");
					}
					fence.autorelease(it.semaphore.get());
					if (commandPool) {
						commandPool->autorelease(it.semaphore.get());
					}
				}
			}

			for (auto &it : sync->signalAttachments) {
				if (it.semaphore) {
					it.semaphore->setSignaled(true);
					it.semaphore->setInUse(true, it.semaphore->getTimeline());
					fence.autorelease(it.semaphore.get());
					if (commandPool) {
						commandPool->autorelease(it.semaphore.get());
					}
				}
			}
		}

		fence.setArmed(*this);

		if (sync) {
			for (auto &it : sync->images) {
				it.image->setLayout(it.newLayout);
			}
		}
	}
	return getStatus(result);
}

ImageMemoryBarrier::ImageMemoryBarrier(Image *image, VkAccessFlags src, VkAccessFlags dst,
		VkImageLayout old, VkImageLayout _new)
: srcAccessMask(src), dstAccessMask(dst), oldLayout(old), newLayout(_new)
, image(image), subresourceRange(VkImageSubresourceRange{
	image->getAspectMask(), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS
}) { }

ImageMemoryBarrier::ImageMemoryBarrier(Image *image, VkAccessFlags src, VkAccessFlags dst,
		VkImageLayout old, VkImageLayout _new, VkImageSubresourceRange range)
: srcAccessMask(src), dstAccessMask(dst), oldLayout(old), newLayout(_new)
, image(image), subresourceRange(range) { }

ImageMemoryBarrier::ImageMemoryBarrier(Image *image, VkAccessFlags src, VkAccessFlags dst,
		VkImageLayout old, VkImageLayout _new, QueueFamilyTransfer transfer)
: srcAccessMask(src), dstAccessMask(dst), oldLayout(old), newLayout(_new), familyTransfer(transfer)
, image(image), subresourceRange(VkImageSubresourceRange{
	image->getAspectMask(), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS
}) { }

ImageMemoryBarrier::ImageMemoryBarrier(Image *image, VkAccessFlags src, VkAccessFlags dst,
	 VkImageLayout old, VkImageLayout _new, QueueFamilyTransfer transfer, VkImageSubresourceRange range)
: srcAccessMask(src), dstAccessMask(dst), oldLayout(old), newLayout(_new), familyTransfer(transfer)
, image(image), subresourceRange(range) { }

ImageMemoryBarrier::ImageMemoryBarrier(const VkImageMemoryBarrier &barrier)
: srcAccessMask(barrier.srcAccessMask), dstAccessMask(barrier.dstAccessMask)
, oldLayout(barrier.oldLayout), newLayout(barrier.newLayout)
, familyTransfer(QueueFamilyTransfer{barrier.srcQueueFamilyIndex, barrier.dstQueueFamilyIndex})
, image(nullptr), subresourceRange(barrier.subresourceRange) { }

BufferMemoryBarrier::BufferMemoryBarrier(Buffer *buf, VkAccessFlags src, VkAccessFlags dst)
: srcAccessMask(src), dstAccessMask(dst), buffer(buf) { }

BufferMemoryBarrier::BufferMemoryBarrier(Buffer *buf, VkAccessFlags src, VkAccessFlags dst,
		QueueFamilyTransfer transfer)
: srcAccessMask(src), dstAccessMask(dst), familyTransfer(transfer), buffer(buf) { }

BufferMemoryBarrier::BufferMemoryBarrier(Buffer *buf, VkAccessFlags src, VkAccessFlags dst,
		QueueFamilyTransfer transfer, VkDeviceSize offset, VkDeviceSize size)
: srcAccessMask(src), dstAccessMask(dst), familyTransfer(transfer),
  buffer(buf), offset(offset), size(size) { }

BufferMemoryBarrier::BufferMemoryBarrier(const VkBufferMemoryBarrier &barrier)
: srcAccessMask(barrier.srcAccessMask), dstAccessMask(barrier.dstAccessMask)
, familyTransfer(QueueFamilyTransfer{barrier.srcQueueFamilyIndex, barrier.dstQueueFamilyIndex})
, buffer(nullptr), offset(barrier.offset), size(barrier.size) { }


DescriptorImageInfo::~DescriptorImageInfo() { }

DescriptorImageInfo::DescriptorImageInfo(const PipelineDescriptor *desc, uint32_t index, bool external)
: DescriptorInfo(desc, index, external) { }

DescriptorBufferInfo::~DescriptorBufferInfo() { }

DescriptorBufferInfo::DescriptorBufferInfo(const PipelineDescriptor *desc, uint32_t index, bool external)
: DescriptorInfo(desc, index, external) { }

DescriptorBufferViewInfo::~DescriptorBufferViewInfo() { }

DescriptorBufferViewInfo::DescriptorBufferViewInfo(const PipelineDescriptor *desc, uint32_t index, bool external)
: DescriptorInfo(desc, index, external) { }

CommandBuffer::~CommandBuffer() {
	invalidate();
}

bool CommandBuffer::init(const CommandPool *pool, const DeviceTable *table, VkCommandBuffer buffer, Vector<Rc<DescriptorPool>> &&descriptors) {
	_pool = pool;
	_table = table;
	_buffer = buffer;
	_availableDescriptors = sp::move(descriptors);
	return true;
}

void CommandBuffer::invalidate() {
	_buffer = VK_NULL_HANDLE;
	_availableDescriptors.clear();
	_usedDescriptors.clear();
}

void CommandBuffer::cmdPipelineBarrier(VkPipelineStageFlags srcFlags, VkPipelineStageFlags dstFlags, VkDependencyFlags deps,
		SpanView<ImageMemoryBarrier> imageBarriers) {
	Vector<VkImageMemoryBarrier> images; images.reserve(imageBarriers.size());
	for (auto &it : imageBarriers) {
		images.emplace_back(VkImageMemoryBarrier{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
			it.srcAccessMask, it.dstAccessMask,
			it.oldLayout, it.newLayout,
			it.familyTransfer.srcQueueFamilyIndex, it.familyTransfer.dstQueueFamilyIndex,
			it.image->getImage(), it.subresourceRange
		});
		bindImage(it.image);
	}

	_table->vkCmdPipelineBarrier(_buffer, srcFlags, dstFlags, deps, 0, nullptr, 0, nullptr, uint32_t(images.size()), images.data());
}

void CommandBuffer::cmdPipelineBarrier(VkPipelineStageFlags srcFlags, VkPipelineStageFlags dstFlags, VkDependencyFlags deps,
		SpanView<BufferMemoryBarrier> bufferBarriers) {
	Vector<VkBufferMemoryBarrier> buffers; buffers.reserve(bufferBarriers.size());
	for (auto &it : bufferBarriers) {
		buffers.emplace_back(VkBufferMemoryBarrier{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
			it.srcAccessMask, it.dstAccessMask,
			it.familyTransfer.srcQueueFamilyIndex, it.familyTransfer.dstQueueFamilyIndex,
			it.buffer->getBuffer(), it.offset, it.size
		});
		bindBuffer(it.buffer);
	}

	_table->vkCmdPipelineBarrier(_buffer, srcFlags, dstFlags, deps, 0, nullptr, uint32_t(buffers.size()), buffers.data(), 0, nullptr);
}

void CommandBuffer::cmdGlobalBarrier(VkPipelineStageFlags srcFlags, VkPipelineStageFlags dstFlags, VkDependencyFlags deps,
		VkAccessFlags src, VkAccessFlags dst) {
	VkMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.pNext = nullptr;
	barrier.srcAccessMask = src;
	barrier.dstAccessMask = dst;

	_table->vkCmdPipelineBarrier(_buffer, srcFlags, dstFlags, deps, 1, &barrier, 0, nullptr, 0, nullptr);
}

void CommandBuffer::cmdPipelineBarrier(VkPipelineStageFlags srcFlags, VkPipelineStageFlags dstFlags, VkDependencyFlags deps,
		SpanView<BufferMemoryBarrier> bufferBarriers, SpanView<ImageMemoryBarrier> imageBarriers) {
	Vector<VkBufferMemoryBarrier> buffers; buffers.reserve(bufferBarriers.size());
	for (auto &it : bufferBarriers) {
		buffers.emplace_back(VkBufferMemoryBarrier{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
			it.srcAccessMask, it.dstAccessMask,
			it.familyTransfer.srcQueueFamilyIndex, it.familyTransfer.dstQueueFamilyIndex,
			it.buffer->getBuffer(), it.offset, it.size
		});
		bindBuffer(it.buffer);
	}

	Vector<VkImageMemoryBarrier> images; images.reserve(imageBarriers.size());
	for (auto &it : imageBarriers) {
		images.emplace_back(VkImageMemoryBarrier{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
			it.srcAccessMask, it.dstAccessMask,
			it.oldLayout, it.newLayout,
			it.familyTransfer.srcQueueFamilyIndex, it.familyTransfer.dstQueueFamilyIndex,
			it.image->getImage(), it.subresourceRange
		});
		bindImage(it.image);
	}

	_table->vkCmdPipelineBarrier(_buffer, srcFlags, dstFlags, deps, 0, nullptr,
			uint32_t(buffers.size()), buffers.data(), uint32_t(images.size()), images.data());
}

void CommandBuffer::cmdCopyBuffer(Buffer *src, Buffer *dst) {
	VkBufferCopy copy{0, 0, std::min(src->getSize(), dst->getSize())};
	cmdCopyBuffer(src, dst, makeSpanView(&copy, 1));
}

void CommandBuffer::cmdCopyBuffer(Buffer *src, Buffer *dst, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size) {
	VkBufferCopy copy{srcOffset, dstOffset, size};
	cmdCopyBuffer(src, dst, makeSpanView(&copy, 1));
}

void CommandBuffer::cmdCopyBuffer(Buffer *src, Buffer *dst, SpanView<VkBufferCopy> copy) {
	bindBuffer(src);
	bindBuffer(dst);

	_table->vkCmdCopyBuffer(_buffer, src->getBuffer(), dst->getBuffer(), uint32_t(copy.size()), copy.data());
}

void CommandBuffer::cmdCopyImage(Image *src, VkImageLayout srcLayout, Image *dst, VkImageLayout dstLayout, VkFilter filter) {
	auto sourceExtent = src->getInfo().extent;
	auto targetExtent = dst->getInfo().extent;

	if (sourceExtent == targetExtent) {
		VkImageCopy copy{
			VkImageSubresourceLayers{src->getAspectMask(), 0, 0, src->getInfo().arrayLayers.get()},
			VkOffset3D{0, 0, 0},
			VkImageSubresourceLayers{dst->getAspectMask(), 0, 0, dst->getInfo().arrayLayers.get()},
			VkOffset3D{0, 0, 0},
			VkExtent3D{targetExtent.width, targetExtent.height, targetExtent.depth}
		};

		_table->vkCmdCopyImage(_buffer, src->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				dst->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	} else {
		VkImageBlit blit{
			VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, src->getInfo().arrayLayers.get()},
			{ VkOffset3D{0, 0, 0}, VkOffset3D{int32_t(sourceExtent.width), int32_t(sourceExtent.height), int32_t(sourceExtent.depth)} },
			VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, dst->getInfo().arrayLayers.get()},
			{ VkOffset3D{0, 0, 0}, VkOffset3D{int32_t(targetExtent.width), int32_t(targetExtent.height), int32_t(targetExtent.depth)} },
		};

		_table->vkCmdBlitImage(_buffer, src->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				dst->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, filter);
	}
}

void CommandBuffer::cmdCopyImage(Image *src, VkImageLayout srcLayout, Image *dst, VkImageLayout dstLayout, const VkImageCopy &copy) {
	bindImage(src);
	bindImage(dst);

	_table->vkCmdCopyImage(_buffer, src->getImage(), srcLayout, dst->getImage(), dstLayout, 1, &copy);
}

void CommandBuffer::cmdCopyImage(Image *src, VkImageLayout srcLayout, Image *dst, VkImageLayout dstLayout, SpanView<VkImageCopy> copy) {
	bindImage(src);
	bindImage(dst);

	_table->vkCmdCopyImage(_buffer, src->getImage(), srcLayout, dst->getImage(), dstLayout, uint32_t(copy.size()), copy.data());
}

void CommandBuffer::cmdCopyBufferToImage(Buffer *buf, Image *img, VkImageLayout layout, VkDeviceSize offset) {
	auto &extent = img->getInfo().extent;
	VkImageSubresourceLayers copyLayers({ img->getAspectMask(), 0, 0, img->getInfo().arrayLayers.get() });

	VkBufferImageCopy copyRegion{offset, 0, 0, copyLayers, VkOffset3D{0, 0, 0},
		VkExtent3D{extent.width, extent.height, extent.depth}};

	cmdCopyBufferToImage(buf, img, layout, makeSpanView(&copyRegion, 1));
}

void CommandBuffer::cmdCopyBufferToImage(Buffer *buf, Image *img, VkImageLayout layout, SpanView<VkBufferImageCopy> copy) {
	bindBuffer(buf);
	bindImage(img);

	_table->vkCmdCopyBufferToImage(_buffer, buf->getBuffer(), img->getImage(), layout, uint32_t(copy.size()), copy.data());
}

void CommandBuffer::cmdCopyImageToBuffer(Image *img, VkImageLayout layout, Buffer *buf, VkDeviceSize offset) {
	auto &extent = img->getInfo().extent;
	VkImageSubresourceLayers copyLayers({ img->getAspectMask(), 0, 0, img->getInfo().arrayLayers.get() });

	VkBufferImageCopy copyRegion{offset, 0, 0, copyLayers, VkOffset3D{0, 0, 0},
		VkExtent3D{extent.width, extent.height, extent.depth}};

	cmdCopyImageToBuffer(img, layout, buf, makeSpanView(&copyRegion, 1));
}

void CommandBuffer::cmdCopyImageToBuffer(Image *img, VkImageLayout layout, Buffer *buf, SpanView<VkBufferImageCopy> copy) {
	bindBuffer(buf);
	bindImage(img);

	_table->vkCmdCopyImageToBuffer(_buffer, img->getImage(), layout, buf->getBuffer(), uint32_t(copy.size()), copy.data());
}

void CommandBuffer::cmdClearColorImage(Image *image, VkImageLayout layout, const Color4F &color) {
	VkClearColorValue clearColorEmpty;
	clearColorEmpty.float32[0] = color.r;
	clearColorEmpty.float32[1] = color.g;
	clearColorEmpty.float32[2] = color.b;
	clearColorEmpty.float32[3] = color.a;

	VkImageSubresourceRange range{ image->getAspectMask(), 0, image->getInfo().mipLevels.get(), 0, image->getInfo().arrayLayers.get() };

	bindImage(image);
	_table->vkCmdClearColorImage(_buffer, image->getImage(), layout,
			&clearColorEmpty, 1, &range);
}

void CommandBuffer::cmdBeginRenderPass(RenderPass *pass, Framebuffer *fb, VkSubpassContents subpass, bool alt) {
	auto &clearValues = pass->getClearValues();
	auto currentExtent = fb->getExtent();

	VkRenderPassBeginInfo renderPassInfo {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr
	};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.pNext = nullptr;
	renderPassInfo.renderPass = pass->getRenderPass(alt);
	renderPassInfo.framebuffer = fb->getFramebuffer();
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = VkExtent2D{currentExtent.width, currentExtent.height};
	renderPassInfo.clearValueCount = uint32_t(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	bindFramebuffer(fb);
	_table->vkCmdBeginRenderPass(_buffer, &renderPassInfo, subpass);

	_currentSubpass = 0;
	_withinRenderpass = true;
}

void CommandBuffer::cmdEndRenderPass() {
	_table->vkCmdEndRenderPass(_buffer);

	_withinRenderpass = false;
	_currentSubpass = 0;
}

void CommandBuffer::cmdSetViewport(uint32_t firstViewport, SpanView<VkViewport> viewports) {
	_table->vkCmdSetViewport(_buffer, firstViewport, uint32_t(viewports.size()), viewports.data());
}

void CommandBuffer::cmdSetScissor(uint32_t firstScissor, SpanView<VkRect2D> scissors) {
	//log::verbose("CommandBuffer", "cmdSetScissor: ", scissors.front().offset.x, " ", scissors.front().offset.y, " ",
	//		scissors.front().extent.width, " ", scissors.front().extent.height);
	_table->vkCmdSetScissor(_buffer, firstScissor, uint32_t(scissors.size()), scissors.data());
}

void CommandBuffer::cmdBindPipeline(GraphicPipeline *pipeline) {
	if (pipeline != _boundGraphicPipeline) {
		_table->vkCmdBindPipeline(_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());
		_boundGraphicPipeline = pipeline;
	}
}

void CommandBuffer::cmdBindPipeline(ComputePipeline *pipeline) {
	if (pipeline != _boundComputePipeline) {
		_table->vkCmdBindPipeline(_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->getPipeline());
		_boundComputePipeline = pipeline;
	}
}

void CommandBuffer::cmdBindPipelineWithDescriptors(const core::GraphicPipelineData *data, uint32_t firstSet) {
	cmdBindDescriptorSets(static_cast<RenderPass *>(data->subpass->pass->impl.get()), _availableDescriptors[data->layout->index], firstSet);
	cmdBindPipeline(static_cast<GraphicPipeline *>(data->pipeline.get()));
}

void CommandBuffer::cmdBindPipelineWithDescriptors(const core::ComputePipelineData *data, uint32_t firstSet) {
	cmdBindDescriptorSets(static_cast<RenderPass *>(data->subpass->pass->impl.get()), _availableDescriptors[data->layout->index], firstSet);
	cmdBindPipeline(static_cast<ComputePipeline *>(data->pipeline.get()));
}

void CommandBuffer::cmdBindIndexBuffer(Buffer *buf, VkDeviceSize offset, VkIndexType indexType) {
	_table->vkCmdBindIndexBuffer(_buffer, buf->getBuffer(), offset, indexType);
}

void CommandBuffer::cmdBindDescriptorSets(RenderPass *pass, uint32_t index, uint32_t firstSet) {
	cmdBindDescriptorSets(pass, _availableDescriptors[index], firstSet);
}

void CommandBuffer::cmdBindDescriptorSets(RenderPass *pass, const Rc<DescriptorPool> &pool, uint32_t firstSet) {
	auto bindPoint = (pass->getType() == core::PassType::Compute) ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
	auto sets = pool->getSets();

	auto targetLayout = pool->getLayout();

	Vector<VkDescriptorSet> bindSets; bindSets.reserve(sets.size());
	for (auto &it : sets) {
		bindSets.emplace_back(it->set);
	}

	auto doBindSets = [&] {
		for (auto &it : sets) {
			_descriptorSets.emplace(it);
		}

		_boundLayout = targetLayout;

		_table->vkCmdBindDescriptorSets(_buffer, bindPoint, _boundLayout->getLayout(), firstSet,
				uint32_t(bindSets.size()), bindSets.data(), 0, nullptr);

		auto it = std::find(_availableDescriptors.begin(), _availableDescriptors.end(), pool);
		if (it == _availableDescriptors.end()) {
			_usedDescriptors.emplace(pool);
		}
	};

	if (targetLayout != _boundLayout) {
		updateBoundSets(bindSets, firstSet);
		doBindSets();
	} else if (!updateBoundSets(bindSets, firstSet)) {
		doBindSets();
	}
}

void CommandBuffer::cmdBindDescriptorSets(RenderPass *pass, SpanView<VkDescriptorSet> sets, uint32_t firstSet) {
	auto bindPoint = (pass->getType() == core::PassType::Compute) ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

	if (!_boundLayout) {
		log::error("vk::CommandBuffer", "Try to rebind sets when no layout is bound");
	}

	if (!updateBoundSets(sets, firstSet)) {
		_table->vkCmdBindDescriptorSets(_buffer, bindPoint, _boundLayout->getLayout(), firstSet,
				uint32_t(sets.size()), sets.data(), 0, nullptr);
	}
}

void CommandBuffer::cmdBindGraphicDescriptorSets(VkPipelineLayout layout, SpanView<VkDescriptorSet> sets, uint32_t firstSet) {
	_table->vkCmdBindDescriptorSets(_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, firstSet,
			uint32_t(sets.size()), sets.data(), 0, nullptr);
}

void CommandBuffer::cmdBindComputeDescriptorSets(VkPipelineLayout layout, SpanView<VkDescriptorSet> sets, uint32_t firstSet) {
	_table->vkCmdBindDescriptorSets(_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, layout, firstSet,
			uint32_t(sets.size()), sets.data(), 0, nullptr);
}

void CommandBuffer::cmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
	_table->vkCmdDraw(_buffer, vertexCount, instanceCount, firstVertex, firstInstance);
}
void CommandBuffer::cmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
		int32_t vertexOffset, uint32_t firstInstance) {
	_table->vkCmdDrawIndexed(_buffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBuffer::cmdPushConstants(PipelineLayout *layout, VkShaderStageFlags stageFlags, uint32_t offset, BytesView data) {
	_table->vkCmdPushConstants(_buffer, layout->getLayout(), stageFlags, offset, uint32_t(data.size()), data.data());
}

void CommandBuffer::cmdPushConstants(VkShaderStageFlags stageFlags, uint32_t offset, BytesView data) {
	XLASSERT(_boundLayout, "cmdPushConstants without bound layout");
	_table->vkCmdPushConstants(_buffer, _boundLayout->getLayout(), stageFlags, offset, uint32_t(data.size()), data.data());
}

void CommandBuffer::cmdFillBuffer(Buffer *buffer, uint32_t data) {
	cmdFillBuffer(buffer, 0, VK_WHOLE_SIZE, data);
}

void CommandBuffer::cmdFillBuffer(Buffer *buffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) {
	bindBuffer(buffer);
	_table->vkCmdFillBuffer(_buffer, buffer->getBuffer(), dstOffset, size, data);
}

void CommandBuffer::cmdDispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
	_table->vkCmdDispatch(_buffer, groupCountX, groupCountY, groupCountZ);
}

void CommandBuffer::cmdDispatchPipeline(ComputePipeline *pipeline, uint32_t countX, uint32_t countY, uint32_t countZ) {
	cmdBindPipeline(pipeline);
	cmdDispatch((countX - 1) / pipeline->getLocalX() + 1,
			(countY - 1) / pipeline->getLocalY() + 1,
			(countZ - 1) / pipeline->getLocalZ() + 1);
}

uint32_t CommandBuffer::cmdNextSubpass() {
	if (_withinRenderpass) {
		_table->vkCmdNextSubpass(_buffer, VK_SUBPASS_CONTENTS_INLINE);
		++ _currentSubpass;
		return _currentSubpass;
	}
	return 0;
}

void CommandBuffer::writeImageTransfer(uint32_t sourceFamily, uint32_t targetFamily, const Rc<Buffer> &buffer, const Rc<Image> &image) {
	ImageMemoryBarrier inImageBarrier(image, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	cmdPipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, makeSpanView(&inImageBarrier, 1));

	auto sourceFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	auto targetFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	if (image->getInfo().type != core::PassType::Generic && sourceFamily != VK_QUEUE_FAMILY_IGNORED && targetFamily != VK_QUEUE_FAMILY_IGNORED) {
		if (sourceFamily != targetFamily) {
			sourceFamilyIndex = sourceFamily;
			targetFamilyIndex = targetFamily;
		}
	}

	cmdCopyBufferToImage(buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0);

	ImageMemoryBarrier outImageBarrier(image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		QueueFamilyTransfer{sourceFamilyIndex, targetFamilyIndex});

	cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, makeSpanView(&outImageBarrier, 1));

	if (targetFamilyIndex != VK_QUEUE_FAMILY_IGNORED) {
		image->setPendingBarrier(outImageBarrier);
	}
}

void CommandBuffer::bindBuffer(core::BufferObject *buffer) {
	core::CommandBuffer::bindBuffer(buffer);
	if (auto pool = static_cast<Buffer *>(buffer)->getMemory()->getPool()) {
		_memPool.emplace(pool);
	}
}

bool CommandBuffer::updateBoundSets(SpanView<VkDescriptorSet> sets, uint32_t firstSet) {
	auto size = sets.size() + firstSet;
	if (size <= _boundSets.size()) {
		if (memcmp(_boundSets.data() + firstSet, sets.data(), sizeof(VkDescriptorSet) * sets.size()) == 0) {
			return true;
		}
	}

	_boundSets.resize(size);
	memcpy(_boundSets.data() + firstSet, sets.data(), sizeof(VkDescriptorSet) * sets.size());
	return false;
}

CommandPool::~CommandPool() {
	if (_commandPool) {
		log::error("VK-Error", "CommandPool was not destroyed");
	}
}

static void CommandPool_destroy(core::Device *dev, core::ObjectType, core::ObjectHandle ptr, void *) {
	auto d = static_cast<Device *>(dev);
	auto target = (VkCommandPool *)ptr.get();
	if (*target) {
		d->getTable()->vkDestroyCommandPool(d->getDevice(), *target, nullptr);
		*target = nullptr;
	}
}

bool CommandPool::init(Device &dev, uint32_t familyIdx, core::QueueFlags c, bool transient) {
	_familyIdx = familyIdx;
	_class = c;

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.queueFamilyIndex = familyIdx;
	poolInfo.flags = 0; // transient ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : 0;

	if (dev.getTable()->vkCreateCommandPool(dev.getDevice(), &poolInfo, nullptr, &_commandPool) == VK_SUCCESS) {
		return core::CommandPool::init(dev, CommandPool_destroy, core::ObjectType::CommandPool, core::ObjectHandle(&_commandPool));
	}

	return false;
}

const CommandBuffer *CommandPool::recordBuffer(Device &dev, Vector<Rc<DescriptorPool>> &&descriptors,
		const Callback<bool(CommandBuffer &)> &cb,
		VkCommandBufferUsageFlagBits flags, Level level) {
	if (!_commandPool) {
		return nullptr;
	}

	if (_invalidated) {
		recreatePool(dev);
	}

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = _commandPool;
	allocInfo.level = VkCommandBufferLevel(level);
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer buf;
	if (dev.getTable()->vkAllocateCommandBuffers(dev.getDevice(), &allocInfo, &buf) != VK_SUCCESS) {
		return nullptr;
	}

	VkCommandBufferBeginInfo beginInfo { };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = flags;
	beginInfo.pInheritanceInfo = nullptr;

	if (dev.getTable()->vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS) {
		dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool, 1, &buf);
		return nullptr;
	}

	auto b = Rc<CommandBuffer>::create(this, dev.getTable(), buf, sp::move(descriptors));
	if (!b) {
		dev.getTable()->vkEndCommandBuffer(buf);
		dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool, 1, &buf);
		return nullptr;
	}

	auto result = cb(*b);

	dev.getTable()->vkEndCommandBuffer(buf);

	if (!result) {
		dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool, 1, &buf);
		return nullptr;
	}

	return static_cast<const CommandBuffer *>(_buffers.emplace_back(move(b)).get());
}

void CommandPool::freeDefaultBuffers(Device &dev, Vector<VkCommandBuffer> &vec) {
	if (_commandPool) {
		dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool, static_cast<uint32_t>(vec.size()), vec.data());
	}
	vec.clear();
}

void CommandPool::reset(Device &dev, bool release) {
	if (_commandPool) {
		//dev.getTable()->vkResetCommandPool(dev.getDevice(), _commandPool, release ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT : 0);

		Vector<VkCommandBuffer> buffersToFree;
		for (auto &it : _buffers) {
			if (it) {
				buffersToFree.emplace_back(static_cast<const CommandBuffer *>(it.get())->getBuffer());
			}
		}
		if (buffersToFree.size() > 0) {
			dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool, static_cast<uint32_t>(buffersToFree.size()), buffersToFree.data());
		}

		if (dev.isPortabilityMode()) {
			_invalidated = true;
		} else {
			recreatePool(dev);
		}
	}

	core::CommandPool::reset(dev, release);
}

void CommandPool::autorelease(Rc<Ref> &&ref) {
	_autorelease.emplace_back(move(ref));
}

void CommandPool::recreatePool(Device &dev) {
	if (_commandPool) {
		dev.getTable()->vkDestroyCommandPool(dev.getDevice(), _commandPool, nullptr);
		_commandPool = VkCommandPool(0);
	}

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.queueFamilyIndex = _familyIdx;
	poolInfo.flags = 0; // transient ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : 0;

	dev.getTable()->vkCreateCommandPool(dev.getDevice(), &poolInfo, nullptr, &_commandPool);

	_invalidated = false;
}

}
