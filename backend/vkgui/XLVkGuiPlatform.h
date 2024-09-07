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

#ifndef XENOLITH_BACKEND_VKGUI_XLVKGUIPLATFORM_H_
#define XENOLITH_BACKEND_VKGUI_XLVKGUIPLATFORM_H_

#include "XLVkView.h"
#include "XLVkPlatform.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

SP_PUBLIC Rc<vk::View> createView(Application &, const core::Device &, ViewInfo &&);

SP_PUBLIC bool initInstance(vk::platform::VulkanInstanceData &data, const vk::platform::VulkanInstanceInfo &info);

SP_PUBLIC uint32_t checkPresentationSupport(const vk::Instance *instance, VkPhysicalDevice device, uint32_t queueIdx);

}

#endif /* XENOLITH_BACKEND_VKGUI_XLVKGUIPLATFORM_H_ */
