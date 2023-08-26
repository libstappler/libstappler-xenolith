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

#include "XLPlatformAndroidClassLoader.h"

#if ANDROID

namespace stappler::xenolith::platform {

ClassLoader::ClassLoader() { }

ClassLoader::~ClassLoader() { }

bool ClassLoader::init(ANativeActivity *activity, int32_t sdk) {
	sdkVersion = sdk;
	auto activityClass = activity->env->GetObjectClass(activity->clazz);
	auto classClass = activity->env->GetObjectClass(activityClass);
	auto getClassLoaderMethod = activity->env->GetMethodID(classClass,
			"getClassLoader", "()Ljava/lang/ClassLoader;");

	auto path = stappler::filesystem::platform::Android_getApkPath();
	auto codeCachePath = getCodeCachePath(activity->env, activity->clazz, activityClass);
	auto paths = getNativePaths(activity->env, activity->clazz, activityClass);

	apkPath = activity->env->GetStringUTFChars(paths.apkPath, NULL);
	nativeLibraryDir = activity->env->GetStringUTFChars(paths.nativeLibraryDir, NULL);

	filesystem::ftw(nativeLibraryDir, [] (StringView path, bool isFile) {
		if (isFile) {
			log::info("NativeClassLoader", path);
		}
	});

	auto classLoader = activity->env->CallObjectMethod(activityClass, getClassLoaderMethod);

	activity->env->DeleteLocalRef(activityClass);

	if (!codeCachePath || !paths.apkPath) {
		checkJniError(activity->env);
		return false;
	}

	if (classLoader) {
		activityClassLoader = activity->env->NewGlobalRef(classLoader);
		activity->env->DeleteLocalRef(classLoader);

		auto classLoaderClass = activity->env->GetObjectClass(activityClassLoader);
		activityClassLoaderClass = reinterpret_cast<jclass>(activity->env->NewGlobalRef(classLoaderClass));
		activity->env->DeleteLocalRef(classLoaderClass);

		auto className = getClassName(activity->env, activityClassLoaderClass);
		const char *mstr = activity->env->GetStringUTFChars(className, NULL);

		log::info("JNI", "Activity: ClassLoader: ", mstr);
		if (StringView(mstr) == StringView("java.lang.BootClassLoader")) {
			// acquire new dex class loader
			auto dexClassLoaderClass = activity->env->FindClass("dalvik/system/DexClassLoader");
			if (!dexClassLoaderClass) {
				checkJniError(activity->env);
				return false;
			}

			jmethodID dexClassLoaderConstructor = activity->env->GetMethodID(dexClassLoaderClass, "<init>",
					"(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");

			auto dexLoader = activity->env->NewObject(dexClassLoaderClass, dexClassLoaderConstructor,
					paths.apkPath, codeCachePath, paths.nativeLibraryDir, activityClassLoader);
			if (dexLoader) {
				apkClassLoader = activity->env->NewGlobalRef(dexLoader);
				activity->env->DeleteLocalRef(dexLoader);

				apkClassLoaderClass = reinterpret_cast<jclass>(activity->env->NewGlobalRef(dexClassLoaderClass));
				activity->env->DeleteLocalRef(dexClassLoaderClass);

				findClassMethod = activity->env->GetMethodID(apkClassLoaderClass, "loadClass", "(Ljava/lang/String;Z)Ljava/lang/Class;");

				auto j_classClass = activity->env->GetObjectClass(apkClassLoaderClass);
				loaderClassClass = reinterpret_cast<jclass>(activity->env->NewGlobalRef(j_classClass));
				activity->env->DeleteLocalRef(j_classClass);

				getMethodsMethod = activity->env->GetMethodID(loaderClassClass, "getMethods", "()[Ljava/lang/reflect/Method;");
				getFieldsMethod = activity->env->GetMethodID(loaderClassClass, "getFields", "()[Ljava/lang/reflect/Field;");
			} else {
				checkJniError(activity->env);
				return false;
			}
		} else {
			apkClassLoader = activity->env->NewGlobalRef(activityClassLoader);
			apkClassLoaderClass = reinterpret_cast<jclass>(activity->env->NewGlobalRef(activityClassLoaderClass));
			loaderClassClass = reinterpret_cast<jclass>(activity->env->NewGlobalRef(classClass));
			findClassMethod = activity->env->GetMethodID(activityClassLoaderClass, "loadClass", "(Ljava/lang/String;Z)Ljava/lang/Class;");
			getMethodsMethod = activity->env->GetMethodID(loaderClassClass, "getMethods", "()[Ljava/lang/reflect/Method;");
			getFieldsMethod = activity->env->GetMethodID(loaderClassClass, "getFields", "()[Ljava/lang/reflect/Field;");
		}
	}

	if (loaderClassClass) {
		getClassNameMethod = activity->env->GetMethodID(loaderClassClass, "getName", "()Ljava/lang/String;");
	}

	activity->env->DeleteLocalRef(classClass);

	if (auto methodClass = findClass(activity->env, "java/lang/reflect/Method")) {
		getMethodNameMethod = activity->env->GetMethodID(methodClass, "getName", "()Ljava/lang/String;");
		activity->env->DeleteLocalRef(methodClass);
	}

	if (auto fieldClass = findClass(activity->env, "java/lang/reflect/Field")) {
		getFieldNameMethod = activity->env->GetMethodID(fieldClass, "getName", "()Ljava/lang/String;");
		getFieldTypeMethod = activity->env->GetMethodID(fieldClass, "getType", "()Ljava/lang/Class;");
		getFieldIntMethod = activity->env->GetMethodID(fieldClass, "getInt", "(Ljava/lang/Object;)I");
		activity->env->DeleteLocalRef(fieldClass);
	}

	checkJniError(activity->env);

	return true;
}

void ClassLoader::finalize(JNIEnv *env) {
	if (activityClassLoader) {
		env->DeleteGlobalRef(activityClassLoader);
		activityClassLoader = nullptr;
	}
	if (activityClassLoaderClass) {
		env->DeleteGlobalRef(activityClassLoaderClass);
		activityClassLoaderClass = nullptr;
	}
	if (apkClassLoader) {
		env->DeleteGlobalRef(apkClassLoader);
		apkClassLoader = nullptr;
	}
	if (apkClassLoaderClass) {
		env->DeleteGlobalRef(apkClassLoaderClass);
		apkClassLoaderClass = nullptr;
	}
	findClassMethod = nullptr;
}

void ClassLoader::foreachMethod(JNIEnv *env, jclass cl, const Callback<void(JNIEnv *, StringView, jobject)> &cb) {
	jobjectArray jobjArray = static_cast<jobjectArray>(env->CallObjectMethod(cl, getMethodsMethod));
	jsize len = env->GetArrayLength(jobjArray);

	for (jsize i = 0 ; i < len ; i++) {
		jobject methodObject = env->GetObjectArrayElement(jobjArray, i);

		jstring j_name = static_cast<jstring>(env->CallObjectMethod(methodObject, getMethodNameMethod));
		const char *str = env->GetStringUTFChars(j_name, 0);

		cb(env, str, methodObject);

		env->ReleaseStringUTFChars(j_name, str);
		env->DeleteLocalRef(methodObject);
	}

	env->DeleteLocalRef(jobjArray);
}

void ClassLoader::foreachField(JNIEnv *env, jclass cl, const Callback<void(JNIEnv *, StringView, StringView, jobject)> &cb) {
	jobjectArray jobjArray = static_cast<jobjectArray>(env->CallObjectMethod(cl, getFieldsMethod));
	jsize len = env->GetArrayLength(jobjArray);

	for (jsize i = 0 ; i < len ; i++) {
		jobject fieldObject = env->GetObjectArrayElement(jobjArray, i);
		jobject fieldType = env->CallObjectMethod(fieldObject, getFieldTypeMethod);

		jstring j_type = static_cast<jstring>(env->CallObjectMethod(fieldType, getClassNameMethod));
		jstring j_name = static_cast<jstring>(env->CallObjectMethod(fieldObject, getFieldNameMethod));

		const char *c_name = env->GetStringUTFChars(j_name, 0);
		const char *c_type = env->GetStringUTFChars(j_type, 0);

		cb(env, c_type, c_name, fieldObject);

		env->ReleaseStringUTFChars(j_type, c_type);
		env->ReleaseStringUTFChars(j_name, c_name);

		env->DeleteLocalRef(j_type);
		env->DeleteLocalRef(j_name);
		env->DeleteLocalRef(fieldType);
		env->DeleteLocalRef(fieldObject);
	}

	env->DeleteLocalRef(jobjArray);
}

int ClassLoader::getIntField(JNIEnv *env, jobject origin, jobject field) {
	auto ret = env->CallIntMethod(field, getFieldIntMethod, origin);
	return ret;
}

jclass ClassLoader::findClass(JNIEnv *env, StringView data) {
	auto tmp = env->NewStringUTF(data.data());
	auto ret = findClass(env, tmp);
	env->DeleteLocalRef(tmp);
	return ret;
}

jclass ClassLoader::findClass(JNIEnv *env, jstring name) {
	auto ret = reinterpret_cast<jclass>(env->CallObjectMethod(apkClassLoader, findClassMethod, name, jboolean(1)));
	checkJniError(env);
	return ret;
}

jstring ClassLoader::getClassName(JNIEnv *env, jclass cl) {
	jclass classClass = env->GetObjectClass(cl);
	jmethodID getName = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");
	jstring name = reinterpret_cast<jstring>(env->CallObjectMethod(cl, getName));
	env->DeleteLocalRef(classClass);
	return name;
}

ClassLoader::NativePaths ClassLoader::getNativePaths(JNIEnv *env, jobject context, jclass cl) {
	NativePaths ret;
	bool hasClass = (cl != nullptr);
	if (!cl) {
		cl = env->GetObjectClass(context);
	}

	auto getPackageNameMethod = env->GetMethodID(cl, "getPackageName", "()Ljava/lang/String;");
	auto getPackageManagerMethod = env->GetMethodID(cl, "getPackageManager", "()Landroid/content/pm/PackageManager;");

	auto packageName = reinterpret_cast<jstring>(env->CallObjectMethod(context, getPackageNameMethod));
	auto packageManager = env->CallObjectMethod(context, getPackageManagerMethod);

	if (packageName && packageManager) {
		auto packageManagerClass = env->GetObjectClass(packageManager);
		//auto packageManagerClass = env->FindClass("android/content/pm/PackageManager");
		auto getApplicationInfoMethod = env->GetMethodID(packageManagerClass, "getApplicationInfo",
				"(Ljava/lang/String;I)Landroid/content/pm/ApplicationInfo;");

		auto applicationInfo = env->CallObjectMethod(packageManager, getApplicationInfoMethod, packageName, 0);

		if (applicationInfo) {
			auto applicationInfoClass = env->GetObjectClass(applicationInfo);
			auto publicSourceDirField = env->GetFieldID(applicationInfoClass, "publicSourceDir", "Ljava/lang/String;");
			auto nativeLibraryDirField = env->GetFieldID(applicationInfoClass, "nativeLibraryDir", "Ljava/lang/String;");

			ret.apkPath = reinterpret_cast<jstring>(env->GetObjectField(applicationInfo, publicSourceDirField));
			ret.nativeLibraryDir = reinterpret_cast<jstring>(env->GetObjectField(applicationInfo, nativeLibraryDirField));

			env->DeleteLocalRef(applicationInfoClass);
			env->DeleteLocalRef(applicationInfo);
		}

		if (packageManagerClass) { env->DeleteLocalRef(packageManagerClass); }
	}

	checkJniError(env);

	if (packageName) { env->DeleteLocalRef(packageName); }
	if (packageManager) { env->DeleteLocalRef(packageManager); }

	if (!hasClass) {
		env->DeleteLocalRef(cl);
	}

	return ret;
}

jstring ClassLoader::getCodeCachePath(JNIEnv *env, jobject context, jclass cl) {
	jstring codeCachePath = nullptr;
	bool hasClass = (cl != nullptr);
	if (!cl) {
		cl = env->GetObjectClass(context);
	}

	auto getCodeCacheDirMethod = env->GetMethodID(cl, "getCodeCacheDir", "()Ljava/io/File;");

	auto codeCacheDir = env->CallObjectMethod(context, getCodeCacheDirMethod);
	auto fileClass = env->GetObjectClass(codeCacheDir);
	auto getAbsolutePathMethod = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");

	codeCachePath = reinterpret_cast<jstring>(env->CallObjectMethod(codeCacheDir, getAbsolutePathMethod));

	if (codeCacheDir) { env->DeleteLocalRef(codeCacheDir); }
	if (fileClass) { env->DeleteLocalRef(fileClass); }

	if (!hasClass) {
		env->DeleteLocalRef(cl);
	}

	return codeCachePath;
}

}

#endif
