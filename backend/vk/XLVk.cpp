/**
Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLCommon.h"
#include "XLVk.h"

#define XL_VK_DEBUG 0
#define XL_VKDEVICE_DEBUG 0
#define XL_VKAPI_DEBUG 0

#if XL_VK_DEBUG
#define XL_VK_LOG(...) log::vtext("Vk::Loop", __VA_ARGS__)
#else
#define XL_VK_LOG(...)
#endif

#if XL_VKDEVICE_DEBUG
#define XL_VKDEVICE_LOG(...) log::vtext("Vk::Device", __VA_ARGS__)
#else
#define XL_VKDEVICE_LOG(...)
#endif

#if XL_VKAPI_DEBUG
#define XL_VKAPI_LOG(...) log::vtext("vk::Api", __VA_ARGS__)
#else
#define XL_VKAPI_LOG(...)
#endif

#include "XLVkDeviceQueue.cc"
#include "XLVkDevice.cc"
#include "XLVkSync.cc"
#include "XLVkAllocator.cc"
#include "XLVkPipeline.cc"
#include "XLVkRenderPass.cc"
#include "XLVkObject.cc"
#include "XLVkBuffer.cc"
#include "XLVkTextureSet.cc"

#include "XLVkLoop.cc"
#include "XLVkInstance.cc"
#include "XLVkTable.cc"
#include "XLVkInfo.cc"

#include "XLVkAttachment.cc"
#include "XLVkQueuePass.cc"
#include "XLVkRenderQueueCompiler.cc"
#include "XLVkTransferQueue.cc"

#include "platform/android/XLVkPlatform.cc"
#include "platform/linux/XLVkPlatform.cc"
