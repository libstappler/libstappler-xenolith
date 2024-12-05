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
#include "XLPlatform.h"

#include "XLPlatformApplication.cc"

#if LINUX

#if XL_X11_DEBUG
#define XL_X11_LOG(...) log::debug("XCB", __VA_ARGS__)
#else
#define XL_X11_LOG(...)
#endif

#include "linux/thirdparty/glfw/xkb_unicode.cc"
#include "linux/XLPlatformLinuxDbus.cc"
#include "linux/XLPlatformLinuxXkb.cc"
#include "linux/XLPlatformLinuxWayland.cc"
#include "linux/XLPlatformLinuxWaylandView.cc"
#include "linux/XLPlatformLinuxXcb.cc"
#include "linux/XLPlatformLinuxXcbConnection.cc"
#include "linux/XLPlatformLinuxXcbView.cc"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

core::ImageFormat getCommonFormat() {
	return core::ImageFormat::B8G8R8A8_UNORM;
}

}

#endif

#if WIN32

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

core::ImageFormat getCommonFormat() {
	return core::ImageFormat::B8G8R8A8_UNORM;
}

}

#include "win32/XLPlatformWin32Library.cc"
#include "win32/XLPlatformWin32View.cc"

#endif

#if ANDROID
#include "android/XLPlatformAndroid.cc"
#include "android/XLPlatformAndroidActivity.cc"
#include "android/XLPlatformAndroidClassLoader.cc"
#include "android/XLPlatformAndroidMessageInterface.cc"
#include "android/XLPlatformAndroidNetworkConnectivity.cc"
#endif

#if MACOS
namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

core::ImageFormat getCommonFormat() {
	return core::ImageFormat::B8G8R8A8_UNORM;
}

}

#include "macos/XLPlatformMacosUtils.cc"

#endif
