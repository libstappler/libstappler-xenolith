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

#include "XLVkGuiMainLoop.h"
#include "XLVkLoop.h"
#include "XLVkGuiPlatform.h"
#include "platform/linux/XLVkGuiViewImpl.h"

namespace stappler::xenolith::vk {

GuiMainLoop::~GuiMainLoop() { }

bool GuiMainLoop::init(StringView name) {
	if (init(name, [&] (vk::platform::VulkanInstanceData &data, const vk::platform::VulkanInstanceInfo &info) {
		data.applicationName = name;
		data.applicationVersion = XL_MAKE_API_VERSION(0, 0, 1, 0);
		data.checkPresentationSupport = vk::platform::checkPresentationSupport;
		return vk::platform::initInstance(data, info);
	})) {
		return true;
	}

	return false;
}

bool GuiMainLoop::init(StringView name, const Callback<bool(VulkanInstanceData &, const VulkanInstanceInfo &)> &cb) {
	auto instance = vk::platform::createInstance(cb);

	if (MainLoop::init(name, move(instance))) {
		return true;
	}

	return false;
}

void GuiMainLoop::run(const CallbackInfo &cb, uint32_t threadCount, TimeInterval ival) {
	core::LoopInfo info;

	run(cb, move(info), threadCount, ival);
}

void GuiMainLoop::run(const CallbackInfo &cb, core::LoopInfo &&info, uint32_t threadCount, TimeInterval ival) {
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

	MainLoop::run(cb, move(info), threadCount, ival);
}

bool GuiMainLoop::addView(ViewInfo &&info) {
	if (!_glLoop) {
		return false;
	}

	_glLoop->performOnGlThread([this, info = move(info)] () mutable {
		if (_device) {
			Rc<vk::platform::ViewImpl>::create(*this, *_device, move(info));
		} else {
			_tmpViews.emplace_back(move(info));
		}
	}, this);

	return true;
}

void GuiMainLoop::removeView(xenolith::View *view) {

}

void GuiMainLoop::handleDeviceStarted(const core::Loop &loop, const core::Device &dev) {
	MainLoop::handleDeviceStarted(loop, dev);

	_device = &dev;

	for (auto &it : _tmpViews) {
		Rc<vk::platform::ViewImpl>::create(*this, *_device, move(it));
	}

	_tmpViews.clear();
}

void GuiMainLoop::handleDeviceFinalized(const core::Loop &loop, const core::Device &dev) {
	_device = nullptr;

	MainLoop::handleDeviceFinalized(loop, dev);
}

}
