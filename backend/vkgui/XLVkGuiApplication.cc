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

#include "XLVkLoop.h"
#include "XLVkGuiPlatform.h"
#include "platform/linux/XLVkGuiViewImpl.h"
#include "XLVkGuiApplication.h"

namespace stappler::xenolith::vk {

GuiApplication::~GuiApplication() { }

bool GuiApplication::init(CommonInfo &&appInfo, Rc<core::Instance> &&instance) {
	if (instance) {
		if (Application::init(move(appInfo), move(instance))) {
			return true;
		}
	} else {
		auto inst = vk::platform::createInstance([&] (vk::platform::VulkanInstanceData &data, const vk::platform::VulkanInstanceInfo &info) {
			data.applicationName = appInfo.applicationName;
			data.applicationVersion = appInfo.applicationVersion;
			data.checkPresentationSupport = vk::platform::checkPresentationSupport;
			return vk::platform::initInstance(data, info);
		});
		if (Application::init(move(appInfo), move(inst))) {
			return true;
		}
	}
	return false;
}

bool GuiApplication::init(CommonInfo &&appInfo, const Callback<bool(VulkanInstanceData &, const VulkanInstanceInfo &)> &cb) {
	auto instance = vk::platform::createInstance([&] (VulkanInstanceData &data, const VulkanInstanceInfo &info) {
		if (cb(data, info)) {
			data.applicationName = appInfo.applicationName;
			data.applicationVersion = appInfo.applicationVersion;
			return true;
		}
		return false;
	});

	if (Application::init(move(appInfo), move(instance))) {
		return true;
	}

	return false;
}

void GuiApplication::run(const CallbackInfo &cb, uint32_t threadCount, TimeInterval ival) {
	core::LoopInfo info;

	run(cb, move(info), threadCount, ival);
}

void GuiApplication::run(const CallbackInfo &cb, core::LoopInfo &&info, uint32_t threadCount, TimeInterval ival) {
	if (!info.platformData) {
		auto data = Rc<LoopData>::alloc();
		data->deviceSupportCallback = [] (const vk::DeviceInfo &dev) {
			return dev.supportsPresentation() &&
					std::find(dev.availableExtensions.begin(), dev.availableExtensions.end(),
							String(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) != dev.availableExtensions.end();
		};
		data->deviceExtensionsCallback = [] (const vk::DeviceInfo &dev) -> Vector<StringView> {
			Vector<StringView> ret;
			ret.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
			return ret;
		};
		info.platformData = data;
	}

	Application::run(cb, move(info), threadCount, ival);
}

}
