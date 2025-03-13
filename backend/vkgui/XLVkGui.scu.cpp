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

#include "XLCommon.h"
#include "XLVk.h"
#include "XLPlatformViewInterface.h"

// Enable to log key API calls and timings
#ifndef XL_VKAPI_DEBUG
#define XL_VKAPI_DEBUG 0
#endif

#if XL_VKAPI_DEBUG
#define XL_VKAPI_LOG(...) log::debug("vk::Api", __VA_ARGS__)
#else
#define XL_VKAPI_LOG(...)
#endif

#include "XLVkGuiApplication.cc"
#include "XLVkSwapchain.cc"
#include "XLVkPresentationEngine.cc"
#include "XLVkView.cc"
#include "XLVkGuiShared.cc"

#if LINUX
#include "platform/linux/XLVkGuiViewImpl.cc"
#endif

#if ANDROID
#include "platform/android/XLVkGuiViewImpl.cc"
#endif

#if WIN32
#include "platform/win32/XLVkGuiViewImpl.cc"
#endif

#if MACOS
#include "platform/macos/XLVkGuiViewImpl.cc"
#endif
