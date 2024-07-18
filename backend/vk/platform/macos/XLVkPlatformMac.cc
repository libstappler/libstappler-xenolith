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

#include "XLVkPlatform.h"

#if MACOS

#include "SPDso.h"
#include <mach-o/dyld.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

Rc<core::Instance> createInstance(const Callback<bool(VulkanInstanceData &, const VulkanInstanceInfo &)> &cb) {
	char pathBuf[1024];
	uint32_t size = sizeof(pathBuf);
	if (::_NSGetExecutablePath(pathBuf, &size) != 0) {
		log::error("Vulkan", "Fail to detect executable path");
		return nullptr;
	}

	auto loaderPath = filepath::merge<Interface>(filepath::root(pathBuf), "vulkan/lib", "libvulkan.dylib");
	if (!filesystem::exists(loaderPath)) {
		log::error("Vulkan", "Vulkan loader is not found on path: ", loaderPath);
		return nullptr;
	}

	::setenv("VK_LAYER_PATH", filepath::merge<Interface>(filepath::root(pathBuf), "vulkan", "explicit_layer.d").data(), 1);

	Dso handle(loaderPath);
	if (!handle) {
		log::error("Vulkan", "Fail to dlopen loader: ", loaderPath);
		return nullptr;
	}

	auto getInstanceProcAddr = handle.sym<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	if (!getInstanceProcAddr) {
		log::error("Vulkan", "Fail to find entrypoint 'vkGetInstanceProcAddr' in loader: ", loaderPath);
		return nullptr;
	}

	FunctionTable table(getInstanceProcAddr);

	if (!table) {
		return nullptr;
	}

	if (auto instance = table.createInstance(cb, move(handle), [] { })) {
		return instance;
	}

	return nullptr;
}

}

#endif
