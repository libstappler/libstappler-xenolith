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

#ifndef XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROIDCLASSLOADER_H_
#define XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROIDCLASSLOADER_H_

#include "XLPlatformAndroid.h"

#if ANDROID

#include <android/native_activity.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct ClassLoader : Ref {
	struct NativePaths {
		jni::LocalString apkPath = nullptr;
		jni::LocalString nativeLibraryDir = nullptr;
	};

	jni::Global activityClassLoader = nullptr;
	jni::GlobalClass activityClassLoaderClass = nullptr;

	jni::Global apkClassLoader = nullptr;
	jni::GlobalClass apkClassLoaderClass = nullptr;

	jni::GlobalClass loaderClassClass = nullptr;

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
	void finalize();

	void foreachMethod(const jni::RefClass &, const Callback<void(StringView, const jni::Ref &)> &);
	void foreachField(const jni::RefClass &, const Callback<void(StringView, StringView, const jni::Ref &)> &);

	int getIntField(const jni::Ref &origin, const jni::Ref &field);

	jni::LocalClass findClass(const jni::Env &, StringView);
	jni::LocalClass findClass(const jni::RefString &);
	NativePaths getNativePaths(const jni::Ref &, const jni::RefClass & = nullptr);
	jni::LocalString getCodeCachePath(const jni::Ref &, const jni::RefClass & = nullptr);
};

}

#endif

#endif /* XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROIDCLASSLOADER_H_ */
