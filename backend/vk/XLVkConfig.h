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

#ifndef XENOLITH_BACKEND_VK_XLVKCONFIG_H_
#define XENOLITH_BACKEND_VK_XLVKCONFIG_H_

#include <stdint.h>

namespace stappler::xenolith::config {

/* Max sampled image descriptors per material texture set (can be actually lower due maxPerStageDescriptorSampledImages) */
static constexpr uint32_t MaxTextureSetImages = 1024;

/* Max buffers in buffer array */
static constexpr uint32_t MaxBufferArrayObjects = 64;

/* Presentation Scheduler interval, used for non-blocking vkWaitForFence */
static constexpr uint64_t PresentationSchedulerInterval = 500; // 500 ms or 1/32 of 60fps frame

inline uint16_t getGlThreadCount() {
#if DEBUG
	return math::clamp(uint16_t(std::thread::hardware_concurrency()), uint16_t(2), uint16_t(4));
#else
	return math::clamp(uint16_t(std::thread::hardware_concurrency()), uint16_t(4), uint16_t(16));
#endif
}

}

#endif /* XENOLITH_BACKEND_VK_XLVKCONFIG_H_ */
