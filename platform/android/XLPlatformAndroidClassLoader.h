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

#ifndef XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROIDCLASSLOADER_H_
#define XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROIDCLASSLOADER_H_

#include "XLPlatformAndroid.h"

#if ANDROID

#include <android/native_activity.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct ClassLoader : Ref {
	struct NativePaths {
		jstring apkPath = nullptr;
		jstring nativeLibraryDir = nullptr;
	};

	jobject activityClassLoader = nullptr;
	jclass activityClassLoaderClass = nullptr;

	jobject apkClassLoader = nullptr;
	jclass apkClassLoaderClass = nullptr;

	jclass loaderClassClass = nullptr;

	jmethodID findClassMethod = nullptr;
	jmethodID getMethodsMethod = nullptr;
	jmethodID getFieldsMethod = nullptr;
	jmethodID getClassNameMethod = nullptr;
	jmethodID getFieldNameMethod = nullptr;
	jmethodID getFieldTypeMethod = nullptr;
	jmethodID getFieldIntMethod = nullptr;
	jmethodID getMethodNameMethod = nullptr;

	String apkPath;
	String nativeLibraryDir;

	int32_t sdkVersion = 0;

	ClassLoader();
	~ClassLoader();

	bool init(ANativeActivity *activity, int32_t sdk);
	void finalize(JNIEnv *);

	void foreachMethod(JNIEnv *, jclass, const Callback<void(JNIEnv *, StringView, jobject)> &);
	void foreachField(JNIEnv *, jclass, const Callback<void(JNIEnv *, StringView, StringView, jobject)> &);

	int getIntField(JNIEnv *, jobject origin, jobject field);

	jclass findClass(JNIEnv *, StringView);
	jclass findClass(JNIEnv *, jstring);
	jstring getClassName(JNIEnv *, jclass);
	NativePaths getNativePaths(JNIEnv *, jobject, jclass = nullptr);
	jstring getCodeCachePath(JNIEnv *, jobject, jclass = nullptr);
};

}

#endif

#endif /* XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROIDCLASSLOADER_H_ */
