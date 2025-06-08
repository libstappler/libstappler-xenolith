/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#include "XLVkGuiViewImpl.h"
#include "XLCoreInput.h"
#include "XLVk.h"

#if LINUX

#include "linux/XLPlatformLinuxWaylandView.h"
#include "linux/XLPlatformLinuxXcbView.h"
#include "linux/XLPlatformLinuxXcbConnection.h"
#include "XLVkPresentationEngine.h"
#include "XLVkPlatform.h"

#include "platform/fd/SPEventPollFd.h"


namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

#if XL_ENABLE_WAYLAND
static VkSurfaceKHR createWindowSurface(xenolith::platform::WaylandView *v, vk::Instance *instance,
		VkPhysicalDevice dev) {
	auto display = v->getDisplay();
	auto surface = v->getSurface();

	std::cout << "Create wayland surface for " << (void *)dev << " on " << (void *)display->display
			  << "\n";
	auto supports =
			instance->vkGetPhysicalDeviceWaylandPresentationSupportKHR(dev, 0, display->display);
	if (!supports) {
		return nullptr;
	}

	VkSurfaceKHR ret;
	VkWaylandSurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR, nullptr,
		0, display->display, surface};

	if (instance->vkCreateWaylandSurfaceKHR(instance->getInstance(), &info, nullptr, &ret)
			== VK_SUCCESS) {
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

static VkSurfaceKHR createWindowSurface(xenolith::platform::XcbView *v, vk::Instance *instance,
		VkPhysicalDevice dev) {
	auto connection = v->getConnection();
	auto window = v->getWindow();

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkXcbSurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR, nullptr, 0,
		connection, window};
	if (instance->vkCreateXcbSurfaceKHR(instance->getInstance(), &createInfo, nullptr, &surface)
			!= VK_SUCCESS) {
		return nullptr;
	}
	return surface;
}

ViewImpl::~ViewImpl() { _view = nullptr; }

bool ViewImpl::init(Application &loop, const core::Device &dev, ViewInfo &&info) {
	if (!vk::View::init(loop, static_cast<const vk::Device &>(dev), move(info))) {
		return false;
	}

	Rc<Surface> surface;
	auto presentMask = _device->getPresentatonMask();

#if XL_ENABLE_WAYLAND
	if (auto wayland = xenolith::platform::WaylandLibrary::getInstance()) {
		if ((platform::SurfaceType(presentMask) & platform::SurfaceType::Wayland)
				!= platform::SurfaceType::None) {
			auto waylandDisplay = getenv("WAYLAND_DISPLAY");
			auto sessionType = getenv("XDG_SESSION_TYPE");

			if (waylandDisplay || strcasecmp("wayland", sessionType) == 0) {
				auto view = Rc<xenolith::platform::WaylandView>::alloc(wayland, this, _info.name,
						_info.bundleId, _info.rect);
				if (view) {
					_view = view;
					_surface = Rc<vk::Surface>::create(_instance,
							createWindowSurface(view, _instance, _device->getPhysicalDevice()),
							_view);
					if (_surface) {
						setFrameInterval(_view->getScreenFrameInterval());
					} else {
						_view = nullptr;
					}
				} else {
					log::error("ViewImpl", "Fail to initialize wayland window, try X11");
				}
			}
		}
	}
#endif

	if (!_view) {
		// try X11
		if (auto xcb = xenolith::platform::XcbLibrary::getInstance()) {
			if ((platform::SurfaceType(presentMask) & platform::SurfaceType::XCB)
					!= platform::SurfaceType::None) {
				auto view = Rc<xenolith::platform::XcbView>::alloc(xcb->acquireConnection(), this,
						_info.window);
				if (!view) {
					log::error("ViewImpl", "Fail to initialize xcb window");
					return false;
				}

				surface = Rc<vk::Surface>::create(_instance,
						createWindowSurface(view, _instance, _device->getPhysicalDevice()), view);
				_view = move(view);
			}
		}
	}

	if (!_view) {
		log::error("ViewImpl", "No available surface type");
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

	_pollHandle = event::PollFdHandle::create(_loop->getLooper()->getQueue(), _view->getSocketFd(),
			event::PollFlags::In, [this](int fd, event::PollFlags flags) {
		if (hasFlag(flags, event::PollFlags::In)) {
			if (!_view->poll(false)) {
				end();
			}
		}
		return Status::Ok;
	}, this);
	_loop->getLooper()->performHandle(_pollHandle);
}

void ViewImpl::end() {
	_pollHandle->cancel();
	_pollHandle = nullptr;

	_view = nullptr;

	View::end();
}

void ViewImpl::mapWindow() {
	if (_view) {
		_view->mapWindow();
	}
	View::mapWindow();
}

void ViewImpl::readFromClipboard(Function<void(BytesView, StringView)> &&cb, Ref *ref) {
	performOnThread([this, cb = sp::move(cb), ref = Rc<Ref>(ref)]() mutable {
		_view->readFromClipboard([this, cb = sp::move(cb)](BytesView view, StringView ct) mutable {
			_application->performOnAppThread([cb = sp::move(cb), view = view.bytes<Interface>(),
													 ct = ct.str<Interface>()]() { cb(view, ct); },
					this);
		}, ref);
	}, this);
}

void ViewImpl::writeToClipboard(BytesView data, StringView contentType) {
	performOnThread(
			[this, data = data.bytes<Interface>(), contentType = contentType.str<Interface>()] {
		_view->writeToClipboard(data, contentType);
	}, this);
}

void ViewImpl::handleFramePresented(core::PresentationFrame *frame) {
	if (_view) {
		_view->handleFramePresented();
	}
}

core::SurfaceInfo ViewImpl::getSurfaceOptions(core::SurfaceInfo &&info) const {
	_view->onSurfaceInfo(info);
	return move(info);
}

bool ViewImpl::updateTextInput(const TextInputRequest &, TextInputFlags flags) {
	if (!_inputEnabled && hasFlag(flags, TextInputFlags::RunIfDisabled)) {
		_inputEnabled = true;
		_textInput->handleInputEnabled(true);
	}
	return true;
}

void ViewImpl::cancelTextInput() {
	_inputEnabled = false;
	_textInput->handleInputEnabled(false);
}

void ViewImpl::handleLayerUpdate(const ViewLayer &layer) {
	View::handleLayerUpdate(layer);
	if (_view) {
		_view->handleLayerUpdate(layer);
	}
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

uint32_t checkPresentationSupport(const vk::Instance *instance, VkPhysicalDevice device,
		uint32_t queueIdx) {
	InstanceSurfaceData *instanceData = (InstanceSurfaceData *)instance->getUserdata();

	uint32_t ret = 0;
	if ((instanceData->surfaceType & SurfaceType::Wayland) != SurfaceType::None) {
#if XL_ENABLE_WAYLAND
		auto display =
				xenolith::platform::WaylandLibrary::getInstance()->getActiveConnection().display;
		std::cout << "Check if " << (void *)device << " [" << queueIdx << "] supports wayland on "
				  << (void *)display << ": ";
		auto supports = instance->vkGetPhysicalDeviceWaylandPresentationSupportKHR(device, queueIdx,
				display);
		if (supports) {
			ret |= toInt(SurfaceType::Wayland);
			std::cout << "yes\n";
		} else {
			std::cout << "no\n";
		}
#endif
	}
	if ((instanceData->surfaceType & SurfaceType::XCB) != SurfaceType::None) {
		auto conn = xenolith::platform::XcbLibrary::getInstance()->getCommonConnection();
		auto supports = instance->vkGetPhysicalDeviceXcbPresentationSupportKHR(device, queueIdx,
				conn->getConnection(), conn->getDefaultScreen()->root_visual);
		if (supports) {
			ret |= toInt(SurfaceType::XCB);
		}
	}
	return ret;
}

bool initInstance(vk::platform::VulkanInstanceData &data,
		const vk::platform::VulkanInstanceInfo &info) {
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

} // namespace stappler::xenolith::vk::platform

#endif
