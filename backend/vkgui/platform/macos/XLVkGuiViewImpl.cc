 /**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "XLVkGuiViewImpl.h"

#if MACOS

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

bool ViewImpl::init(Application &loop, const core::Device &dev, ViewInfo &&info) {
	if (!View::init(loop, static_cast<const vk::Device &>(dev), move(info))) {
		return false;
	}

	_options.followDisplayLink = true;

	loop.performOnMainThread([this] {
		_viewController = xenolith::platform::MacViewController::makeConroller(this);
		_viewController->setTitle(_info.title.empty() ? _info.bundleId : _info.title);

		_viewController->setDisplayLinkCallback([this] {
			if (!_options.followDisplayLink) {
				return;
			}

			_displayLinkFlag.clear();
			_viewController->wakeup();
		});
	}, this);

	return true;
}

void ViewImpl::run() {
	_mainLoop->performOnMainThread([this] {
		threadInit();
	}, this);
}

void ViewImpl::threadInit() {
	auto instance = _instance.cast<vk::Instance>();

	VkSurfaceKHR targetSurface;
	VkMetalSurfaceCreateInfoEXT surfaceCreateInfo;
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
	surfaceCreateInfo.pNext = nullptr;
	surfaceCreateInfo.flags = 0;
	surfaceCreateInfo.pLayer = _viewController->getLayer();

	if (instance->vkCreateMetalSurfaceEXT(instance->getInstance(), &surfaceCreateInfo, nullptr, &targetSurface) != VK_SUCCESS) {
		log::error("ViewImpl", "fail to create surface");
		return;
	}

	_surface = Rc<vk::Surface>::create(instance, targetSurface);

	vk::View::threadInit();
}

void ViewImpl::threadDispose() {
	clearImages();

	_running = false;
	_swapchain = nullptr;
	_surface = nullptr;

	std::unique_lock<Mutex> lock(_mutex);
	_callbacks.clear();

	release(_refId);
}

bool ViewImpl::worker() {
	return false;
}

void ViewImpl::update(bool displayLink) {
	View::update(!_displayLinkFlag.test_and_set());
}

void ViewImpl::end() {
	auto id = retain();
	threadDispose();
	View::end();
	release(id);
}

void ViewImpl::updateTextCursor(uint32_t pos, uint32_t len) {
	/*performOnThread([this, pos, len] {
		ViewImpl_updateTextCursor(_osView, pos, len);
		_director->getApplication()->performOnMainThread([this] () {
			_director->getTextInputManager()->setInputEnabled(true);
		}, this);
	}, this);*/
}

void ViewImpl::updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType type) {
	/*performOnThread([this, str = str.str<Interface>(), pos, len, type] {
		ViewImpl_updateTextInput(_osView, str, pos, len, type);
		_director->getApplication()->performOnMainThread([this] () {
			_director->getTextInputManager()->setInputEnabled(true);
		}, this);
	}, this);*/
}

void ViewImpl::runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType type) {
	/*performOnThread([this, str = str.str<Interface>(), pos, len, type] {
		_inputEnabled = true;
		ViewImpl_runTextInput(_osView, str, pos, len, type);
		_director->getApplication()->performOnMainThread([this] () {
			_director->getTextInputManager()->setInputEnabled(true);
		}, this);
	}, this);*/
}

void ViewImpl::cancelTextInput() {
	/*performOnThread([this] {
		_inputEnabled = false;
		ViewImpl_cancelTextInput(_osView);
		_director->getApplication()->performOnMainThread([&] () {
			_director->getTextInputManager()->setInputEnabled(false);
		}, this);
	}, this);*/
}

bool ViewImpl::isInputEnabled() const {
	return false;
}

void ViewImpl::linkWithNativeWindow(void *) { }
void ViewImpl::stopNativeWindow() { }

void ViewImpl::readFromClipboard(Function<void(BytesView, StringView)> &&, Ref *) { }
void ViewImpl::writeToClipboard(BytesView, StringView contentType) { }

void ViewImpl::mapWindow() {
	_mainLoop->performOnMainThread([this] {
		_viewController->mapWindow();
	}, this);
}

void ViewImpl::wakeup(std::unique_lock<Mutex> &) {
	_viewController->wakeup();
}

void ViewImpl::startLiveResize() {
	/*_liveResize = true;
	_loop->performOnGlThread([this] {
		_loop->getFrameCache()->freeze();
	}, this);
	_options.renderImageOffscreen = true;
	deprecateSwapchain(false);*/
	/*_swapchain->deprecate(false);

	auto it = _scheduledPresent.begin();
	while (it != _scheduledPresent.end()) {
		runScheduledPresent(move(*it));
		it = _scheduledPresent.erase(it);
	}

	// log::vtext("View", "recreateSwapchain - View::deprecateSwapchain (", renderqueue::FrameHandle::GetActiveFramesCount(), ")");
	recreateSwapchain(_swapchain->getRebuildMode());*/
}

void ViewImpl::stopLiveResize() {
	/*_options.renderImageOffscreen = false;
	deprecateSwapchain(false);

	_loop->performOnGlThread([this] {
		_loop->getFrameCache()->unfreeze();
	}, this);
	_liveResize = false;*/
}

void ViewImpl::submitTextData(WideStringView str, TextCursor cursor, TextCursor marked) {
	/*_director->getApplication()->performOnMainThread([this, str = str.str<Interface>(), cursor, marked] {
		_director->getTextInputManager()->textChanged(str, cursor, marked);
	}, this);*/
}

bool ViewImpl::pollInput(bool frameReady) {
	return true; // we should return false when view should be closed
}

bool ViewImpl::createSwapchain(const core::SurfaceInfo &info, core::SwapchainConfig &&cfg, core::PresentMode presentMode) {
	if (presentMode == core::PresentMode::Immediate) {
		_options.followDisplayLink = false;
		_viewController->setVSyncEnabled(false);
	} else {
		_options.followDisplayLink = true;
		_viewController->setVSyncEnabled(true);
	}

	auto ret = vk::View::createSwapchain(info, move(cfg), presentMode);
	if (ret) {
		_constraints.density = _viewController->getLayerDensity();
	}
	return ret;
}

Rc<vk::View> createView(Application &app, const core::Device &dev, ViewInfo &&info) {
	return Rc<ViewImpl>::create(app, dev, move(info));
}

bool initInstance(vk::platform::VulkanInstanceData &data, const vk::platform::VulkanInstanceInfo &info) {
	const char *surfaceExt = nullptr;
	const char *androidExt = nullptr;
	for (auto &extension : info.availableExtensions) {
		if (strcmp(VK_KHR_SURFACE_EXTENSION_NAME, extension.extensionName) == 0) {
			surfaceExt = extension.extensionName;
			data.extensionsToEnable.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		} else if (strcmp(VK_EXT_METAL_SURFACE_EXTENSION_NAME, extension.extensionName) == 0) {
			androidExt = extension.extensionName;
			data.extensionsToEnable.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
		}
	}

	if (surfaceExt && androidExt) {
		return true;
	}

	return false;
}

uint32_t checkPresentationSupport(const vk::Instance *instance, VkPhysicalDevice device, uint32_t queueIdx) {
	return 1;
}

}

#endif
