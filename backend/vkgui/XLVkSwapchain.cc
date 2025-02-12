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

#include "XLVkSwapchain.h"
#include "XLVkGuiConfig.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

Surface::~Surface() {
	if (_surface) {
		_instance->vkDestroySurfaceKHR(_instance->getInstance(), _surface, nullptr);
		_surface = VK_NULL_HANDLE;
	}
	_window = nullptr;
}

bool Surface::init(Instance *instance, VkSurfaceKHR surface, Ref *win) {
	if (!surface) {
		return false;
	}
	_instance = instance;
	_surface = surface;
	_window = win;
	return true;
}

SwapchainHandle::~SwapchainHandle() {
	for (auto &it : _images) {
		for (auto &v : it.views) {
			if (v.second) {
				v.second->runReleaseCallback();
				v.second->invalidate();
				v.second = nullptr;
			}
		}
	}

	invalidate();

	_semaphores.clear();

	if (_device) {
		for (auto &it : _presentSemaphores) {
			if (it) {
				_device->invalidateSemaphore(move(it));
				it = nullptr;
			}
		}
	}

	_presentSemaphores.clear();
}

bool SwapchainHandle::init(Device &dev, const core::SurfaceInfo &info, const core::SwapchainConfig &cfg, ImageInfo &&swapchainImageInfo,
		core::PresentMode presentMode, Surface *surface, uint32_t families[2], SwapchainHandle *old) {

	_device = &dev;

	VkSwapchainCreateInfoKHR swapChainCreateInfo{}; sanitizeVkStruct(swapChainCreateInfo);
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = surface->getSurface();
	swapChainCreateInfo.minImageCount = cfg.imageCount;
	swapChainCreateInfo.imageFormat = VkFormat(swapchainImageInfo.format);
	swapChainCreateInfo.imageColorSpace = VkColorSpaceKHR(cfg.colorSpace);
	swapChainCreateInfo.imageExtent = VkExtent2D({swapchainImageInfo.extent.width, swapchainImageInfo.extent.height});
	swapChainCreateInfo.imageArrayLayers = swapchainImageInfo.arrayLayers.get();
	swapChainCreateInfo.imageUsage = VkImageUsageFlags(swapchainImageInfo.usage);

	if (families[0] != families[1]) {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.queueFamilyIndexCount = 2;
		swapChainCreateInfo.pQueueFamilyIndices = families;
	} else {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	if ((cfg.transform & core::SurfaceTransformFlags::PreRotated) != core::SurfaceTransformFlags::None) {
		swapChainCreateInfo.preTransform = VkSurfaceTransformFlagBitsKHR(core::getPureTransform(cfg.transform));
	} else {
		swapChainCreateInfo.preTransform = VkSurfaceTransformFlagBitsKHR(cfg.transform);
	}
	swapChainCreateInfo.compositeAlpha = VkCompositeAlphaFlagBitsKHR(cfg.alpha);
	swapChainCreateInfo.presentMode = getVkPresentMode(presentMode);
	swapChainCreateInfo.clipped = (cfg.clipped ? VK_TRUE : VK_FALSE);

	if (old) {
		swapChainCreateInfo.oldSwapchain = old->getSwapchain();
	} else {
		swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
	}

	VkResult result = VK_ERROR_UNKNOWN;
	dev.makeApiCall([&, this] (const DeviceTable &table, VkDevice device) {
#if XL_VKAPI_DEBUG
		auto t = xenolith::platform::clock(core::ClockType::Monotonic);
		result = table.vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &_swapchain);
		XL_VKAPI_LOG("vkCreateSwapchainKHR: ", result,
				" [", xenolith::platform::clock(core::ClockType::Monotonic) - t, "]");
#else
		result = table.vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &_swapchain);
#endif
	});
	if (result == VK_SUCCESS) {
		Vector<VkImage> swapchainImages;

		uint32_t imageCount = 0;
		dev.getTable()->vkGetSwapchainImagesKHR(dev.getDevice(), _swapchain, &imageCount, nullptr);
		swapchainImages.resize(imageCount);
		dev.getTable()->vkGetSwapchainImagesKHR(dev.getDevice(), _swapchain, &imageCount, swapchainImages.data());

		_images.reserve(imageCount);
		_presentSemaphores.resize(imageCount);

		if (old) {
			std::unique_lock<Mutex> lock(_resourceMutex);
			std::unique_lock<Mutex> lock2(old->_resourceMutex);
			_semaphores = sp::move(old->_semaphores);

			for (auto &it : old->_presentSemaphores) {
				if (it) {
					if (!releaseSemaphore(move(it))) {
						_invalidatedSemaphores.emplace_back(move(it));
					}
					it = nullptr;
				}
			}
		}

		auto swapchainImageViewInfo = getSwapchainImageViewInfo(swapchainImageInfo);

		for (auto &it : swapchainImages) {
			auto image = Rc<Image>::create(dev, it, swapchainImageInfo, uint32_t(_images.size()));

			Map<ImageViewInfo, Rc<ImageView>> views;
			views.emplace(swapchainImageViewInfo, Rc<ImageView>::create(dev, image.get(), swapchainImageViewInfo));

			_images.emplace_back(SwapchainImageData{sp::move(image), sp::move(views)});
		}

		_rebuildMode = _presentMode = presentMode;
		_imageInfo = move(swapchainImageInfo);
		_config = move(cfg);
		_config.imageCount = imageCount;
		_surface = surface;
		_surfaceInfo = info;

		return core::Object::init(dev, [] (core::Device *dev, core::ObjectType, ObjectHandle ptr, void *) {
			auto d = ((Device *)dev);
			d->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
#if XL_VKAPI_DEBUG
				auto t = xenolith::platform::clock(core::ClockType::Monotonic);
				table.vkDestroySwapchainKHR(device, (VkSwapchainKHR)ptr.get(), nullptr);
				XL_VKAPI_LOG("vkDestroySwapchainKHR: [",xenolith::platform::clock(core::ClockType::Monotonic) - t, "]");
#else
				table.vkDestroySwapchainKHR(device, (VkSwapchainKHR)ptr.get(), nullptr);
#endif
			});
		}, core::ObjectType::Swapchain, ObjectHandle(_swapchain));
	}
	return false;
}

bool SwapchainHandle::isDeprecated() {
	return _deprecated;
}

bool SwapchainHandle::isOptimal() const {
	return _presentMode == _config.presentMode;
}

bool SwapchainHandle::deprecate(bool fast) {
	auto tmp = _deprecated;
	_deprecated = true;
	if (fast && _config.presentModeFast != core::PresentMode::Unsupported) {
		_rebuildMode = _config.presentModeFast;
	}
	return !tmp;
}

auto SwapchainHandle::acquire(bool lockfree, const Rc<Fence> &fence) -> Rc<SwapchainAcquiredImage> {
	if (_deprecated) {
		return nullptr;
	}

	uint64_t timeout = lockfree ? 0 : maxOf<uint64_t>();
	Rc<Semaphore> sem = acquireSemaphore();
	uint32_t imageIndex = maxOf<uint32_t>();
	VkResult ret = VK_ERROR_UNKNOWN;
	_device->makeApiCall([&, this] (const DeviceTable &table, VkDevice device) {
#if XL_VKAPI_DEBUG
		auto t = xenolith::platform::clock(core::ClockType::Monotonic);
		ret = table.vkAcquireNextImageKHR(device, _swapchain, timeout,
				sem ? sem->getSemaphore() : VK_NULL_HANDLE, fence->getFence(), &imageIndex);
		XL_VKAPI_LOG("vkAcquireNextImageKHR: ", imageIndex, " ", ret,
				" [", xenolith::platform::clock(core::ClockType::Monotonic) - t, "]");
#else
		ret = table.vkAcquireNextImageKHR(device, _swapchain, timeout,
				sem ? sem->getSemaphore() : VK_NULL_HANDLE, fence ? fence->getFence() : VK_NULL_HANDLE, &imageIndex);
#endif
	});

	Rc<SwapchainAcquiredImage> image;
	switch (ret) {
	case VK_SUCCESS:
		if (sem) {
			sem->setSignaled(true);
		}
		if (fence) {
			fence->setTag("SwapchainHandle::acquire");
			fence->setArmed();
		}
		++ _acquiredImages;
		return Rc<SwapchainAcquiredImage>::alloc(imageIndex, &_images.at(imageIndex), move(sem), this);
		break;
	case VK_SUBOPTIMAL_KHR:
		if (sem) {
			sem->setSignaled(true);
		}
		if (fence) {
			fence->setTag("SwapchainHandle::acquire");
			fence->setArmed();
		}
		_deprecated = true;
		++ _acquiredImages;
		return Rc<SwapchainAcquiredImage>::alloc(imageIndex, &_images.at(imageIndex), move(sem), this);
		break;
	case VK_ERROR_OUT_OF_DATE_KHR:
		_deprecated = true;
		releaseSemaphore(move(sem));
		break;
	default:
		releaseSemaphore(move(sem));
		break;
	}

	return nullptr;
}

VkResult SwapchainHandle::present(DeviceQueue &queue, const Rc<ImageStorage> &image) {
	auto waitSem = (Semaphore *)image->getSignalSem().get();
	auto waitSemObj = waitSem->getSemaphore();
	auto imageIndex = uint32_t(image->getImageIndex());

	VkPresentInfoKHR presentInfo{}; sanitizeVkStruct(presentInfo);
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &waitSemObj;

	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr; // Optional

	VkResult result = VK_ERROR_UNKNOWN;
	_device->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
#if XL_VKAPI_DEBUG
		auto t = xenolith::platform::clock(core::ClockType::Monotonic);
		result = table.vkQueuePresentKHR(queue.getQueue(), &presentInfo);
		XL_VKAPI_LOG("[", image->getFrameIndex(), "] vkQueuePresentKHR: ", imageIndex, " ", result,
				" [", xenolith::platform::clock(core::ClockType::Monotonic) - t, "] [timeout: ", t - _presentTime,
				"] [acquisition: ", t - image->getAcquisitionTime(), "]");
		_presentTime = t;
#else
		result = table.vkQueuePresentKHR(queue.getQueue(), &presentInfo);
#endif
	});

	do {
		std::unique_lock<Mutex> lock(_resourceMutex);
		((SwapchainImage *)image.get())->setPresented();
		-- _acquiredImages;
	} while (0);

	if (_presentSemaphores[imageIndex]) {
		_presentSemaphores[imageIndex]->setWaited(true);
		releaseSemaphore(move(_presentSemaphores[imageIndex]));
		_presentSemaphores[imageIndex] = nullptr;
	}

	/*for (auto &it : _presentSemaphores) {
		if (it) {
			it->setWaited(true);
			releaseSemaphore(move(it));
			it = nullptr;
		}
	}*/

	_presentSemaphores[imageIndex] = waitSem;

	if (result == VK_SUCCESS) {
		++ _presentedFrames;
		if (_presentedFrames == config::MaxSuboptimalFrames && _presentMode == _config.presentModeFast
				&& _config.presentModeFast != _config.presentMode) {
			result = VK_SUBOPTIMAL_KHR;
			_rebuildMode = _config.presentMode;
		}
	}

	return result;
}

void SwapchainHandle::invalidateImage(const ImageStorage *image) {
	if (!((SwapchainImage *)image)->isPresented()) {
		std::unique_lock<Mutex> lock(_resourceMutex);
		-- _acquiredImages;
	}
}

Rc<Semaphore> SwapchainHandle::acquireSemaphore() {
	std::unique_lock<Mutex> lock(_resourceMutex);
	if (!_semaphores.empty()) {
		auto sem = _semaphores.back();
		_semaphores.pop_back();
		return sem;
	}
	lock.unlock();

	return Rc<Semaphore>::create(*_device);
}

bool SwapchainHandle::releaseSemaphore(Rc<Semaphore> &&sem) {
	if (sem && sem->reset()) {
		std::unique_lock<Mutex> lock(_resourceMutex);
		_semaphores.emplace_back(move(sem));
		return true;
	}
	return false;
}

Rc<core::ImageView> SwapchainHandle::makeView(const Rc<core::ImageObject> &image, const ImageViewInfo &viewInfo) {
	auto img = (Image *)image.get();
	auto idx = img->getIndex();

	auto it = _images[idx].views.find(viewInfo);
	if (it != _images[idx].views.end()) {
		return it->second;
	}

	it = _images[idx].views.emplace(viewInfo, Rc<ImageView>::create(*_device, img, viewInfo)).first;
	return it->second;
}

ImageViewInfo SwapchainHandle::getSwapchainImageViewInfo(const ImageInfo &image) const {
	ImageViewInfo info;
	switch (image.imageType) {
	case core::ImageType::Image1D:
		info.type = core::ImageViewType::ImageView1D;
		break;
	case core::ImageType::Image2D:
		info.type = core::ImageViewType::ImageView2D;
		break;
	case core::ImageType::Image3D:
		info.type = core::ImageViewType::ImageView3D;
		break;
	}

	return image.getViewInfo(info);
}


SwapchainImage::~SwapchainImage() {
	if (_state != State::Presented) {
		if (_image) {
			_swapchain->invalidateImage(this);
		}
		_image = nullptr;
		_swapchain = nullptr;
		_state = State::Presented;
	} else {
		if (_swapchain && _waitSem) {
			_swapchain->releaseSemaphore((Semaphore *)_waitSem.get());
		}
	}
	// prevent views from released
	_views.clear();

	_waitSem = nullptr;
	_signalSem = nullptr;
}

bool SwapchainImage::init(Rc<SwapchainHandle> &&swapchain, uint64_t order, uint64_t presentWindow) {
	_swapchain = move(swapchain);
	_order = order;
	_presentWindow = presentWindow;
	_state = State::Submitted;
	_isSwapchainImage = true;
	return true;
}

bool SwapchainImage::init(Rc<SwapchainHandle> &&swapchain, const SwapchainHandle::SwapchainImageData &image, Rc<Semaphore> &&sem) {
	_swapchain = move(swapchain);
	_image = image.image.get();
	for (auto &it : image.views) {
		_views.emplace(it.first, it.second);
	}
	if (sem) {
		_waitSem = sem.get();
	}
	_signalSem = _swapchain->acquireSemaphore().get();
	_state = State::Submitted;
	_isSwapchainImage = true;
	return true;
}

void SwapchainImage::cleanup() {
	stappler::log::info("SwapchainImage", "cleanup");
}

void SwapchainImage::rearmSemaphores(core::Loop &loop) {
	ImageStorage::rearmSemaphores(loop);
}

void SwapchainImage::releaseSemaphore(core::Semaphore *sem) {
	if (_state == State::Presented && sem == _waitSem && _swapchain) {
		// work on last submit is over, wait sem no longer in use
		if (_swapchain->releaseSemaphore((Semaphore *)sem)) {
			_waitSem = nullptr;
		}
	}
}

ImageInfoData SwapchainImage::getInfo() const {
	if (_image) {
		return _image->getInfo();
	} else if (_swapchain) {
		return _swapchain->getImageInfo();
	}
    return ImageInfoData();
}

Rc<core::ImageView> SwapchainImage::makeView(const ImageViewInfo &info) {
	auto it = _views.find(info);
	if (it != _views.end()) {
		return it->second;
	}

	it = _views.emplace(info, _swapchain->makeView(_image, info)).first;
	return it->second;
}

void SwapchainImage::setImage(Rc<SwapchainHandle> &&handle, const SwapchainHandle::SwapchainImageData &image, const Rc<Semaphore> &sem) {
	_image = image.image.get();
	for (auto &it : image.views) {
		_views.emplace(it.first, it.second);
	}
	if (sem) {
		_waitSem = sem.get();
	}
	_signalSem = _swapchain->acquireSemaphore().get();
}

void SwapchainImage::setPresented() {
	_state = State::Presented;
}

void SwapchainImage::invalidateImage() {
	if (_image) {
		_swapchain->invalidateImage(this);
	}
	_swapchain = nullptr;
	_state = State::Presented;
}

void SwapchainImage::invalidateSwapchain() {
	_swapchain = nullptr;
	_image = nullptr;
	_state = State::Presented;
}

}

