/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROID_H_
#define XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROID_H_

#include "XLCommon.h"
#include "XLCoreInput.h"
#include "SPThread.h"

#if ANDROID

#include <jni.h>
#include <android/configuration.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct NetworkConnectivity;
struct ClassLoader;

class Activity;

struct NativeBufferFormatSupport {
	bool R8G8B8A8_UNORM = true;
	bool R8G8B8X8_UNORM = true;
	bool R8G8B8_UNORM = true;
	bool R5G6B5_UNORM = true;
	bool R16G16B16A16_FLOAT = true;
	bool R10G10B10A2_UNORM = true;
};

struct ActivityInfo {
	static ActivityInfo get(AConfiguration *, JNIEnv *env, jclass activityClass, jobject activity,
			const ActivityInfo *prev = nullptr);

	String bundleName;
	String applicationName;
	String applicationVersion;
	String userAgent;
	String systemAgent;
	String locale;

	float density = 1.0f;

	Extent2 sizeInPixels;
	Size2 sizeInDp;
};

void checkJniError(JNIEnv *env);

void saveApplicationInfo(const Value &);
Value loadApplicationInfo();

void saveMessageToken(StringView);
String loadMessageToken();

}

#endif

#endif /* XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROID_H_ */
