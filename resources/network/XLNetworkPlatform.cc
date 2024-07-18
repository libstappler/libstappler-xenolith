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

#include "XLNetworkController.h"

#if LINUX
#include "linux/XLPlatformLinuxDbus.h"
#endif

#if ANDROID
#include "android/XLPlatformAndroidActivity.h"
#endif

#if WIN32
#include "win32/XLPlatformWin32Library.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::network {

#if LINUX

SPUNUSED static void registerNetworkCallback(Application *, void *key, Function<void(NetworkCapabilities)> &&cb) {
	auto lib = stappler::xenolith::platform::DBusLibrary::get();
	lib.addNetworkConnectionCallback(key, [cb = move(cb)] (const stappler::xenolith::platform::NetworkState &state) {
		NetworkCapabilities defaultFlags = NetworkCapabilities::NotRoaming | NetworkCapabilities::NotCongested | NetworkCapabilities::NotVpn;
		NetworkCapabilities caps = NetworkCapabilities::None;

		switch (state.connectivity) {
		case platform::NM_CONNECTIVITY_UNKNOWN:
		case platform::NM_CONNECTIVITY_NONE:
			break;
		case platform::NM_CONNECTIVITY_PORTAL:
			caps |= NetworkCapabilities::Internet | NetworkCapabilities::CaptivePortal | defaultFlags;
			break;
		case platform::NM_CONNECTIVITY_LIMITED:
			caps |= NetworkCapabilities::Internet | defaultFlags;
			break;
		case platform::NM_CONNECTIVITY_FULL:
			caps |= NetworkCapabilities::Internet | NetworkCapabilities::Validated | NetworkCapabilities::NotRestricted | defaultFlags;
			break;
		}

		switch (state.state) {
		case platform::NM_STATE_UNKNOWN:
			break;
		case platform::NM_STATE_ASLEEP:
			break;
		case platform::NM_STATE_DISCONNECTED:
			break;
		case platform::NM_STATE_DISCONNECTING:
			break;
		case platform::NM_STATE_CONNECTING:
			break;
		case platform::NM_STATE_CONNECTED_LOCAL:
			caps |= NetworkCapabilities::NotSuspended;
			break;
		case platform::NM_STATE_CONNECTED_SITE:
			caps |= NetworkCapabilities::NotSuspended;
			break;
		case platform::NM_STATE_CONNECTED_GLOBAL:
			caps |= NetworkCapabilities::NotRestricted | NetworkCapabilities::NotSuspended;
			break;
		}

		switch (state.metered) {
		case platform::NM_METERED_UNKNOWN:
			break;
		case platform::NM_METERED_YES:
		case platform::NM_METERED_GUESS_YES:
			break;
		case platform::NM_METERED_NO:
		case platform::NM_METERED_GUESS_NO:
			caps |= NetworkCapabilities::NotMetered;
			break;
		}

		cb(caps);
	});
}

SPUNUSED static void unregisterNetworkCallback(Application *, void *key) {
	auto lib = stappler::xenolith::platform::DBusLibrary::get();
	lib.removeNetworkConnectionCallback(key);
}

#endif

#if ANDROID

SPUNUSED static void registerNetworkCallback(Application *app, void *key, Function<void(NetworkCapabilities)> &&cb) {
	auto activity = reinterpret_cast<platform::Activity *>(app->getInfo().nativeHandle);
	cb(NetworkCapabilities(activity->getNetworkCapabilities()));
	activity->addNetworkCallback(key, [key, cb = move(cb)] (platform::NetworkCapabilities caps) {
		cb(NetworkCapabilities(caps));
	});

}

SPUNUSED static void unregisterNetworkCallback(Application *app, void *key) {
	auto activity = reinterpret_cast<platform::Activity *>(app->getInfo().nativeHandle);
	activity->removeNetworkCallback(key);
}

#endif

#if WIN32

SPUNUSED static void registerNetworkCallback(Application *, void *key, Function<void(NetworkCapabilities)> &&cb) {
	auto lib = stappler::xenolith::platform::Win32Library::getInstance();
	lib->addNetworkConnectionCallback(key, move(cb));
}

SPUNUSED static void unregisterNetworkCallback(Application *app, void *key) {
	auto lib = stappler::xenolith::platform::Win32Library::getInstance();
	lib->removeNetworkConnectionCallback(key);

}

#endif

#if MACOS

SPUNUSED static void registerNetworkCallback(Application *, void *key, Function<void(NetworkCapabilities)> &&cb) {

}

SPUNUSED static void unregisterNetworkCallback(Application *app, void *key) {


}

#endif

}
