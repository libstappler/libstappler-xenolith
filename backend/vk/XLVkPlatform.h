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

#ifndef XENOLITH_BACKEND_VK_XLVKPLATFORM_H_
#define XENOLITH_BACKEND_VK_XLVKPLATFORM_H_

#include "XLVk.h"
#include "XLCoreInstance.h"

namespace stappler::xenolith::vk::platform {

struct VulkanInstanceInfo {
	uint32_t targetVersion;
	SpanView<VkLayerProperties> availableLayers;
	SpanView<VkExtensionProperties> availableExtensions;
};

struct VulkanInstanceData {
	uint32_t targetVulkanVersion;
	StringView applicationVersion;
	StringView applicationName;
	Vector<const char *> layersToEnable;
	Vector<const char *> extensionsToEnable;
	Function<uint32_t(const vk::Instance *instance, VkPhysicalDevice device, uint32_t queueIdx)> checkPresentationSupport;
	Rc<Ref> userdata;
};

Rc<core::Instance> createInstance(const Callback<bool(VulkanInstanceData &, const VulkanInstanceInfo &)> &);

}

#endif /* XENOLITH_BACKEND_VK_XLVKPLATFORM_H_ */
