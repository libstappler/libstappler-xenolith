/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "XLNetworkController.h"

#if LINUX
//#include "linux/XLPlatformLinuxDbus.h"
#endif

#if ANDROID
#include "android/XLPlatformAndroidActivity.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::network {

#if ANDROID

SPUNUSED static void registerNetworkCallback(Application *app, void *key,
		Function<void(NetworkCapabilities)> &&cb) {
	auto activity = reinterpret_cast<platform::Activity *>(app->getInfo().platformHandle);
	cb(NetworkCapabilities(activity->getNetworkCapabilities()));
	activity->addNetworkCallback(key, [key, cb = sp::move(cb)](platform::NetworkCapabilities caps) {
		cb(NetworkCapabilities(caps));
	});
}

SPUNUSED static void unregisterNetworkCallback(Application *app, void *key) {
	auto activity = reinterpret_cast<platform::Activity *>(app->getInfo().platformHandle);
	activity->removeNetworkCallback(key);
}

#endif

} // namespace stappler::xenolith::network
