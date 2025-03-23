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

#include "XLPlatformAndroidClassLoader.h"

#if ANDROID

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

ClassLoader::ClassLoader() { }

ClassLoader::~ClassLoader() { }

bool ClassLoader::init(ANativeActivity *activity, int32_t sdk) {
	sdkVersion = sdk;

	auto thiz = jni::Ref(activity->clazz, activity->env);
	auto env = jni::Env(activity->env);

	auto activityClass = thiz.getClass();
	auto classClass = activityClass.getClass();
	auto getClassLoaderMethod = classClass.getMethodID("getClassLoader", "()Ljava/lang/ClassLoader;");

	auto path = stappler::filesystem::platform::Android_getApkPath();
	auto codeCachePath = getCodeCachePath(thiz, activityClass);
	auto paths = getNativePaths(thiz, activityClass);

	apkPath = paths.apkPath.getString().str<Interface>();
	nativeLibraryDir = paths.nativeLibraryDir.getString().str<Interface>();

	filesystem::ftw(nativeLibraryDir, [] (StringView path, bool isFile) {
		if (isFile) {
			log::info("NativeClassLoader", path);
		}
	});

	auto classLoader = activityClass.callMethod<jobject>(getClassLoaderMethod);

	if (!codeCachePath || !paths.apkPath) {
		env.checkErrors();
		return false;
	}

	if (classLoader) {
		activityClassLoader = classLoader;
		activityClassLoaderClass = activityClassLoader.getClass();

		auto className = activityClassLoaderClass.getClassName();

		log::info("JNI", "Activity: ClassLoader: ", className.getString());
		if (className.getString() == StringView("java.lang.BootClassLoader")) {
			// acquire new dex class loader
			auto dexClassLoaderClass = env.findClass("dalvik/system/DexClassLoader");
			if (!dexClassLoaderClass) {
				env.checkErrors();
				return false;
			}

			jmethodID dexClassLoaderConstructor = dexClassLoaderClass.getMethodID("<init>",
					"(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");

			auto dexLoader = env.newObject(dexClassLoaderClass, dexClassLoaderConstructor,
					paths.apkPath, codeCachePath, paths.nativeLibraryDir, activityClassLoader);
			if (dexLoader) {
				apkClassLoader = dexLoader;
				apkClassLoaderClass = apkClassLoader.getClass();

				findClassMethod = apkClassLoaderClass.getMethodID("loadClass", "(Ljava/lang/String;Z)Ljava/lang/Class;");
				loaderClassClass = apkClassLoaderClass.getClass();

				getMethodsMethod = loaderClassClass.getMethodID("getMethods", "()[Ljava/lang/reflect/Method;");
				getFieldsMethod = loaderClassClass.getMethodID("getFields", "()[Ljava/lang/reflect/Field;");
			} else {
				env.checkErrors();
				return false;
			}
		} else {
			apkClassLoader = activityClassLoader;
			apkClassLoaderClass = activityClassLoaderClass;
			loaderClassClass = classClass;
			findClassMethod = activityClassLoaderClass.getMethodID("loadClass", "(Ljava/lang/String;Z)Ljava/lang/Class;");
			getMethodsMethod = loaderClassClass.getMethodID("getMethods", "()[Ljava/lang/reflect/Method;");
			getFieldsMethod = loaderClassClass.getMethodID("getFields", "()[Ljava/lang/reflect/Field;");
		}
	}

	if (loaderClassClass) {
		getClassNameMethod = loaderClassClass.getMethodID("getName", "()Ljava/lang/String;");
	}

	if (auto methodClass = findClass(env, "java/lang/reflect/Method")) {
		getMethodNameMethod = methodClass.getMethodID("getName", "()Ljava/lang/String;");
	}

	if (auto fieldClass = findClass(env, "java/lang/reflect/Field")) {
		getFieldNameMethod = fieldClass.getMethodID("getName", "()Ljava/lang/String;");
		getFieldTypeMethod = fieldClass.getMethodID("getType", "()Ljava/lang/Class;");
		getFieldIntMethod = fieldClass.getMethodID("getInt", "(Ljava/lang/Object;)I");
	}

	env.checkErrors();
	return true;
}

void ClassLoader::finalize() {
	activityClassLoader = nullptr;
	activityClassLoaderClass = nullptr;
	apkClassLoader = nullptr;
	apkClassLoaderClass = nullptr;
	findClassMethod = nullptr;
}

void ClassLoader::foreachMethod(const jni::RefClass &cl, const Callback<void(StringView, const jni::Ref &)> &cb) {
	jobjectArray jobjArray = static_cast<jobjectArray>(cl.getEnv()->CallObjectMethod(cl, getMethodsMethod));
	jsize len = cl.getEnv()->GetArrayLength(jobjArray);

	for (jsize i = 0 ; i < len ; i++) {
		auto methodObject = jni::Local(cl.getEnv()->GetObjectArrayElement(jobjArray, i), cl.getEnv());
		auto j_name = methodObject.callMethod<jstring>(getMethodNameMethod);

		cb(j_name.getString(), methodObject);
	}

	cl.getEnv()->DeleteLocalRef(jobjArray);
}

void ClassLoader::foreachField(const jni::RefClass &cl, const Callback<void(StringView, StringView, const jni::Ref &)> &cb) {
	jobjectArray jobjArray = static_cast<jobjectArray>(cl.getEnv()->CallObjectMethod(cl, getFieldsMethod));
	jsize len = cl.getEnv()->GetArrayLength(jobjArray);

	for (jsize i = 0 ; i < len ; i++) {
		auto fieldObject = jni::Local(cl.getEnv()->GetObjectArrayElement(jobjArray, i), cl.getEnv());
		auto fieldType = fieldObject.callMethod<jobject>(getFieldTypeMethod);

		auto j_type = fieldType.callMethod<jstring>(getClassNameMethod);
		auto j_name = fieldObject.callMethod<jstring>(getFieldNameMethod);

		cb(j_type.getString(), j_name.getString(), fieldObject);
	}

	cl.getEnv()->DeleteLocalRef(jobjArray);
}

int ClassLoader::getIntField(const jni::Ref &origin, const jni::Ref &field) {
	return field.callMethod<jint>(getFieldIntMethod, origin);
}

jni::LocalClass ClassLoader::findClass(const jni::Env &env, StringView data) {
	return findClass(env.newString(data));
}

jni::LocalClass ClassLoader::findClass(const jni::RefString &str) {
	return apkClassLoader.callMethod<jclass>(findClassMethod, str, jboolean(1));
}

ClassLoader::NativePaths ClassLoader::getNativePaths(const jni::Ref &context, const jni::RefClass &icl) {
	NativePaths ret;
	jni::RefClass cl = icl;
	jni::LocalClass tmpClass;
	if (!cl) {
		tmpClass = context.getClass();
		cl = tmpClass;
	}

	auto getPackageNameMethod = cl.getMethodID("getPackageName", "()Ljava/lang/String;");
	auto getPackageManagerMethod = cl.getMethodID("getPackageManager", "()Landroid/content/pm/PackageManager;");

	auto packageName = context.callMethod<jstring>(getPackageNameMethod);
	auto packageManager = context.callMethod<jobject>(getPackageManagerMethod);

	if (packageName && packageManager) {
		auto packageManagerClass = packageManager.getClass();
		auto getApplicationInfoMethod = packageManagerClass.getMethodID("getApplicationInfo",
				"(Ljava/lang/String;I)Landroid/content/pm/ApplicationInfo;");

		auto applicationInfo = packageManager.callMethod<jobject>(getApplicationInfoMethod, packageName, 0);

		if (applicationInfo) {
			auto applicationInfoClass = applicationInfo.getClass();
			auto publicSourceDirField = applicationInfoClass.getFieldID("publicSourceDir", "Ljava/lang/String;");
			auto nativeLibraryDirField = applicationInfoClass.getFieldID("nativeLibraryDir", "Ljava/lang/String;");

			ret.apkPath = applicationInfo.getField<jstring>(publicSourceDirField);
			ret.nativeLibraryDir = applicationInfo.getField<jstring>(nativeLibraryDirField);
		}
	}
	return ret;
}

jni::LocalString ClassLoader::getCodeCachePath(const jni::Ref &context, const jni::RefClass &icl) {
	jni::RefClass cl = icl;
	jni::LocalClass tmpClass;
	if (!cl) {
		tmpClass = context.getClass();
		cl = tmpClass;
	}

	auto getCodeCacheDirMethod = cl.getMethodID("getCodeCacheDir", "()Ljava/io/File;");

	auto codeCacheDir = context.callMethod<jobject>(getCodeCacheDirMethod);
	auto fileClass = codeCacheDir.getClass();
	auto getAbsolutePathMethod = fileClass.getMethodID("getAbsolutePath", "()Ljava/lang/String;");

	return codeCacheDir.callMethod<jstring>(getAbsolutePathMethod);
}

}

#endif
