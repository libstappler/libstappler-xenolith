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

#ifndef XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROIDNETWORKCONNECTIVITY_H_
#define XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROIDNETWORKCONNECTIVITY_H_

#include "XLPlatformAndroid.h"
#include "XLPlatformNetwork.h"

namespace stappler::xenolith::platform {

struct NetworkConnectivity : public Ref {
	jclass clazz = nullptr;
	jobject thiz = nullptr;
	NetworkCapabilities capabilities;
	Function<void(NetworkCapabilities)> callback;

	jmethodID j_hasCapability = nullptr;

	int32_t sdkVersion = 0;

	bool init(JNIEnv *, ClassLoader *, jobject context, Function<void(NetworkCapabilities)> && = nullptr);
	void finalize(JNIEnv *);

	void handleCreated(JNIEnv *, jobject, jobject);
	void handleFinalized(JNIEnv *);
	void handleAvailable(JNIEnv *, jobject, jobject);
	void handleLost(JNIEnv *);
	void handleCapabilitiesChanged(JNIEnv *, jobject);
	void handleLinkPropertiesChanged(JNIEnv *, jobject);
};

}

#endif /* XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROIDNETWORKCONNECTIVITY_H_ */
