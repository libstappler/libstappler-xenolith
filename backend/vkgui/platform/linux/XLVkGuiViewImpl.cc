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

#if LINUX

#include "linux/XLPlatformLinuxWaylandView.h"
#include "linux/XLPlatformLinuxXcbView.h"
#include "XLTextInputManager.h"
#include "XLVkPlatform.h"
#include "SPPlatformUnistd.h"

#include <sys/eventfd.h>
#include <poll.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

ViewImpl::ViewImpl() { }

ViewImpl::~ViewImpl() {
	_view = nullptr;
}

bool ViewImpl::init(Application &loop, const core::Device &dev, ViewInfo &&info) {
	_constraints.density = info.density;

	if (!vk::View::init(loop, static_cast<const vk::Device &>(dev), move(info))) {
		return false;
	}

	return true;
}

#if XL_ENABLE_WAYLAND
static VkSurfaceKHR createWindowSurface(xenolith::platform::WaylandView *v, vk::Instance *instance, VkPhysicalDevice dev) {
	auto display = v->getDisplay();
	auto surface = v->getSurface();

	std::cout << "Create wayland surface for " << (void *)dev << " on " << (void *)display->display << "\n";
	auto supports = instance->vkGetPhysicalDeviceWaylandPresentationSupportKHR(dev, 0, display->display);
	if (!supports) {
		return nullptr;
	}

	VkSurfaceKHR ret;
	VkWaylandSurfaceCreateInfoKHR info{
		VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
		nullptr,
		0,
		display->display,
		surface
	};

	if (instance->vkCreateWaylandSurfaceKHR(instance->getInstance(), &info, nullptr, &ret) == VK_SUCCESS) {
		VkBool32 pSupported = 0;
		instance->vkGetPhysicalDeviceSurfaceSupportKHR(dev, 0, ret, &pSupported);
		if (!pSupported) {
			instance->vkDestroySurfaceKHR(instance->getInstance(), ret, nullptr);
			return nullptr;
		}
		return ret;
	}
	return nullptr;
}
#endif

static VkSurfaceKHR createWindowSurface(xenolith::platform::XcbView *v, vk::Instance *instance, VkPhysicalDevice dev) {
	auto connection = v->getConnection();
	auto window = v->getWindow();

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkXcbSurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR, nullptr, 0, connection, window};
	if (instance->vkCreateXcbSurfaceKHR(instance->getInstance(), &createInfo, nullptr, &surface) != VK_SUCCESS) {
		return nullptr;
	}
	return surface;
}

void ViewImpl::threadInit() {
	thread::ThreadInfo::setThreadInfo(_threadName);
	_threadId = std::this_thread::get_id();

	auto presentMask = _device->getPresentatonMask();

#if XL_ENABLE_WAYLAND
	if (auto wayland = xenolith::platform::WaylandLibrary::getInstance()) {
		if ((platform::SurfaceType(presentMask) & platform::SurfaceType::Wayland) != platform::SurfaceType::None) {
			auto waylandDisplay = getenv("WAYLAND_DISPLAY");
			auto sessionType = getenv("XDG_SESSION_TYPE");

			if (waylandDisplay || strcasecmp("wayland", sessionType) == 0) {
				auto view = Rc<xenolith::platform::WaylandView>::alloc(wayland, this, _info.name, _info.bundleId, _info.rect);
				if (!view) {
					log::error("VkView", "Fail to initialize wayland window");
					return;
				}

				_view = view;
				_surface = Rc<vk::Surface>::create(_instance,
						createWindowSurface(view, _instance, _device->getPhysicalDevice()), _view);
				if (_surface) {
					setFrameInterval(_view->getScreenFrameInterval());
				} else {
					_view = nullptr;
				}
			}
		}
	}
#endif

	if (!_view) {
		// try X11
		if (auto xcb = xenolith::platform::XcbLibrary::getInstance()) {
			if ((platform::SurfaceType(presentMask) & platform::SurfaceType::XCB) != platform::SurfaceType::None) {
				auto view = Rc<xenolith::platform::XcbView>::alloc(xcb, this, _info.name, _info.bundleId, _info.rect);
				if (!view) {
					log::error("VkView", "Fail to initialize xcb window");
					return;
				}

				_view = view;
				_surface = Rc<vk::Surface>::create(_instance,
						createWindowSurface(view, _instance, _device->getPhysicalDevice()), _view);
				setFrameInterval(_view->getScreenFrameInterval());
			}
		}
	}

	if (!_view) {
		log::error("View", "No available surface type");
	}

	View::threadInit();
}

void ViewImpl::threadDispose() {
	View::threadDispose();
}

bool ViewImpl::worker() {
	_eventFd = eventfd(0, EFD_NONBLOCK);
	auto socket = _view->getSocketFd();

	timespec timeoutMin;
	timeoutMin.tv_sec = 0;
	timeoutMin.tv_nsec = 1000 * 250;

	struct pollfd fds[2];

	fds[0].fd = _eventFd;
	fds[0].events = POLLIN;
	fds[0].revents = 0;

	fds[1].fd = socket;
	fds[1].events = POLLIN;
	fds[1].revents = 0;

	update(false);

	while (_shouldQuit.test_and_set()) {
		bool shouldUpdate = false;

		int ret = ::ppoll( fds, 2, &timeoutMin, nullptr);
		if (ret > 0) {
			if (fds[0].revents != 0) {
				uint64_t value = 0;
				auto sz = ::read(_eventFd, &value, sizeof(uint64_t));
				if (sz == 8 && value) {
					shouldUpdate = true;
				}
				fds[0].revents = 0;
			}
			if (fds[1].revents != 0) {
				fds[1].revents = 0;
				if (!_view->poll(false)) {
					break;
				}
			}
		} else if (ret == 0) {
			shouldUpdate = true;
		} else if (errno != EINTR) {
			// ignore EINTR to allow debugging
			break;
		}

		if (shouldUpdate) {
			update(false);
		}
	}

	if (_swapchain) {
		_swapchain->deprecate(false);
	}

	::close(_eventFd);
	return false;
}

void ViewImpl::wakeup(std::unique_lock<Mutex> &) {
	if (_eventFd >= 0) {
		uint64_t value = 1;
		::write(_eventFd, (const void *)&value, sizeof(uint64_t));
	}
}

void ViewImpl::updateTextCursor(uint32_t pos, uint32_t len) {

}

void ViewImpl::updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) {

}

void ViewImpl::runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) {
	performOnThread([this] {
		_inputEnabled = true;
		_mainLoop->performOnMainThread([this] () {
			_director->getTextInputManager()->setInputEnabled(true);
		}, this);
	}, this);
}

void ViewImpl::cancelTextInput() {
	performOnThread([this] {
		_inputEnabled = false;
		_mainLoop->performOnMainThread([this] () {
			_director->getTextInputManager()->setInputEnabled(false);
		}, this);
	}, this);
}

void ViewImpl::presentWithQueue(vk::DeviceQueue &queue, Rc<ImageStorage> &&image) {
	auto &e = _swapchain->getImageInfo().extent;
	_view->commit(e.width, e.height);
	vk::View::presentWithQueue(queue, move(image));
}

bool ViewImpl::pollInput(bool frameReady) {
	if (!_view->poll(frameReady)) {
		close();
		return false;
	}
	return true;
}

core::SurfaceInfo ViewImpl::getSurfaceOptions() const {
	core::SurfaceInfo ret = vk::View::getSurfaceOptions();
	_view->onSurfaceInfo(ret);
	return ret;
}

void ViewImpl::mapWindow() {
	if (_view) {
		_view->mapWindow();
	}
	View::mapWindow();
}

void ViewImpl::readFromClipboard(Function<void(BytesView, StringView)> &&cb, Ref *ref) {
	performOnThread([this, cb = move(cb), ref = Rc<Ref>(ref)] () mutable {
		_view->readFromClipboard([this, cb = move(cb)] (BytesView view, StringView ct) mutable {
			_mainLoop->performOnMainThread([cb = move(cb), view = view.bytes<Interface>(), ct = ct.str<Interface>()] () {
				cb(view, ct);
			}, this);
		}, ref);
	}, this);
}

void ViewImpl::writeToClipboard(BytesView data, StringView contentType) {
	performOnThread([this, data = data.bytes<Interface>(), contentType = contentType.str<Interface>()] {
		_view->writeToClipboard(data, contentType);
	}, this);
}

void ViewImpl::finalize() {
	_view = nullptr;
	View::finalize();
}

Rc<vk::View> createView(Application &loop, const core::Device &dev, ViewInfo &&info) {
	return Rc<ViewImpl>::create(loop, dev, move(info));
}

struct InstanceSurfaceData : Ref {
	virtual ~InstanceSurfaceData() { }

	SurfaceType surfaceType = SurfaceType::None;
#if XL_ENABLE_WAYLAND
	Rc<xenolith::platform::WaylandLibrary> wayland;
#endif
	Rc<xenolith::platform::XcbLibrary> xcb;
};

uint32_t checkPresentationSupport(const vk::Instance *instance, VkPhysicalDevice device, uint32_t queueIdx) {
	InstanceSurfaceData *instanceData = (InstanceSurfaceData *)instance->getUserdata();

	uint32_t ret = 0;
	if ((instanceData->surfaceType & SurfaceType::Wayland) != SurfaceType::None) {
#if XL_ENABLE_WAYLAND
		auto display = xenolith::platform::WaylandLibrary::getInstance()->getActiveConnection().display;
		std::cout << "Check if " << (void *)device << " [" << queueIdx << "] supports wayland on " << (void *)display << ": ";
		auto supports = instance->vkGetPhysicalDeviceWaylandPresentationSupportKHR(device, queueIdx, display);
		if (supports) {
			ret |= toInt(SurfaceType::Wayland);
			std::cout << "yes\n";
		} else {
			std::cout << "no\n";
		}
#endif
	}
	if ((instanceData->surfaceType & SurfaceType::XCB) != SurfaceType::None) {
		auto conn = xenolith::platform::XcbLibrary::getInstance()->getActiveConnection();
		auto supports = instance->vkGetPhysicalDeviceXcbPresentationSupportKHR(device, queueIdx, conn.connection, conn.screen->root_visual);
		if (supports) {
			ret |= toInt(SurfaceType::XCB);
		}
	}
	return ret;
}

bool initInstance(vk::platform::VulkanInstanceData &data, const vk::platform::VulkanInstanceInfo &info) {
	auto instanceData = Rc<InstanceSurfaceData>::alloc();

	SurfaceType osSurfaceType = SurfaceType::None;
	instanceData->xcb = Rc<xenolith::platform::XcbLibrary>::create();
	if (instanceData->xcb) {
		osSurfaceType |= SurfaceType::XCB;
	}

#if XL_ENABLE_WAYLAND
	instanceData->wayland = Rc<xenolith::platform::WaylandLibrary>::create();
	if (instanceData->wayland) {
		osSurfaceType |= SurfaceType::Wayland;
	}
#endif

	const char *surfaceExt = nullptr;
	for (auto &extension : info.availableExtensions) {
		if (strcmp(VK_KHR_SURFACE_EXTENSION_NAME, extension.extensionName) == 0) {
			surfaceExt = extension.extensionName;
			data.extensionsToEnable.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		} else if (strcmp(VK_KHR_XCB_SURFACE_EXTENSION_NAME, extension.extensionName) == 0
				&& (osSurfaceType & SurfaceType::XCB) != SurfaceType::None) {
			instanceData->surfaceType |= SurfaceType::XCB;
			data.extensionsToEnable.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
		} else if (strcmp(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME, extension.extensionName) == 0
				&& (osSurfaceType & SurfaceType::Wayland) != SurfaceType::None) {
			instanceData->surfaceType |= SurfaceType::Wayland;
			data.extensionsToEnable.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
		}
	}

	if (instanceData->surfaceType != SurfaceType::None && surfaceExt) {
		data.checkPresentationSupport = checkPresentationSupport;
		data.userdata = instanceData;
		return true;
	}

	return false;
}

}

#endif
