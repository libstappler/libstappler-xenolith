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

#ifndef XENOLITH_PLATFORM_XLPLATFORMNETWORK_H_
#define XENOLITH_PLATFORM_XLPLATFORMNETWORK_H_

#include "XLCommon.h"

namespace stappler::xenolith::platform {

enum class NetworkCapabilities {
	None,
	Internet = (1 << 0),
	NotCongested = (1 << 1),
	NotMetered = (1 << 2),
	NotRestricted = (1 << 3),
	NotRoaming = (1 << 4),
	NotSuspended = (1 << 5),
	NotVpn = (1 << 6),
	PrioritizeBandwidth = (1 << 7),
	PrioritizeLatency = (1 << 8),
	TemporarilyNotMetered = (1 << 9),
	Trusted = (1 << 10),
	Validated = (1 << 11),
	WifiP2P = (1 << 12),
	CaptivePortal = (1 << 13),
	Local = (1 << 14)
};

SP_DEFINE_ENUM_AS_MASK(NetworkCapabilities)

}

#endif /* XENOLITH_PLATFORM_XLPLATFORMNETWORK_H_ */
