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
#include "platform/linux/XLVkGuiViewImpl.h"
#include "XLVkGuiApplication.h"

#include "XLScene.h"
#include "XLVkGuiPlatform.h"
/*
namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

GuiApplication::~GuiApplication() { }

bool GuiApplication::init(ApplicationInfo &&appInfo, Rc<core::Instance> &&instance) {
	if (instance) {
		if (ViewApplication::init(move(appInfo), move(instance))) {
			return true;
		}
	} else {
		auto inst =
				vk::platform::createInstance([&](vk::platform::VulkanInstanceData &data,
													 const vk::platform::VulkanInstanceInfo &info) {
			data.applicationName = appInfo.applicationName;
			data.applicationVersion = appInfo.applicationVersion;
			data.checkPresentationSupport = vk::platform::checkPresentationSupport;
			return vk::platform::initInstance(data, info);
		});
		if (ViewApplication::init(move(appInfo), move(inst))) {
			return true;
		}
	}
	return false;
}

bool GuiApplication::init(ApplicationInfo &&appInfo,
		const Callback<bool(VulkanInstanceData &, const VulkanInstanceInfo &)> &cb) {
	auto instance = vk::platform::createInstance(
			[&](VulkanInstanceData &data, const VulkanInstanceInfo &info) {
		if (cb(data, info)) {
			data.applicationName = appInfo.applicationName;
			data.applicationVersion = appInfo.applicationVersion;
			return true;
		}
		return false;
	});

	if (ViewApplication::init(move(appInfo), move(instance))) {
		return true;
	}

	return false;
}

void GuiApplication::run(uint32_t threadCount, TimeInterval ival) {
	core::LoopInfo info = _info.loopInfo;

	run(move(info), threadCount, ival);
}

void GuiApplication::run(core::LoopInfo &&info, uint32_t threadCount, TimeInterval ival) {
	_info.appThreadsCount = threadCount;
	_info.updateInterval = ival;
	_info.loopInfo = move(info);

	if (!_info.loopInfo.platformData) {
		auto data = Rc<LoopData>::alloc();
		data->deviceSupportCallback = [](const vk::DeviceInfo &dev) {
			return dev.supportsPresentation()
					&& std::find(dev.availableExtensions.begin(), dev.availableExtensions.end(),
							   String(VK_KHR_SWAPCHAIN_EXTENSION_NAME))
					!= dev.availableExtensions.end();
		};
		data->deviceExtensionsCallback = [](const vk::DeviceInfo &dev) -> Vector<StringView> {
			Vector<StringView> ret;
			ret.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
			return ret;
		};
		_info.loopInfo.platformData = data;
	}

	ViewApplication::run();
}

} // namespace stappler::xenolith::vk
*/
