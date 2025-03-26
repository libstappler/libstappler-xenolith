/**
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

#include "XLVkGuiViewImpl.h"

#if WIN32

#include "XLTextInputManager.h"
#include "XLVkPlatform.h"
#include "XLVkPresentationEngine.h"

#define XL_WIN32_DEBUG 1

#ifndef XL_WIN32_LOG
#if XL_WIN32_DEBUG
#define XL_WIN32_LOG(...) log::debug("Win32", __VA_ARGS__)
#else
#define XL_WIN32_LOG(...)
#endif
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

struct InstanceSurfaceData : Ref {
	virtual ~InstanceSurfaceData() { }

	Rc<xenolith::platform::Win32Library> win32;
};

VkSurfaceKHR WinView_createSurface(vk::Instance *instance, xenolith::platform::Win32View *view) {
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (view->getInstance() && view->getWindow()) {
		VkWin32SurfaceCreateInfoKHR createInfo {
				VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR, nullptr, 0,
				view->getInstance(), view->getWindow() };
		instance->vkCreateWin32SurfaceKHR(instance->getInstance(), &createInfo,
				nullptr, &surface);
	}
	return surface;
}

ViewImpl::ViewImpl() { }

ViewImpl::~ViewImpl() {
	_view = nullptr;
}

bool ViewImpl::init(Application &loop, const core::Device &dev, ViewInfo &&info) {
	if (!vk::View::init(loop, static_cast<const vk::Device &>(dev), move(info))) {
		return false;
	}

	auto data = (InstanceSurfaceData *)_instance->getUserdata();

	_view = Rc<xenolith::platform::Win32View>::create(this, data->win32, xenolith::platform::Win32ViewInfo{
		_info.window.bundleId, _info.window.title, _info.window.rect,
		[] (ViewInterface *view) {
			((ViewImpl *)view)->captureWindow();
		}, [] (ViewInterface *view) {
			((ViewImpl *)view)->releaseWindow();
		}, [] (ViewInterface *view) {
			((ViewImpl *)view)->waitUntilFrame();
		}
	});

	if (!_view) {
		return false;
	}

	auto surface = Rc<vk::Surface>::create(_instance, WinView_createSurface(_instance, _view), _view);
	if (!surface) {
		return false;
	}

	auto engine = Rc<PresentationEngine>::create(_device, this, move(surface),
			_view->exportConstraints(_info.exportConstraints()), _view->getScreenFrameInterval());
	if (engine) {
		setPresentationEngine(move(engine));
		return true;
	}

	log::error("ViewImpl", "Fail to initalize PresentationEngine");
	return false;
}

void ViewImpl::run() {
	View::run();

	_view->getWin32()->runPoll();
}

void ViewImpl::end() {
	_view->getWin32()->stopPoll();
	_view = nullptr;

	View::end();
}

void ViewImpl::updateTextCursor(uint32_t pos, uint32_t len) {

}

void ViewImpl::updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) {

}

void ViewImpl::runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) {
	performOnThread([this] {
		_inputEnabled = true;
		_mainLoop->performOnAppThread([&] () {
			_director->getTextInputManager()->setInputEnabled(true);
		}, this);
	}, this);
}

void ViewImpl::cancelTextInput() {
	performOnThread([this] {
		_inputEnabled = false;
		_mainLoop->performOnAppThread([&] () {
			_director->getTextInputManager()->setInputEnabled(false);
		}, this);
	}, this);
}

void ViewImpl::captureWindow() {
	/*_tmpOptions = _options;
	_options.presentImmediate = true;
	_options.acquireImageImmediately = true;
	_options.renderOnDemand = true;
	_readyForNextFrame = false;*/
	std::unique_lock lock(_captureMutex);
	_windowCaptured = true;
}

void ViewImpl::releaseWindow() {
	//_options = _tmpOptions;
	std::unique_lock lock(_captureMutex);
	_windowCaptured = false;
}

void ViewImpl::readFromClipboard(Function<void(BytesView, StringView)> &&cb, Ref *ref) {
	performOnThread([this, cb = sp::move(cb), ref = Rc<Ref>(ref)] () mutable {
		_view->readFromClipboard([this, cb = sp::move(cb)] (BytesView view, StringView ct) mutable {
			_mainLoop->performOnAppThread([cb = sp::move(cb), view = view.bytes<Interface>(), ct = ct.str<Interface>()] () {
				cb(view, ct);
			}, this);
		}, ref);
	}, this, true);
}

void ViewImpl::writeToClipboard(BytesView data, StringView contentType) {
	performOnThread([this, data = data.bytes<Interface>(), contentType = contentType.str<Interface>()] {
		_view->writeToClipboard(data, contentType);
	}, this, true);
}

void ViewImpl::mapWindow() {
	if (_view) {
		_view->mapWindow();
	}
	View::mapWindow();
}

Rc<vk::View> createView(Application &loop, const core::Device &dev, ViewInfo &&info) {
	return Rc<ViewImpl>::create(loop, dev, move(info));
}

uint32_t checkPresentationSupport(const vk::Instance *instance, VkPhysicalDevice device, uint32_t queueIdx) {
	if (instance->vkGetPhysicalDeviceWin32PresentationSupportKHR(device, queueIdx)) {
		return 1;
	}
	return 0;
}

bool initInstance(VulkanInstanceData &data, const VulkanInstanceInfo &info) {
	auto instanceData = Rc<InstanceSurfaceData>::alloc();
	instanceData->win32 = Rc<xenolith::platform::Win32Library>::create();

	const char *surfaceExt = nullptr;
	const char *winSurfaceExt = nullptr;
	for (auto &extension : info.availableExtensions) {
		if (strcmp(VK_KHR_SURFACE_EXTENSION_NAME, extension.extensionName) == 0) {
			surfaceExt = extension.extensionName;
			data.extensionsToEnable.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		} else if (strcmp(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, extension.extensionName) == 0) {
			winSurfaceExt = extension.extensionName;
			data.extensionsToEnable.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
		}
	}

	if (surfaceExt && winSurfaceExt) {
		data.checkPresentationSupport = checkPresentationSupport;
		data.userdata = instanceData;
		return true;
	}

	return false;
}

}

#endif
