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

#include "XLPlatformAndroid.h"

#if ANDROID

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static std::mutex s_dataMutex;

static String Activity_getApplicatioName(JNIEnv *env, jclass activityClass, jobject activity) {
	static jmethodID j_getApplicationInfo = nullptr;
	static jfieldID j_labelRes = nullptr;
	static jfieldID j_nonLocalizedLabel = nullptr;
	static jmethodID j_toString = nullptr;
	static jmethodID j_getString = nullptr;

	String ret;
	if (!j_getApplicationInfo) {
		j_getApplicationInfo = env->GetMethodID(activityClass, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
	}

	if (!j_getApplicationInfo) {
		checkJniError(env);
		return String();
	}

	auto jAppInfo = env->CallObjectMethod(activity, j_getApplicationInfo);
	if (!jAppInfo) {
		checkJniError(env);
		return String();
	}

	auto jAppInfo_class = env->GetObjectClass(jAppInfo);
	//auto jAppInfo_class = _activity->env->FindClass("android/content/pm/ApplicationInfo");
	if (!j_labelRes) {
		j_labelRes = env->GetFieldID(jAppInfo_class, "labelRes", "I");
	}

	if (!j_nonLocalizedLabel) {
		j_nonLocalizedLabel = env->GetFieldID(jAppInfo_class, "nonLocalizedLabel", "Ljava/lang/CharSequence;");
	}

	auto labelRes = env->GetIntField(jAppInfo, j_labelRes);
	if (labelRes == 0) {
		auto jNonLocalizedLabel = env->GetObjectField(jAppInfo, j_nonLocalizedLabel);
		jclass clzCharSequence = env->GetObjectClass(jNonLocalizedLabel);
		if (!j_toString) {
			j_toString = env->GetMethodID(clzCharSequence, "toString", "()Ljava/lang/String;");
		}
		jobject s = env->CallObjectMethod(jNonLocalizedLabel, j_toString);
		const char* cstr = env->GetStringUTFChars(static_cast<jstring>(s), NULL);
		ret = cstr;
		env->ReleaseStringUTFChars(static_cast<jstring>(s), cstr);
		env->DeleteLocalRef(s);
		env->DeleteLocalRef(jNonLocalizedLabel);
	} else {
		if (!j_getString) {
			j_getString = env->GetMethodID(activityClass, "getString", "(I)Ljava/lang/String;");
		}
		auto jAppName = static_cast<jstring>(env->CallObjectMethod(activity, j_getString, labelRes));
		const char* cstr = env->GetStringUTFChars(jAppName, NULL);
		ret = cstr;
		env->ReleaseStringUTFChars(jAppName, cstr);
		env->DeleteLocalRef(jAppName);
	}
	env->DeleteLocalRef(jAppInfo);
	return ret;
}

static String Activity_getApplicatioVersion(JNIEnv *env, jclass activityClass, jobject activity, jstring jPackageName) {
	static jmethodID j_getPackageManager = nullptr;
	static jmethodID j_getPackageInfo = nullptr;
	static jfieldID j_versionName = nullptr;

	String ret;
	if (!j_getPackageManager) {
		j_getPackageManager = env->GetMethodID(activityClass, "getPackageManager", "()Landroid/content/pm/PackageManager;");
	}
	if (!j_getPackageManager) {
		checkJniError(env);
		return String();
	}

	auto jpm = env->CallObjectMethod(activity, j_getPackageManager);
	if (!jpm) {
		checkJniError(env);
		return String();
	}

	auto jpm_class = env->GetObjectClass(jpm);
	//auto jpm_class = env->FindClass("android/content/pm/PackageManager");
	if (!j_getPackageInfo) {
		j_getPackageInfo = env->GetMethodID(jpm_class, "getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");
	}
	if (!j_getPackageInfo) {
		checkJniError(env);
		return String();
	}

	auto jinfo = env->CallObjectMethod(jpm, j_getPackageInfo, jPackageName, 0);
	if (!jinfo) {
		checkJniError(env);
		return String();
	}

	auto jinfo_class = env->GetObjectClass(jinfo);
	//auto jinfo_class = env->FindClass("android/content/pm/PackageInfo");
	if (!j_versionName) {
		j_versionName = env->GetFieldID(jinfo_class, "versionName", "Ljava/lang/String;");
	}
	if (!j_versionName) {
		checkJniError(env);
		return String();
	}

	auto jversion = reinterpret_cast<jstring>(env->GetObjectField(jinfo, j_versionName));
	if (!jversion) {
		checkJniError(env);
		return String();
	}

	auto versionChars = env->GetStringUTFChars(jversion, NULL);
	ret = versionChars;
	env->ReleaseStringUTFChars(jversion, versionChars);

	env->DeleteLocalRef(jversion);
	env->DeleteLocalRef(jinfo);
	env->DeleteLocalRef(jpm);
	return ret;
}

static String Activity_getSystemAgent(JNIEnv *env) {
	static jmethodID j_getProperty = nullptr;
	String ret;
	auto systemClass = env->FindClass("java/lang/System");
	if (systemClass) {
		if (!j_getProperty) {
			j_getProperty = env->GetStaticMethodID(systemClass, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
		}
		if (j_getProperty) {
			auto jPropRequest = env->NewStringUTF("http.agent");
			auto jUserAgent = static_cast<jstring>(env->CallStaticObjectMethod(systemClass, j_getProperty, jPropRequest));
			if (jUserAgent) {
				const char* cstr = env->GetStringUTFChars(jUserAgent, NULL);
				ret = cstr;
				env->ReleaseStringUTFChars(jUserAgent, cstr);
				env->DeleteLocalRef(jUserAgent);
			} else {
				checkJniError(env);
			}
		} else {
			checkJniError(env);
		}
	} else {
		checkJniError(env);
	}
	return ret;
}

static String Activity_getUserAgent(JNIEnv *env, jobject activity) {
	static jmethodID j_getDefaultUserAgent = nullptr;
	String ret;
	auto settingsClass = env->FindClass("android/webkit/WebSettings");
	if (settingsClass) {
		if (!j_getDefaultUserAgent) {
			j_getDefaultUserAgent = env->GetStaticMethodID(settingsClass, "getDefaultUserAgent", "(Landroid/content/Context;)Ljava/lang/String;");
		}
		if (j_getDefaultUserAgent) {
			auto jUserAgent = static_cast<jstring>(env->CallStaticObjectMethod(settingsClass, j_getDefaultUserAgent, activity));
			const char* cstr = env->GetStringUTFChars(jUserAgent, NULL);
			ret = cstr;
			env->ReleaseStringUTFChars(jUserAgent, cstr);
			env->DeleteLocalRef(jUserAgent);
		} else {
			checkJniError(env);
		}
	} else {
		checkJniError(env);
	}
	return ret;
}

ActivityInfo ActivityInfo::get(AConfiguration *conf, JNIEnv *env, jclass activityClass, jobject activity, const ActivityInfo *prev) {
	static jmethodID j_getPackageName = nullptr;
	static jmethodID j_getResources = nullptr;
	static jmethodID j_getDisplayMetrics = nullptr;
	static jfieldID j_density = nullptr;
	static jfieldID j_heightPixels = nullptr;
	static jfieldID j_widthPixels = nullptr;

	ActivityInfo activityInfo;

	if (prev) {
		activityInfo = *prev;
	}

	if (!prev || prev->bundleName.empty() || prev->applicationName.empty()
			|| prev->applicationVersion.empty() || prev->systemAgent.empty() || prev->userAgent.empty()) {
		do {
			if (!j_getPackageName) {
				j_getPackageName = env->GetMethodID(activityClass, "getPackageName", "()Ljava/lang/String;");
			}

			if (!j_getPackageName) {
				break;
			}

			jstring jPackageName = reinterpret_cast<jstring>(env->CallObjectMethod(activity, j_getPackageName));
			if (!jPackageName) {
				break;
			}

			if (activityInfo.bundleName.empty()) {
				auto chars = env->GetStringUTFChars(jPackageName, NULL);
				activityInfo.bundleName = chars;
				env->ReleaseStringUTFChars(jPackageName, chars);
			}

			if (activityInfo.applicationName.empty()) {
				activityInfo.applicationName = Activity_getApplicatioName(env, activityClass, activity);
			}
			if (activityInfo.applicationVersion.empty()) {
				activityInfo.applicationVersion = Activity_getApplicatioVersion(env, activityClass, activity, jPackageName);
			}
			if (activityInfo.systemAgent.empty()) {
				activityInfo.systemAgent = Activity_getSystemAgent(env);
			}
			if (activityInfo.userAgent.empty()) {
				activityInfo.userAgent = Activity_getUserAgent(env, activity);
			}

			env->DeleteLocalRef(jPackageName);
		} while (0);
	}

	int widthPixels = 0;
	int heightPixels = 0;
	float density = nan();
	if (!j_getResources) {
		j_getResources = env->GetMethodID(activityClass, "getResources", "()Landroid/content/res/Resources;");
	}

	if (jobject resObj = reinterpret_cast<jstring>(env->CallObjectMethod(activity, j_getResources))) {
		if (!j_getDisplayMetrics) {
			j_getDisplayMetrics = env->GetMethodID(env->GetObjectClass(resObj), "getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
		}
		if (jobject dmObj = reinterpret_cast<jstring>(env->CallObjectMethod(resObj, j_getDisplayMetrics))) {
			auto dmClass = env->GetObjectClass(dmObj);
			if (!j_density) {
				j_density = env->GetFieldID(dmClass, "density", "F");
			}
			if (!j_heightPixels) {
				j_heightPixels = env->GetFieldID(dmClass, "heightPixels", "I");
			}
			if (!j_widthPixels) {
				j_widthPixels = env->GetFieldID(dmClass, "widthPixels", "I");
			}
			density = env->GetFloatField(dmObj, j_density);
			heightPixels = env->GetIntField(dmObj, j_heightPixels);
			widthPixels = env->GetIntField(dmObj, j_widthPixels);
		}
	}

	String language = "en-us";
	AConfiguration_getLanguage(conf, language.data());
	AConfiguration_getCountry(conf, language.data() + 3);
	language = string::tolower<Interface>(language);
	activityInfo.locale = language;

	if (isnan(density)) {
		auto densityValue = AConfiguration_getDensity(conf);
		switch (densityValue) {
			case ACONFIGURATION_DENSITY_LOW: density = 0.75f; break;
			case ACONFIGURATION_DENSITY_MEDIUM: density = 1.0f; break;
			case ACONFIGURATION_DENSITY_TV: density = 1.5f; break;
			case ACONFIGURATION_DENSITY_HIGH: density = 1.5f; break;
			case 280: density = 2.0f; break;
			case ACONFIGURATION_DENSITY_XHIGH: density = 2.0f; break;
			case 360: density = 3.0f; break;
			case 400: density = 3.0f; break;
			case 420: density = 3.0f; break;
			case ACONFIGURATION_DENSITY_XXHIGH: density = 3.0f; break;
			case 560: density = 4.0f; break;
			case ACONFIGURATION_DENSITY_XXXHIGH: density = 4.0f; break;
			default: break;
		}
	}

	activityInfo.density = density;

	int32_t orientation = AConfiguration_getOrientation(conf);
	int32_t width = AConfiguration_getScreenWidthDp(conf);
	int32_t height = AConfiguration_getScreenHeightDp(conf);

	switch (orientation) {
	case ACONFIGURATION_ORIENTATION_ANY:
	case ACONFIGURATION_ORIENTATION_SQUARE:
		activityInfo.sizeInPixels = Extent2(widthPixels, heightPixels);
		activityInfo.sizeInDp = Size2(widthPixels / density, heightPixels / density);
		break;
	case ACONFIGURATION_ORIENTATION_PORT:
		activityInfo.sizeInPixels = Extent2(std::min(widthPixels, heightPixels), std::max(widthPixels, heightPixels));
		activityInfo.sizeInDp = Size2(std::min(widthPixels, heightPixels) / density, std::max(widthPixels, heightPixels) / density);
		break;
	case ACONFIGURATION_ORIENTATION_LAND:
		activityInfo.sizeInPixels = Extent2(std::max(widthPixels, heightPixels), std::min(widthPixels, heightPixels));
		activityInfo.sizeInDp = Size2(std::max(widthPixels, heightPixels) / density, std::min(widthPixels, heightPixels) / density);
		break;
	}

	return activityInfo;
}

void checkJniError(JNIEnv* env) {
	if (env->ExceptionCheck()) {
		// Read exception msg
		jthrowable e = env->ExceptionOccurred();
		env->ExceptionClear(); // clears the exception; e seems to remain valid
		jclass clazz = env->GetObjectClass(e);
		jclass classClass = env->GetObjectClass(clazz);
		jmethodID getName = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");
		jmethodID getMessage = env->GetMethodID(clazz, "getMessage", "()Ljava/lang/String;");
		jstring message = reinterpret_cast<jstring>(env->CallObjectMethod(e, getMessage));
		jstring exName = reinterpret_cast<jstring>(env->CallObjectMethod(clazz, getName));

		const char *mstr = env->GetStringUTFChars(message, NULL);
		log::error("JNI", "[", env->GetStringUTFChars(exName, NULL), "] ", mstr);

		env->ReleaseStringUTFChars(message, mstr);
		env->DeleteLocalRef(message);
		env->DeleteLocalRef(exName);
		env->DeleteLocalRef(classClass);
		env->DeleteLocalRef(clazz);
		env->DeleteLocalRef(e);
	}
}

void saveApplicationInfo(const Value &value) {
	std::unique_lock lock(s_dataMutex);
	filesystem::mkdir(filesystem::documentsPath<Interface>());
	auto path = filesystem::documentsPath<Interface>("application.cbor");
	data::save(value, path, data::EncodeFormat::CborCompressed);
}

Value loadApplicationInfo() {
	std::unique_lock lock(s_dataMutex);
	auto path = filesystem::documentsPath<Interface>("application.cbor");
	return data::readFile<Interface>(path);
}

void saveMessageToken(StringView tok) {
	std::unique_lock lock(s_dataMutex);
	filesystem::mkdir(filesystem::documentsPath<Interface>());
	auto path = filesystem::documentsPath<Interface>("token.cbor");
	data::save(Value({
		pair("token", Value(tok))
	}), path, data::EncodeFormat::CborCompressed);
}

String loadMessageToken() {
	std::unique_lock lock(s_dataMutex);
	auto path = filesystem::documentsPath<Interface>("token.cbor");
	auto d = data::readFile<Interface>(path);
	return d.getString("token");
}

}

#endif
