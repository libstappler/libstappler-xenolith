/**
Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class Fence;
class Allocator;
class TextureSetLayout;
class Sampler;
class Loop;
class DeviceMemoryPool;

class SP_PUBLIC DeviceFrameHandle : public core::FrameHandle {
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

class SP_PUBLIC Device : public core::Device {
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

	void begin(Loop &loop, Function<void(bool)> &&);
	virtual void end() override;

	const DeviceInfo & getInfo() const { return _info; }
	const DeviceTable * getTable() const;
	const Rc<Allocator> & getAllocator() const { return _allocator; }

	const Rc<TextureSetLayout> &getTextureSetLayout() const { return _textureSetLayout; }

	BytesView emplaceConstant(core::PredefinedConstant, Bytes &) const;

	virtual bool supportsUpdateAfterBind(DescriptorType) const override;

	virtual Rc<core::ImageObject> getEmptyImageObject() const override;
	virtual Rc<core::ImageObject> getSolidImageObject() const override;

	virtual Rc<core::Framebuffer> makeFramebuffer(const core::QueuePassData *, SpanView<Rc<core::ImageView>>) override;
	virtual Rc<ImageStorage> makeImage(const ImageInfoData &) override;
	virtual Rc<core::Semaphore> makeSemaphore() override;
	virtual Rc<core::ImageView> makeImageView(const Rc<core::ImageObject> &, const ImageViewInfo &) override;
	virtual Rc<core::CommandPool> makeCommandPool(uint32_t family, core::QueueFlags flags) override;

	template <typename Callback>
	void makeApiCall(const Callback &cb) {
		//_apiMutex.lock();
		cb(*getTable(), getDevice());
		//_apiMutex.unlock();
	}

	bool hasNonSolidFillMode() const;
	bool hasDynamicIndexedBuffers() const;
	bool hasBufferDeviceAddresses() const;
	bool hasExternalFences() const;
	bool isPortabilityMode() const;

	virtual void waitIdle() const override;

	void compileImage(const Loop &loop, const Rc<core::DynamicImage> &, Function<void(bool)> &&);

private:
	using core::Device::init;

	friend class DeviceQueue;

	// TODO - move this to queue compiler
	virtual void compileSamplers(event::Looper *looper, bool force);

	bool setup(const Instance *instance, VkPhysicalDevice p, const Properties &prop,
			const Vector<core::DeviceQueueFamily> &queueFamilies,
			const Features &features, const Vector<StringView> &requiredExtension);

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

	bool _finished = false;
	bool _updateAfterBindEnabled = true;

	Vector<VkSampler> _immutableSamplers;
	Vector<Rc<Sampler>> _samplers;
	size_t _compiledSamplers = 0;
	std::atomic<bool> _samplersCompiled = false;

	std::unordered_map<VkFormat, VkFormatProperties> _formats;

	std::condition_variable _resourceQueueCond;
	Mutex _apiMutex;
};

}

#endif /* XENOLITH_BACKEND_VK_XLVKDEVICE_H_ */
