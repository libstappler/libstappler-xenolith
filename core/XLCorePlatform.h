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

#ifndef XENOLITH_CORE_PLATFORM_XLCOREPLATFORM_H_
#define XENOLITH_CORE_PLATFORM_XLCOREPLATFORM_H_

#include "XLCore.h"
#include "XLCoreEnum.h"

namespace stappler::xenolith::platform {

inline const char *name() { return "Stappler+Xenolith"; }
inline uint32_t version() { return XL_MAKE_API_VERSION(0, 1, 0, 0); }

uint64_t clock(core::ClockType = core::ClockType::Default);
void sleep(uint64_t microseconds);

// should be commonly supported format,
// R8G8B8A8_UNORM on Android, B8G8R8A8_UNORM on others
core::ImageFormat getCommonFormat();

}

#endif /* XENOLITH_CORE_PLATFORM_XLCOREPLATFORM_H_ */
