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

#ifndef XENOLITH_BACKEND_VKGUI_XLVKGUICONFIG_H_
#define XENOLITH_BACKEND_VKGUI_XLVKGUICONFIG_H_

#include "XLCommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::config {

/* forced view update interval for on-demand frame mode */
static constexpr uint64_t OnDemandFrameInterval = 1'000'000;

/* Number of frames, that can be performed in suboptimal swapchain modes */
static constexpr uint32_t MaxSuboptimalFrames = 24;

/* Default update interval for main loop */
static constexpr uint64_t GuiMainLoopDefaultInterval = 100000;

inline uint16_t getGlThreadCount() {
#if DEBUG
	return math::clamp(uint16_t(std::thread::hardware_concurrency()), uint16_t(2), uint16_t(4));
#else
	return math::clamp(uint16_t(std::thread::hardware_concurrency()), uint16_t(4), uint16_t(16));
#endif
}

inline uint16_t getMainThreadCount() {
#if DEBUG
	return math::clamp(uint16_t(std::thread::hardware_concurrency() / 2), uint16_t(2), uint16_t(4));
#else
	return math::clamp(uint16_t(std::thread::hardware_concurrency() / 2), uint16_t(2), uint16_t(16));
#endif
}

}

#endif /* XENOLITH_BACKEND_VKGUI_XLVKGUICONFIG_H_ */
