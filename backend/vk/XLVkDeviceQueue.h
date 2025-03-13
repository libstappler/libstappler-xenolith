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

#ifndef XENOLITH_BACKEND_VK_XLVKDEVICEQUEUE_H_
#define XENOLITH_BACKEND_VK_XLVKDEVICEQUEUE_H_

#include "XLVkInstance.h"
#include "XLVkLoop.h"
#include "XLCoreDevice.h"
#include "XLCoreLoop.h"
#include "XLCoreDeviceQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class Device;
class DeviceQueue;
class CommandPool;
class Semaphore;
class Fence;
class Loop;
class Image;
class ImageView;
class Buffer;
class RenderPass;
class Framebuffer;
class GraphicPipeline;
class ComputePipeline;
class CommandBuffer;
class DeviceMemoryPool;
class PipelineLayout;
class DescriptorPool;
struct DescriptorSetBindings;

using PipelineDescriptor = core::PipelineDescriptor;

class SP_PUBLIC DeviceQueue final : public core::DeviceQueue {
public:
	using FrameSync = core::FrameSync;
	using FrameHandle = core::FrameHandle;

	virtual ~DeviceQueue();

	bool init(Device &device, VkQueue queue, uint32_t index, core::QueueFlags flags);

	virtual Status waitIdle() override;

	VkQueue getQueue() const { return _queue; }

protected:
	virtual Status doSubmit(const FrameSync *, core::CommandPool *, core::Fence &, SpanView<const core::CommandBuffer *>,
			core::DeviceIdleFlags = core::DeviceIdleFlags::None) override;

	VkQueue _queue;
};


enum class BufferLevel {
	Primary = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	Secondary = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
};

struct SP_PUBLIC QueueFamilyTransfer {
	uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
};

struct SP_PUBLIC ImageMemoryBarrier {
	ImageMemoryBarrier() = default;

	ImageMemoryBarrier(Image *, VkAccessFlags src, VkAccessFlags dst,
			VkImageLayout old, VkImageLayout _new);
	ImageMemoryBarrier(Image *, VkAccessFlags src, VkAccessFlags dst,
			VkImageLayout old, VkImageLayout _new, VkImageSubresourceRange);
	ImageMemoryBarrier(Image *, VkAccessFlags src, VkAccessFlags dst,
			VkImageLayout old, VkImageLayout _new, QueueFamilyTransfer);
	ImageMemoryBarrier(Image *, VkAccessFlags src, VkAccessFlags dst,
			VkImageLayout old, VkImageLayout _new, QueueFamilyTransfer, VkImageSubresourceRange);
	ImageMemoryBarrier(const VkImageMemoryBarrier &);

	explicit operator bool() const {
		return srcAccessMask != 0 || dstAccessMask != 0
			|| oldLayout != VK_IMAGE_LAYOUT_UNDEFINED || newLayout != VK_IMAGE_LAYOUT_UNDEFINED
			|| image != nullptr;
	}

	VkAccessFlags srcAccessMask = 0;
	VkAccessFlags dstAccessMask = 0;
	VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	QueueFamilyTransfer familyTransfer;
	Image *image = nullptr;
	VkImageSubresourceRange subresourceRange;
};

struct SP_PUBLIC BufferMemoryBarrier {
	BufferMemoryBarrier() = default;
	BufferMemoryBarrier(Buffer *, VkAccessFlags src, VkAccessFlags dst);
	BufferMemoryBarrier(Buffer *, VkAccessFlags src, VkAccessFlags dst,
			QueueFamilyTransfer);
	BufferMemoryBarrier(Buffer *, VkAccessFlags src, VkAccessFlags dst,
			QueueFamilyTransfer, VkDeviceSize, VkDeviceSize);
	BufferMemoryBarrier(const VkBufferMemoryBarrier &);

	explicit operator bool() const {
		return srcAccessMask != 0 || dstAccessMask != 0 || buffer != nullptr;
	}

	VkAccessFlags srcAccessMask = 0;
	VkAccessFlags dstAccessMask = 0;
	QueueFamilyTransfer familyTransfer;
	Buffer *buffer = nullptr;
	VkDeviceSize offset = 0;
	VkDeviceSize size = VK_WHOLE_SIZE;
};

struct SP_PUBLIC DescriptorInfo {
	DescriptorInfo(const PipelineDescriptor *desc, uint32_t index, bool external)
	: descriptor(desc), index(index), external(external) { }

	const PipelineDescriptor *descriptor;
	uint32_t index;
	bool external;
};

struct SP_PUBLIC DescriptorImageInfo : public DescriptorInfo {
	~DescriptorImageInfo();
	DescriptorImageInfo(const PipelineDescriptor *desc, uint32_t index, bool external);

	Rc<ImageView> imageView;
	VkSampler sampler = VK_NULL_HANDLE;
	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct SP_PUBLIC DescriptorBufferInfo : public DescriptorInfo {
	~DescriptorBufferInfo();
	DescriptorBufferInfo(const PipelineDescriptor *desc, uint32_t index, bool external);

	Rc<Buffer> buffer;
	VkDeviceSize offset = 0;
	VkDeviceSize range = VK_WHOLE_SIZE;
};

struct SP_PUBLIC DescriptorBufferViewInfo : public DescriptorInfo {
	~DescriptorBufferViewInfo();
	DescriptorBufferViewInfo(const PipelineDescriptor *desc, uint32_t index, bool external);

	Rc<Buffer> buffer;
	VkBufferView target = VK_NULL_HANDLE;
};

class SP_PUBLIC CommandBuffer : public core::CommandBuffer {
public:
	virtual ~CommandBuffer();

	bool init(const CommandPool *, const DeviceTable *, VkCommandBuffer, Vector<Rc<DescriptorPool>> &&descriptors);
	void invalidate();

	void cmdPipelineBarrier(VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags,
			SpanView<ImageMemoryBarrier>);
	void cmdPipelineBarrier(VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags,
			SpanView<BufferMemoryBarrier>);
	void cmdPipelineBarrier(VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags,
			SpanView<BufferMemoryBarrier>, SpanView<ImageMemoryBarrier>);
	void cmdGlobalBarrier(VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags,
			VkAccessFlags src, VkAccessFlags dst);

	void cmdCopyBuffer(Buffer *src, Buffer *dst);
	void cmdCopyBuffer(Buffer *src, Buffer *dst, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size);
	void cmdCopyBuffer(Buffer *src, Buffer *dst, SpanView<VkBufferCopy>);

	void cmdCopyImage(Image *src, VkImageLayout, Image *dst, VkImageLayout, VkFilter filter = VK_FILTER_LINEAR);
	void cmdCopyImage(Image *src, VkImageLayout, Image *dst, VkImageLayout, const VkImageCopy &copy);
	void cmdCopyImage(Image *src, VkImageLayout, Image *dst, VkImageLayout, SpanView<VkImageCopy>);

	void cmdCopyBufferToImage(Buffer *, Image *, VkImageLayout, VkDeviceSize offset);
	void cmdCopyBufferToImage(Buffer *, Image *, VkImageLayout, SpanView<VkBufferImageCopy>);

	void cmdCopyImageToBuffer(Image *, VkImageLayout, Buffer *buf, VkDeviceSize offset);
	void cmdCopyImageToBuffer(Image *, VkImageLayout, Buffer *buf, SpanView<VkBufferImageCopy>);

	void cmdClearColorImage(Image *, VkImageLayout, const Color4F &);

	void cmdBeginRenderPass(RenderPass *pass, Framebuffer *fb, VkSubpassContents subpass, bool alt = false);
	void cmdEndRenderPass();

	void cmdSetViewport(uint32_t firstViewport, SpanView<VkViewport> viewports);
	void cmdSetScissor(uint32_t firstScissor, SpanView<VkRect2D> scissors);

	void cmdBindPipeline(GraphicPipeline *);
	void cmdBindPipeline(ComputePipeline *);

	void cmdBindPipelineWithDescriptors(const core::GraphicPipelineData *, uint32_t firstSet = 0);
	void cmdBindPipelineWithDescriptors(const core::ComputePipelineData *, uint32_t firstSet = 0);

	void cmdBindIndexBuffer(Buffer *, VkDeviceSize offset, VkIndexType indexType);

	void cmdBindDescriptorSets(RenderPass *, uint32_t index, uint32_t firstSet = 0);
	void cmdBindDescriptorSets(RenderPass *, const Rc<DescriptorPool> &, uint32_t firstSet = 0);
	void cmdBindDescriptorSets(RenderPass *, SpanView<VkDescriptorSet>, uint32_t firstSet = 0);

	void cmdBindGraphicDescriptorSets(VkPipelineLayout, SpanView<VkDescriptorSet>, uint32_t firstSet = 0);
	void cmdBindComputeDescriptorSets(VkPipelineLayout, SpanView<VkDescriptorSet>, uint32_t firstSet = 0);

	void cmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
	void cmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
			int32_t vertexOffset, uint32_t firstInstance);

	void cmdPushConstants(PipelineLayout *layout, VkShaderStageFlags stageFlags, uint32_t offset, BytesView);
	void cmdPushConstants(VkShaderStageFlags stageFlags, uint32_t offset, BytesView);

	void cmdFillBuffer(Buffer *, uint32_t data);
	void cmdFillBuffer(Buffer *, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data);

	// count in group blocks
	void cmdDispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1);

	// count in individual elements, not in blocks
	void cmdDispatchPipeline(ComputePipeline *, uint32_t countX, uint32_t countY = 1, uint32_t countZ = 1);

	uint32_t cmdNextSubpass();

	VkCommandBuffer getBuffer() const { return _buffer; }

	uint32_t getCurrentSubpass() const { return _currentSubpass; }
	uint32_t getBoundLayoutIndex() const { return _boundLayoutIndex; }
	PipelineLayout *getBoundLayout() const { return _boundLayout; }

	void writeImageTransfer(uint32_t sourceFamily, uint32_t targetFamily, const Rc<Buffer> &, const Rc<Image> &);

	virtual void bindBuffer(core::BufferObject *) override;

protected:
	bool updateBoundSets(SpanView<VkDescriptorSet>, uint32_t firstSet);

	Vector<Rc<DescriptorPool>> _availableDescriptors;
	Set<Rc<DescriptorPool>> _usedDescriptors;

	Rc<PipelineLayout> _boundLayout;

	const CommandPool *_pool = nullptr;
	const DeviceTable *_table = nullptr;
	VkCommandBuffer _buffer = VK_NULL_HANDLE;

	Set<Rc<DescriptorSetBindings>> _descriptorSets;
	Set<Rc<DeviceMemoryPool>> _memPool;
	Vector<VkDescriptorSet> _boundSets;

	const GraphicPipeline *_boundGraphicPipeline = nullptr;
	const ComputePipeline *_boundComputePipeline = nullptr;
};

class SP_PUBLIC CommandPool : public core::CommandPool {
public:
	static constexpr VkCommandBufferUsageFlagBits DefaultFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	using Level = BufferLevel;

	virtual ~CommandPool();

	bool init(Device &dev, uint32_t familyIdx, core::QueueFlags c = core::QueueFlags::Graphics, bool transient = true);

	VkCommandPool getCommandPool() const { return _commandPool; }

	const CommandBuffer * recordBuffer(Device &dev, Vector<Rc<DescriptorPool>> &&descriptors,
			const Callback<bool(CommandBuffer &)> &,
			VkCommandBufferUsageFlagBits = DefaultFlags, Level = Level::Primary);

	void freeDefaultBuffers(Device &dev, Vector<VkCommandBuffer> &);
	void reset(Device &dev, bool release = false);

	void autorelease(Rc<Ref> &&);

protected:
	void recreatePool(Device &dev);

	VkCommandPool _commandPool = VK_NULL_HANDLE;
};

}

#endif /* XENOLITH_BACKEND_VK_XLVKDEVICEQUEUE_H_ */
