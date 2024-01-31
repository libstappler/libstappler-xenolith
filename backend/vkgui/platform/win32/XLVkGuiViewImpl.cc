/**
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

#include "XLVkGuiViewImpl.h"

#if WIN32

#include "XLTextInputManager.h"
#include "XLVkPlatform.h"

#include <chrono>

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

	//_options.flattenFrameRate = true;
	//_options.renderOnDemand = false;

	return true;
}

void ViewImpl::threadInit() {
	thread::ThreadInfo::setThreadInfo(_threadName);
	_threadId = std::this_thread::get_id();

	auto data = (InstanceSurfaceData *)_instance->getUserdata();
	_view = Rc<xenolith::platform::Win32View>::create(this, data->win32, xenolith::platform::Win32ViewInfo{
		_info.bundleId, _info.name, _info.rect,
		[] (ViewInterface *view) {
			((ViewImpl *)view)->captureWindow();
		}, [] (ViewInterface *view) {
			((ViewImpl *)view)->releaseWindow();
		}, [] (ViewInterface *view) {
			((ViewImpl *)view)->handlePaint();
		}
	});
	if (_view) {
		if (auto surface = WinView_createSurface(_instance, _view)) {
			_surface = Rc<Surface>::create(_instance, surface, _view);
		}
		setFrameInterval(_view->getScreenFrameInterval());
	}

	View::threadInit();
}

void ViewImpl::threadDispose() {
	View::threadDispose();

	_surface = nullptr;
	_view = nullptr;
}

bool ViewImpl::worker() {
	MSG msg = { };

	while (_shouldQuit.test_and_set()) {
		if (_view->shouldUpdate()) {
			update(false);
		}

		auto ret = PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE);
		if (ret) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (_view->shouldUpdate()) {
				update(false);
			}
			if (_view->shouldQuit()) {
				_view->dispose();
				return false;
			}
		} else {
			if (MsgWaitForMultipleObjects(0, NULL, FALSE, 1, QS_ALLEVENTS) == WAIT_TIMEOUT) {
				update(false);
			}
			//WaitMessage();
		}
	}

	_view->dispose();
	PostQuitMessage(0);
	return false;
}

void ViewImpl::wakeup(std::unique_lock<Mutex> &lock) {
	lock.unlock();
	std::unique_lock lock2(_captureMutex);
	if (_windowCaptured) {
		_captureCondVar.notify_all();
	} else {
		_view->wakeup();
	}
}

void ViewImpl::updateTextCursor(uint32_t pos, uint32_t len) {

}

void ViewImpl::updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) {

}

void ViewImpl::runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) {
	performOnThread([this] {
		_inputEnabled = true;
		_mainLoop->performOnMainThread([&] () {
			_director->getTextInputManager()->setInputEnabled(true);
		}, this);
	}, this);
}

void ViewImpl::cancelTextInput() {
	performOnThread([this] {
		_inputEnabled = false;
		_mainLoop->performOnMainThread([&] () {
			_director->getTextInputManager()->setInputEnabled(false);
		}, this);
	}, this);
}

void ViewImpl::presentWithQueue(vk::DeviceQueue &queue, Rc<ImageStorage> &&image) {
	vk::View::presentWithQueue(queue, move(image));
}

void ViewImpl::captureWindow() {
	_tmpOptions = _options;
	_options.presentImmediate = true;
	_options.acquireImageImmediately = true;
	_options.renderOnDemand = true;
	_readyForNextFrame = false;
	std::unique_lock lock(_captureMutex);
	_windowCaptured = true;
}

void ViewImpl::releaseWindow() {
	_options = _tmpOptions;
	std::unique_lock lock(_captureMutex);
	_windowCaptured = false;
}

void ViewImpl::handlePaint() {
	// draw frame in blocking mode when resizeing

	std::unique_lock lock(_captureMutex);
	// wait until swaphain recreation
	if (_swapchain->isDeprecated()) {
		update(false);
		while (_swapchain->isDeprecated()) {
			auto status = _captureCondVar.wait_for(lock, std::chrono::milliseconds(32));
			if (status == std::cv_status::timeout) {
				return;
			}
			update(false);
		}
	}

	auto c = _swapchain->getPresentedFramesCount();
	update(false);
	while (c == _swapchain->getPresentedFramesCount()) {
		auto status = _captureCondVar.wait_for(lock, std::chrono::milliseconds(32));
		if (status == std::cv_status::timeout) {
			return;
		}
		update(false);
	}
}

bool ViewImpl::pollInput(bool frameReady) {
	/*MSG msg = { };
	while (PeekMessageW(&msg, _view->getWindow(), 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}*/
	if (_view->shouldQuit()) {
		_shouldQuit.clear();
		return false;
	}
	return true;
}

void ViewImpl::mapWindow() {
	if (_view) {
		_view->mapWindow();
	}
	View::mapWindow();
}

void ViewImpl::finalize() {
	_view = nullptr;
	View::finalize();
}

void ViewImpl::schedulePresent(SwapchainImage *img, uint64_t t) {
	View::schedulePresent(img, t);
	if (_view) {
		_view->schedule(t);
	}
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
