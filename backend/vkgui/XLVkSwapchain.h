/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_BACKEND_VKGUI_XLVKSWAPCHAIN_H_
#define XENOLITH_BACKEND_VKGUI_XLVKSWAPCHAIN_H_

#include "XLVkDevice.h"
#include "XLVkObject.h"
#include "XLCoreImageStorage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class SwapchainImage;

class Surface : public Ref {
public:
	virtual ~Surface();

	bool init(Instance *instance, VkSurfaceKHR surface, Ref * = nullptr);

	VkSurfaceKHR getSurface() const { return _surface; }

protected:
	Rc<Ref> _window;
	Rc<Instance> _instance;
	VkSurfaceKHR _surface = VK_NULL_HANDLE;
};

class SwapchainHandle : public core::Object {
public:
	using ImageStorage = core::ImageStorage;

	struct SwapchainImageData {
		Rc<Image> image;
		Map<ImageViewInfo, Rc<ImageView>> views;
	};

	struct SwapchainAcquiredImage : public Ref {
		uint32_t imageIndex;
		const SwapchainImageData *data;
		Rc<Semaphore> sem;
		Rc<SwapchainHandle> swapchain;

		SwapchainAcquiredImage(uint32_t idx, const SwapchainImageData *data, Rc<Semaphore> &&sem, Rc<SwapchainHandle> &&swapchain)
		: imageIndex(idx), data(data), sem(move(sem)), swapchain(move(swapchain)) { }
	};

	virtual ~SwapchainHandle();

	bool init(Device &dev, const core::SurfaceInfo &, const core::SwapchainConfig &, ImageInfo &&, core::PresentMode,
			Surface *, uint32_t families[2], SwapchainHandle * = nullptr);

	core::PresentMode getPresentMode() const { return _presentMode; }
	core::PresentMode getRebuildMode() const { return _rebuildMode; }
	const ImageInfo &getImageInfo() const { return _imageInfo; }
	const core::SwapchainConfig &getConfig() const { return _config; }
	const core::SurfaceInfo &getSurfaceInfo() const { return _surfaceInfo; }
	VkSwapchainKHR getSwapchain() const { return _swapchain; }
	uint32_t getAcquiredImagesCount() const { return _acquiredImages; }
	uint64_t getPresentedFramesCount() const { return _presentedFrames; }
	const Vector<SwapchainImageData> &getImages() const { return _images; }

	bool isDeprecated();
	bool isOptimal() const;

	// returns true if it was first deprecation
	bool deprecate(bool fast);

	Rc<SwapchainAcquiredImage> acquire(bool lockfree, const Rc<Fence> &fence);

	VkResult present(DeviceQueue &queue, const Rc<ImageStorage> &);
	void invalidateImage(const ImageStorage *);

	Rc<Semaphore> acquireSemaphore();
	void releaseSemaphore(Rc<Semaphore> &&);

	Rc<core::ImageView> makeView(const Rc<core::ImageObject> &, const ImageViewInfo &);

protected:
	using core::Object::init;

	ImageViewInfo getSwapchainImageViewInfo(const ImageInfo &image) const;

	Device *_device = nullptr;
	bool _deprecated = false;
	core::PresentMode _presentMode = core::PresentMode::Unsupported;
	ImageInfo _imageInfo;
	core::SurfaceInfo _surfaceInfo;
	core::SwapchainConfig _config;
	VkSwapchainKHR _swapchain = VK_NULL_HANDLE;
	Vector<SwapchainImageData> _images;
	uint32_t _acquiredImages = 0;
	uint64_t _presentedFrames = 0;
	uint64_t _presentTime = 0;
	core::PresentMode _rebuildMode = core::PresentMode::Unsupported;

	Mutex _resourceMutex;
	Vector<Rc<Semaphore>> _semaphores;
	Vector<Rc<Semaphore>> _presentSemaphores;
	Rc<Surface> _surface;
};

class SwapchainImage : public core::ImageStorage {
public:
	enum class State {
		Initial,
		Submitted,
		Presented,
	};

	virtual ~SwapchainImage();

	virtual bool init(Rc<SwapchainHandle> &&, uint64_t frameOrder, uint64_t presentWindow);
	virtual bool init(Rc<SwapchainHandle> &&, const SwapchainHandle::SwapchainImageData &, Rc<Semaphore> &&);

	virtual void cleanup() override;
	virtual void rearmSemaphores(core::Loop &) override;
	virtual void releaseSemaphore(core::Semaphore *) override;

	virtual bool isSemaphorePersistent() const override { return false; }

	virtual ImageInfoData getInfo() const override;

	virtual Rc<core::ImageView> makeView(const ImageViewInfo &) override;

	void setImage(Rc<SwapchainHandle> &&, const SwapchainHandle::SwapchainImageData &, const Rc<Semaphore> &);

	uint64_t getOrder() const { return _order; }
	uint64_t getPresentWindow() const { return _presentWindow; }

	void setPresented();
	bool isPresented() const { return _state == State::Presented; }
	bool isSubmitted() const { return _state == State::Submitted || _state == State::Presented; }

	const Rc<SwapchainHandle> &getSwapchain() const { return _swapchain; }

	void invalidateImage();
	void invalidateSwapchain();

protected:
	using core::ImageStorage::init;

	uint64_t _order = 0;
	uint64_t _presentWindow = 0;
	State _state = State::Initial;
	Rc<SwapchainHandle> _swapchain;
};

}

#endif /* XENOLITH_BACKEND_VKGUI_XLVKSWAPCHAIN_H_ */
