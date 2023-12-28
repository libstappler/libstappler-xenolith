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

#ifndef XENOLITH_BACKEND_VK_XLVKDEVICE_H_
#define XENOLITH_BACKEND_VK_XLVKDEVICE_H_

#include "XLVkInstance.h"
#include "XLVkDeviceQueue.h"
#include "XLVkLoop.h"

namespace stappler::xenolith::vk {

class Fence;
class Allocator;
class TextureSetLayout;
class Sampler;
class Loop;
class DeviceMemoryPool;

class DeviceFrameHandle : public core::FrameHandle {
public:
	virtual ~DeviceFrameHandle();

	bool init(Loop &loop, Device &, Rc<FrameRequest> &&, uint64_t gen);

	const Rc<Allocator> &getAllocator() const { return _allocator; }
	const Rc<DeviceMemoryPool> &getMemPool(void *key);

protected:
	Rc<Allocator> _allocator;
	Mutex _mutex;
	Map<void *, Rc<DeviceMemoryPool>>  _memPools;
};

class Device : public core::Device {
public:
	using Features = DeviceInfo::Features;
	using Properties = DeviceInfo::Properties;
	using FrameHandle = core::FrameHandle;

	Device();
	virtual ~Device();

	bool init(const vk::Instance *instance, DeviceInfo &&, const Features &, const Vector<StringView> &);

	const Instance *getInstance() const { return _vkInstance; }
	VkDevice getDevice() const { return _device; }
	VkPhysicalDevice getPhysicalDevice() const;

	void begin(Loop &loop, thread::TaskQueue &, Function<void(bool)> &&);
	virtual void end() override;

	const DeviceInfo & getInfo() const { return _info; }
	const DeviceTable * getTable() const;
	const Rc<Allocator> & getAllocator() const { return _allocator; }

	const DeviceQueueFamily *getQueueFamily(uint32_t) const;
	const DeviceQueueFamily *getQueueFamily(QueueOperations) const;
	const DeviceQueueFamily *getQueueFamily(core::PassType) const;

	const Vector<DeviceQueueFamily> &getQueueFamilies() const;

	// acquire VkQueue handle
	// - QueueOperations - one of QueueOperations flags, defining capabilities of required queue
	// - gl::FrameHandle - frame, in which queue will be used
	// - acquire - function, to call with result, can be called immediately
	// 		or when queue for specified operations become available (on GL thread)
	// - invalidate - function to call when queue query is invalidated (e.g. when frame is invalidated)
	// - ref - ref to preserve until query is completed
	// returns
	// - true is query was completed or scheduled,
	// - false if frame is not valid or no queue family with requested capabilities exists
	//
	// Acquired DeviceQueue must be released with releaseQueue
	bool acquireQueue(QueueOperations, FrameHandle &, Function<void(FrameHandle &, const Rc<DeviceQueue> &)> && acquire,
			Function<void(FrameHandle &)> && invalidate, Rc<Ref> && = nullptr);
	bool acquireQueue(QueueOperations, Loop &, Function<void(Loop &, const Rc<DeviceQueue> &)> && acquire,
			Function<void(Loop &)> && invalidate, Rc<Ref> && = nullptr);
	void releaseQueue(Rc<DeviceQueue> &&);

	// Запросить DeviceQueue синхронно, может блокировать текущий поток до завершения захвата
	// Преднахзначна для потоков, не относящихся к группе графических (например, для потока окна)
	// Вызов в графическом потоке может заблокировать возврат очереди, уже принадлежащей потоку
	Rc<DeviceQueue> tryAcquireQueueSync(QueueOperations, bool lock);

	Rc<CommandPool> acquireCommandPool(QueueOperations, uint32_t = 0);
	Rc<CommandPool> acquireCommandPool(uint32_t familyIndex);
	void releaseCommandPool(core::Loop &, Rc<CommandPool> &&);
	void releaseCommandPoolUnsafe(Rc<CommandPool> &&);

	const Rc<TextureSetLayout> &getTextureSetLayout() const { return _textureSetLayout; }

	BytesView emplaceConstant(core::PredefinedConstant, Bytes &) const;

	virtual bool supportsUpdateAfterBind(DescriptorType) const override;

	virtual Rc<core::ImageObject> getEmptyImageObject() const override;
	virtual Rc<core::ImageObject> getSolidImageObject() const override;

	virtual Rc<core::Framebuffer> makeFramebuffer(const core::QueuePassData *, SpanView<Rc<core::ImageView>>) override;
	virtual Rc<ImageStorage> makeImage(const ImageInfoData &) override;
	virtual Rc<core::Semaphore> makeSemaphore() override;
	virtual Rc<core::ImageView> makeImageView(const Rc<core::ImageObject> &, const ImageViewInfo &) override;

	template <typename Callback>
	void makeApiCall(const Callback &cb) {
		//_apiMutex.lock();
		cb(*getTable(), getDevice());
		//_apiMutex.unlock();
	}

	bool hasNonSolidFillMode() const;
	bool hasDynamicIndexedBuffers() const;

	virtual void waitIdle() const override;

	void compileImage(const Loop &loop, const Rc<core::DynamicImage> &, Function<void(bool)> &&);

private:
	using core::Device::init;

	friend class DeviceQueue;

	virtual void compileSamplers(thread::TaskQueue &q, bool force);

	bool setup(const Instance *instance, VkPhysicalDevice p, const Properties &prop,
			const Vector<DeviceQueueFamily> &queueFamilies, const Features &features, const Vector<StringView> &requiredExtension);

	const vk::Instance *_vkInstance = nullptr;
	const DeviceTable *_table = nullptr;
#if VK_HOOK_DEBUG
	const DeviceTable *_original = nullptr;
#endif
	VkDevice _device = VK_NULL_HANDLE;

	DeviceInfo _info;
	Features _enabledFeatures;

	Rc<Allocator> _allocator;
	Rc<TextureSetLayout> _textureSetLayout;

	Vector<DeviceQueueFamily> _families;

	bool _finished = false;
	bool _updateAfterBindEnabled = true;

	Vector<VkSampler> _immutableSamplers;
	Vector<Rc<Sampler>> _samplers;
	size_t _compiledSamplers = 0;
	std::atomic<bool> _samplersCompiled = false;

	std::unordered_map<VkFormat, VkFormatProperties> _formats;

	Mutex _resourceMutex;
	uint32_t _resourceQueueWaiters = 0;
	std::condition_variable _resourceQueueCond;
	Mutex _apiMutex;
};

}

#endif /* XENOLITH_BACKEND_VK_XLVKDEVICE_H_ */
