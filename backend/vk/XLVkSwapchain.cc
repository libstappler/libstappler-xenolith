/**
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

#include "XLVkSwapchain.h"
#include "XLVk.h"
#include "XLVkConfig.h"
#include "XLVkInstance.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

Surface::~Surface() {
	if (_surface) {
		auto inst = _instance.get_cast<Instance>();
		inst->vkDestroySurfaceKHR(inst->getInstance(), _surface, nullptr);
		_surface = VK_NULL_HANDLE;
	}
	_window = nullptr;
}

bool Surface::init(Instance *instance, VkSurfaceKHR surface, Ref *win) {
	if (!surface) {
		return false;
	}

	if (!core::Surface::init(instance, win)) {
		return false;
	}

	_surface = surface;
	return true;
}

core::SurfaceInfo Surface::getSurfaceOptions(const core::Device &dev) const {
	return _instance.get_cast<Instance>()->getSurfaceOptions(_surface,
			static_cast<const Device &>(dev).getPhysicalDevice());
}

static void SwapchainHandle_destroy(core::Device *dev, core::ObjectType, core::ObjectHandle handle,
		void *ptr) {
	auto data = reinterpret_cast<SwapchainHandle::SwapchainHandleData *>(ptr);

	auto d = static_cast<Device *>(dev);
	d->makeApiCall([&](const DeviceTable &table, VkDevice device) {
#if XL_VKAPI_DEBUG
		auto t = sp::platform::clock(ClockType::Monotonic);
		table.vkDestroySwapchainKHR(device, (VkSwapchainKHR)handle.get(), nullptr);
		XL_VKAPI_LOG("vkDestroySwapchainKHR: [", sp::platform::clock(ClockType::Monotonic) - t,
				"]");
#else
		table.vkDestroySwapchainKHR(device, (VkSwapchainKHR)handle.get(), nullptr);
#endif
	});

	data->invalidate(*dev);
	delete data;
}

bool SwapchainHandle::init(Device &dev, const core::SurfaceInfo &info,
		const core::SwapchainConfig &cfg, ImageInfo &&swapchainImageInfo,
		core::PresentMode presentMode, Surface *surface, uint32_t families[2],
		SwapchainHandle *old) {

	VkSwapchainCreateInfoKHR swapChainCreateInfo{};
	sanitizeVkStruct(swapChainCreateInfo);
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = surface->getSurface();
	swapChainCreateInfo.minImageCount = cfg.imageCount;
	swapChainCreateInfo.imageFormat = VkFormat(swapchainImageInfo.format);
	swapChainCreateInfo.imageColorSpace = VkColorSpaceKHR(cfg.colorSpace);
	swapChainCreateInfo.imageExtent =
			VkExtent2D({swapchainImageInfo.extent.width, swapchainImageInfo.extent.height});
	swapChainCreateInfo.imageArrayLayers = swapchainImageInfo.arrayLayers.get();
	swapChainCreateInfo.imageUsage = VkImageUsageFlags(swapchainImageInfo.usage);

	if (families[0] != families[1]) {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.queueFamilyIndexCount = 2;
		swapChainCreateInfo.pQueueFamilyIndices = families;
	} else {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	if ((cfg.transform & core::SurfaceTransformFlags::PreRotated)
			!= core::SurfaceTransformFlags::None) {
		swapChainCreateInfo.preTransform =
				VkSurfaceTransformFlagBitsKHR(core::getPureTransform(cfg.transform));
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

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkResult result = VK_ERROR_UNKNOWN;
	dev.makeApiCall([&](const DeviceTable &table, VkDevice device) {
#if XL_VKAPI_DEBUG
		auto t = sp::platform::clock(ClockType::Monotonic);
		result = table.vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &swapchain);
		XL_VKAPI_LOG("vkCreateSwapchainKHR: ", result, " [",
				sp::platform::clock(ClockType::Monotonic) - t, "]");
#else
		result = table.vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &swapchain);
#endif
	});

	if (result == VK_SUCCESS) {
		auto data = new SwapchainHandleData;
		data->swapchain = swapchain;

		Vector<VkImage> swapchainImages;

		uint32_t imageCount = 0;
		dev.getTable()->vkGetSwapchainImagesKHR(dev.getDevice(), data->swapchain, &imageCount,
				nullptr);
		swapchainImages.resize(imageCount);
		dev.getTable()->vkGetSwapchainImagesKHR(dev.getDevice(), data->swapchain, &imageCount,
				swapchainImages.data());

		data->images.reserve(imageCount);
		data->presentSemaphores.resize(imageCount);

		if (old) {
			std::unique_lock<Mutex> lock(_resourceMutex);
			std::unique_lock<Mutex> lock2(old->_resourceMutex);
			data->semaphores = sp::move(old->_data->semaphores);

			for (auto &it : old->_data->presentSemaphores) {
				if (it) {
					if (!releaseSemaphore(ref_cast<Semaphore>(move(it)))) {
						_invalidatedSemaphores.emplace_back(move(it));
					}
					it = nullptr;
				}
			}
		}

		auto swapchainImageViewInfo = getSwapchainImageViewInfo(swapchainImageInfo);

		for (auto &it : swapchainImages) {
			auto image = Rc<Image>::create(dev,
					toString("SwapchainImage[", uint32_t(data->images.size()), "]"), it,
					swapchainImageInfo, uint32_t(data->images.size()));

			Map<ImageViewInfo, Rc<core::ImageView>> views;
			views.emplace(swapchainImageViewInfo,
					Rc<ImageView>::create(dev, image.get(), swapchainImageViewInfo));

			data->images.emplace_back(SwapchainImageData{sp::move(image), sp::move(views)});
		}

		_presentMode = presentMode;
		_imageInfo = move(swapchainImageInfo);
		_config = move(cfg);
		_config.imageCount = imageCount;
		_surface = surface;
		_surfaceInfo = info;
		_data = data;

		return core::Swapchain::init(dev, SwapchainHandle_destroy, core::ObjectType::Swapchain,
				ObjectHandle(_data->swapchain), _data);
	} else {
		log::error("SwapchainHandle", "Fail to create swapchain: ", getStatus(result));
	}
	return false;
}

SpanView<SwapchainHandle::SwapchainImageData> SwapchainHandle::getImages() const {
	return _data->images;
}

auto SwapchainHandle::acquire(bool lockfree, const Rc<core::Fence> &fence, Status &status)
		-> Rc<SwapchainAcquiredImage> {
	if (_deprecated) {
		return nullptr;
	}

	uint64_t timeout = lockfree ? 0 : maxOf<uint64_t>();
	Rc<core::Semaphore> sem = acquireSemaphore();

	auto dev = static_cast<Device *>(_object.device);

	uint32_t imageIndex = maxOf<uint32_t>();
	VkResult ret = VK_ERROR_UNKNOWN;
	dev->makeApiCall([&, this](const DeviceTable &table, VkDevice device) {
#if XL_VKAPI_DEBUG
		auto t = sp::platform::clock(ClockType::Monotonic);
#endif
		if (table.vkAcquireNextImage2KHR) {
			VkAcquireNextImageInfoKHR info;
			info.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR;
			info.pNext = nullptr;
			info.swapchain = _data->swapchain;
			info.timeout = timeout;
			info.semaphore = sem ? sem.get_cast<Semaphore>()->getSemaphore() : VK_NULL_HANDLE;
			info.fence = fence ? fence.get_cast<Fence>()->getFence() : VK_NULL_HANDLE;
			info.deviceMask = 1;

			ret = table.vkAcquireNextImage2KHR(device, &info, &imageIndex);
		} else {
			ret = table.vkAcquireNextImageKHR(device, _data->swapchain, timeout,
					sem ? sem.get_cast<Semaphore>()->getSemaphore() : VK_NULL_HANDLE,
					fence ? fence.get_cast<Fence>()->getFence() : VK_NULL_HANDLE, &imageIndex);
		}
#if XL_VKAPI_DEBUG
		XL_VKAPI_LOG("vkAcquireNextImageKHR: ", imageIndex, " ", ret, " [",
				sp::platform::clock(ClockType::Monotonic) - t, "]");
#endif
	});

	status = getStatus(ret);

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
		++_acquiredImages;
		return Rc<SwapchainAcquiredImage>::alloc(imageIndex, &_data->images.at(imageIndex),
				move(sem), this);
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
		++_acquiredImages;
		return Rc<SwapchainAcquiredImage>::alloc(imageIndex, &_data->images.at(imageIndex),
				move(sem), this);
		break;
	case VK_ERROR_OUT_OF_DATE_KHR:
		_deprecated = true;
		releaseSemaphore(ref_cast<Semaphore>(move(sem)));
		break;
	case VK_TIMEOUT: releaseSemaphore(ref_cast<Semaphore>(move(sem))); break;
	default:
		releaseSemaphore(ref_cast<Semaphore>(move(sem)));
		log::error("vk::SwapchainHandle", "Fail to acquire image: ", getStatus(ret));
		break;
	}

	return nullptr;
}

Status SwapchainHandle::present(core::DeviceQueue &queue, core::ImageStorage *image) {
	if (_invalid) {
		return Status::ErrorCancelled;
	}

	auto waitSem = (Semaphore *)image->getSignalSem().get();
	auto waitSemObj = waitSem->getSemaphore();
	auto imageIndex = uint32_t(image->getImageIndex());

	auto dev = static_cast<Device *>(_object.device);

	VkPresentInfoKHR presentInfo{};
	sanitizeVkStruct(presentInfo);
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &waitSemObj;

	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &_data->swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr; // Optional

	VkResult result = VK_ERROR_UNKNOWN;
	dev->makeApiCall([&](const DeviceTable &table, VkDevice device) {
#if XL_VKAPI_DEBUG
		auto t = sp::platform::clock(ClockType::Monotonic);
		result =
				table.vkQueuePresentKHR(static_cast<DeviceQueue &>(queue).getQueue(), &presentInfo);
		XL_VKAPI_LOG("[", image->getFrameIndex(), "] vkQueuePresentKHR: ", imageIndex, " ", result,
				" [", sp::platform::clock(ClockType::Monotonic) - t,
				"] [timeout: ", t - _presentTime,
				"] [acquisition: ", t - image->getAcquisitionTime(), "]");
		_presentTime = t;
#else
		result =
				table.vkQueuePresentKHR(static_cast<DeviceQueue &>(queue).getQueue(), &presentInfo);
#endif
	});

	do {
		std::unique_lock<Mutex> lock(_resourceMutex);
		static_cast<core::SwapchainImage *>(image)->setPresented();
		--_acquiredImages;
	} while (0);

	if (_data->presentSemaphores[imageIndex]) {
		_data->presentSemaphores[imageIndex]->setWaited(true);
		releaseSemaphore(ref_cast<Semaphore>(move(_data->presentSemaphores[imageIndex])));
		_data->presentSemaphores[imageIndex] = nullptr;
	}

	_data->presentSemaphores[imageIndex] = waitSem;

	if (result == VK_SUCCESS) {
		++_presentedFrames;
		if (_presentedFrames == config::MaxSuboptimalFrames
				&& _presentMode == _config.presentModeFast
				&& _config.presentModeFast != _config.presentMode) {
			result = VK_SUBOPTIMAL_KHR;
		}
	} else if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		_invalid = true;
	}

	return getStatus(result);
}

void SwapchainHandle::invalidateImage(const core::ImageStorage *image) {
	if (!static_cast<const core::SwapchainImage *>(image)->isPresented()) {
		std::unique_lock<Mutex> lock(_resourceMutex);
		--_acquiredImages;
	}
}

Rc<core::Semaphore> SwapchainHandle::acquireSemaphore() {
	std::unique_lock<Mutex> lock(_resourceMutex);
	if (!_data->semaphores.empty()) {
		auto sem = _data->semaphores.back();
		_data->semaphores.pop_back();
		return sem;
	}
	lock.unlock();

	return Rc<Semaphore>::create(*static_cast<Device *>(_object.device),
			core::SemaphoreType::Default);
}

bool SwapchainHandle::releaseSemaphore(Rc<core::Semaphore> &&sem) {
	if (sem && sem->reset()) {
		std::unique_lock<Mutex> lock(_resourceMutex);
		_data->semaphores.emplace_back(move(sem));
		return true;
	}
	return false;
}

Rc<core::ImageView> SwapchainHandle::makeView(const Rc<core::ImageObject> &image,
		const ImageViewInfo &viewInfo) {
	auto img = (Image *)image.get();
	auto idx = img->getIndex();

	auto dev = static_cast<Device *>(_object.device);

	auto it = _data->images[idx].views.find(viewInfo);
	if (it != _data->images[idx].views.end()) {
		return it->second;
	}

	it = _data->images[idx]
				 .views.emplace(viewInfo, Rc<ImageView>::create(*dev, img, viewInfo))
				 .first;
	return it->second;
}

} // namespace stappler::xenolith::vk
