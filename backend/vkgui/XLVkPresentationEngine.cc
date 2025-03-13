/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#include "XLVkPresentationEngine.h"
#include "XLCorePresentationFrame.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameCache.h"
#include "XLDirector.h"

#define XL_VKPRESENT_DEBUG 0

#ifndef XL_VKAPI_LOG
#define XL_VKAPI_LOG(...)
#endif

#if XL_VKPRESENT_DEBUG
#define XL_VKPRESENT_LOG(...) log::debug("vk::PresentationEngine", __VA_ARGS__)
#else
#define XL_VKPRESENT_LOG(...)
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

bool PresentationEngine::init(Device *dev, View *view, Rc<Surface> &&surface, core::FrameConstraints &&c, uint64_t frameInterval) {
	_view = view;
	_surface = move(surface);
	_targetFrameInterval = frameInterval;

	_device = dev;
	_loop = view->getGlLoop();

	_constraints = move(c);

	return true;
}

bool PresentationEngine::run() {
	auto info = _surface->getSurfaceOptions(*static_cast<Device *>(_device));
	auto cfg = _view->selectConfig(info);

	createSwapchain(info, move(cfg), cfg.presentMode);

	return core::PresentationEngine::run();
}

bool PresentationEngine::recreateSwapchain(core::PresentMode mode) {
	XL_VKPRESENT_LOG("recreateSwapchain");

	resetFrames();

	if (mode == core::PresentMode::Unsupported) {
		return false;
	}

	auto info = _surface->getSurfaceOptions(*static_cast<Device *>(_device));
	auto cfg = _view->selectConfig(info);

	if (!info.isSupported(cfg)) {
		log::error("Vk-Error", "Presentation with config ", cfg.description(), " is not supported for ", info.description());
		return false;
	}

	if (cfg.extent.width == 0 || cfg.extent.height == 0) {
		return false;
	}

	bool ret = false;
	if (mode == core::PresentMode::Unsupported) {
		ret = createSwapchain(info, move(cfg), cfg.presentMode);
	} else {
		ret = createSwapchain(info, move(cfg), mode);
	}
	if (ret) {
		_nextPresentWindow = 0;
		XL_VKPRESENT_LOG("recreateSwapchain - scheduleNextImage");
		// run frame, no present window, no wait on fences
		scheduleNextImage();
	}
	return ret;
}

bool PresentationEngine::createSwapchain(const core::SurfaceInfo &info, core::SwapchainConfig &&cfg, core::PresentMode presentMode) {
	auto dev = static_cast<Device *>(_device);
	auto devInfo = dev->getInfo();

	auto swapchainImageInfo = _view->getSwapchainImageInfo(cfg);
	uint32_t queueFamilyIndices[] = { devInfo.graphicsFamily.index, devInfo.presentFamily.index };

	do {
		auto oldSwapchain = move(_swapchain);

		log::verbose("vk::View", "Surface: ", info.description());
		_swapchain = Rc<SwapchainHandle>::create(*dev, info, cfg, move(swapchainImageInfo), presentMode,
				_surface, queueFamilyIndices, oldSwapchain ? oldSwapchain.get_cast<SwapchainHandle>() : nullptr);

		if (_swapchain) {
			_constraints.extent = cfg.extent;
			_constraints.transform = cfg.transform;

			Vector<uint64_t> ids;
			auto &cache = _loop->getFrameCache();
			for (auto &it : static_cast<SwapchainHandle *>(_swapchain.get())->getImages()) {
				for (auto &iit : it.views) {
					auto id = iit.second->getIndex();
					ids.emplace_back(iit.second->getIndex());
					iit.second->setReleaseCallback([loop = _loop, cache, id] {
						cache->removeImageView(id);
					});
				}
			}

			for (auto &id : ids) {
				cache->addImageView(id);
			}

			log::verbose("vk::View", "Swapchain: ", cfg.description());
		}
	} while (0);

	return _swapchain != nullptr;
}

void PresentationEngine::handleFramePresented(core::PresentationFrame *frame) {
	_view->handleFramePresented(frame);
	core::PresentationEngine::handleFramePresented(frame);
}

void PresentationEngine::acquireFrameData(core::PresentationFrame *frame, Function<void(core::PresentationFrame *)> &&cb) {
	_view->getApplication()->performOnAppThread([this, frame = Rc<core::PresentationFrame>(frame), cb = sp::move(cb)] () mutable {
		XL_VKPRESENT_LOG("scheduleSwapchainImage: _director->acquireFrame");
		if (_view->getDirector()->acquireFrame(frame->getRequest())) {
			XL_VKPRESENT_LOG("scheduleSwapchainImage: frame acquired");
			_view->performOnThread([this, frame = move(frame), cb = sp::move(cb)] () mutable {
				cb(frame);
			}, this);
		}
	}, this);
}

bool PresentationEngine::isImagePresentable(const core::ImageObject &image, VkFilter &filter) const {
	auto dev = static_cast<Device *>(_device);

	auto &config = _swapchain->getConfig();

	auto &sourceImageInfo = image.getInfo();
	if (sourceImageInfo.extent.depth != 1 || sourceImageInfo.format != config.imageFormat
			|| (sourceImageInfo.usage & core::ImageUsage::TransferSrc) == core::ImageUsage::None) {
		log::error("Swapchain", "Image can not be presented on swapchain");
		return false;
	}

	VkFormatProperties sourceProps;
	VkFormatProperties targetProps;

	dev->getInstance()->vkGetPhysicalDeviceFormatProperties(dev->getInfo().device,
			VkFormat(sourceImageInfo.format), &sourceProps);
	dev->getInstance()->vkGetPhysicalDeviceFormatProperties(dev->getInfo().device,
			VkFormat(config.imageFormat), &targetProps);

	if (config.extent.width == sourceImageInfo.extent.width && config.extent.height == sourceImageInfo.extent.height) {
		if ((targetProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) == 0) {
			return false;
		}

		if (sourceImageInfo.tiling == core::ImageTiling::Optimal) {
			if ((sourceProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0) {
				return false;
			}
		} else {
			if ((sourceProps.linearTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0) {
				return false;
			}
		}
	} else {
		if ((targetProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) == 0) {
			return false;
		}

		if (sourceImageInfo.tiling == core::ImageTiling::Optimal) {
			if ((sourceProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) == 0) {
				return false;
			}

			if ((sourceProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0) {
				filter = VK_FILTER_LINEAR;
			}
		} else {
			if ((sourceProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) == 0) {
				return false;
			}

			if ((sourceProps.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0) {
				filter = VK_FILTER_LINEAR;
			}
		}
	}

	return true;
}

}
