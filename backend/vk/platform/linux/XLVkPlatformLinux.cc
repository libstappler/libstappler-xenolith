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

#include "XLVkPlatform.h"

#if LINUX

#include <dlfcn.h>

namespace stappler::xenolith::vk::platform {

Rc<core::Instance> createInstance(const Callback<bool(VulkanInstanceData &, const VulkanInstanceInfo &)> &cb) {
	auto handle = ::dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_LOCAL);
	if (!handle) {
		log::error("Vk", "Fail to open libvulkan.so.1");
		return nullptr;
	}

	auto getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(handle, "vkGetInstanceProcAddr"));
	if (!getInstanceProcAddr) {
		return nullptr;
	}

	FunctionTable table(getInstanceProcAddr);

	if (!table) {
		::dlclose(handle);
		return nullptr;
	}

	if (auto instance = table.createInstance(cb, [handle] { ::dlclose(handle); })) {
		return instance;
	}

	::dlclose(handle);
	return nullptr;
}

}

#endif
