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

#include "XLCorePlatform.h"
#include "SPPlatformUnistd.h"

#if ANDROID

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static uint64_t getStaticMinFrameTime() {
	return 1000'000 / 60;
}

static clockid_t getClockSource() {
	struct timespec ts;

	auto minFrameNano = (getStaticMinFrameTime() * 1000) / 5; // clock should have at least 1/5 frame resolution
	if (clock_getres(CLOCK_MONOTONIC_COARSE, &ts) == 0) {
		if (ts.tv_sec == 0 && uint64_t(ts.tv_nsec) < minFrameNano) {
			return CLOCK_MONOTONIC_COARSE;
		}
	}

	if (clock_getres(CLOCK_MONOTONIC, &ts) == 0) {
		if (ts.tv_sec == 0 && uint64_t(ts.tv_nsec) < minFrameNano) {
			return CLOCK_MONOTONIC;
		}
	}

	if (clock_getres(CLOCK_MONOTONIC_RAW, &ts) == 0) {
		if (ts.tv_sec == 0 && uint64_t(ts.tv_nsec) < minFrameNano) {
			return CLOCK_MONOTONIC_RAW;
		}
	}

	return CLOCK_MONOTONIC;
}

uint64_t clock(core::ClockType type) {
	static clockid_t ClockSource = getClockSource();

	struct timespec ts;
	switch (type) {
	case core::ClockType::Default: clock_gettime(ClockSource, &ts); break;
	case core::ClockType::Monotonic: clock_gettime(CLOCK_MONOTONIC, &ts); break;
	case core::ClockType::Realtime: clock_gettime(CLOCK_REALTIME, &ts); break;
	case core::ClockType::Process: clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts); break;
	case core::ClockType::Thread: clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts); break;
	}

	return uint64_t(ts.tv_sec) * uint64_t(1000'000) + uint64_t(ts.tv_nsec / 1000);
}

void sleep(uint64_t microseconds) {
	usleep(useconds_t(microseconds));
}

}

#endif
