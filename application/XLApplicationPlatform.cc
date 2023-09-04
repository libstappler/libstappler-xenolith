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

#include "XLApplication.h"

#if ANDROID

#include "android/XLPlatformAndroidActivity.h"

namespace stappler::xenolith {

void Application::nativeInit() {
	auto activity = static_cast<platform::Activity *>(_info.nativeHandle);

	auto tok = activity->getMessageToken();
	if (!tok.empty()) {
		handleMessageToken(move(tok));
	}
	activity->addTokenCallback(this, [this] (StringView str) {
		performOnMainThread([this, str = str.str<Interface>()] () mutable {
			handleMessageToken(move(str));
		}, this);
	});

	activity->addRemoteNotificationCallback(this, [this] (const Value &val) {
		performOnMainThread([this, val = val] () mutable {
			handleRemoteNotification(move(val));
		}, this);
	});
}

void Application::nativeDispose() {
	auto activity = static_cast<platform::Activity *>(_info.nativeHandle);

	activity->removeTokenCallback(this);
	activity->removeRemoteNotificationCallback(this);
}

void Application::openUrl(StringView url) const {
	auto activity = static_cast<platform::Activity *>(_info.nativeHandle);

	activity->openUrl(url);
}

}

#endif


#if LINUX

#include <stdlib.h>

namespace stappler::xenolith {

void Application::nativeInit() { }

void Application::nativeDispose() { }

void Application::openUrl(StringView url) const {
	::system(toString("xdg-open ", url).data());
}

}

#endif
