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

static String Activity_getApplicatioName(const jni::RefClass &activityClass, const jni::Ref &activity) {
	static jmethodID j_getApplicationInfo = nullptr;
	static jfieldID j_labelRes = nullptr;
	static jfieldID j_nonLocalizedLabel = nullptr;
	static jmethodID j_toString = nullptr;
	static jmethodID j_getString = nullptr;

	String ret;
	if (!j_getApplicationInfo) {
		j_getApplicationInfo = activityClass.getMethodID("getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
	}

	if (!j_getApplicationInfo) {
		return String();
	}

	auto jAppInfo = activity.callMethod<jobject>(j_getApplicationInfo);
	if (!jAppInfo) {
		return String();
	}

	auto jAppInfo_class = jAppInfo.getClass();
	if (!j_labelRes) {
		j_labelRes = jAppInfo_class.getFieldID("labelRes", "I");
	}

	if (!j_nonLocalizedLabel) {
		j_nonLocalizedLabel = jAppInfo_class.getFieldID("nonLocalizedLabel", "Ljava/lang/CharSequence;");
	}

	auto labelRes = jAppInfo.getField<jint>(j_labelRes);
	if (labelRes == 0) {
		auto jNonLocalizedLabel = jAppInfo.getField<jobject>(j_nonLocalizedLabel);
		auto clzCharSequence = jNonLocalizedLabel.getClass();
		if (!j_toString) {
			j_toString = clzCharSequence.getMethodID("toString", "()Ljava/lang/String;");
		}
		auto s = jNonLocalizedLabel.callMethod<jstring>(j_toString);
		ret = s.getString().str<Interface>();
	} else {
		if (!j_getString) {
			j_getString = activityClass.getMethodID("getString", "(I)Ljava/lang/String;");
		}
		auto jAppName = activity.callMethod<jstring>(j_getString, labelRes);
		ret = jAppName.getString().str<Interface>();
	}
	return ret;
}

static String Activity_getApplicatioVersion(const jni::RefClass &activityClass,
		const jni::Ref &activity, const jni::RefString &jPackageName) {
	static jmethodID j_getPackageManager = nullptr;
	static jmethodID j_getPackageInfo = nullptr;
	static jfieldID j_versionName = nullptr;

	String ret;
	if (!j_getPackageManager) {
		j_getPackageManager = activityClass.getMethodID("getPackageManager", "()Landroid/content/pm/PackageManager;");
	}
	if (!j_getPackageManager) {
		return String();
	}

	auto jpm = activity.callMethod<jobject>(j_getPackageManager);
	if (!jpm) {
		return String();
	}

	auto jpm_class = jpm.getClass();
	if (!j_getPackageInfo) {
		j_getPackageInfo = jpm_class.getMethodID("getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");
	}

	if (!j_getPackageInfo) {
		return String();
	}

	auto jinfo = jpm.callMethod<jobject>(j_getPackageInfo, jPackageName, 0);
	if (!jinfo) {
		return String();
	}

	auto jinfo_class = jinfo.getClass();
	if (!j_versionName) {
		j_versionName = jinfo_class.getFieldID("versionName", "Ljava/lang/String;");
	}
	if (!j_versionName) {
		return String();
	}

	auto jversion = jinfo.getField<jstring>(j_versionName);
	if (!jversion) {
		return String();
	}

	return jversion.getString().str<Interface>();
}

static String Activity_getSystemAgent(const jni::Env &env) {
	static jmethodID j_getProperty = nullptr;
	auto systemClass = env.findClass("java/lang/System");
	if (systemClass) {
		if (!j_getProperty) {
			j_getProperty = systemClass.getStaticMethodID("getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
		}
		if (j_getProperty) {
			auto jUserAgent = systemClass.callStaticMethod<jstring>(j_getProperty, env.newString("http.agent"));
			if (jUserAgent) {
				return jUserAgent.getString().str<Interface>();
			} else {
				env.checkErrors();
			}
		} else {
			env.checkErrors();
		}
	} else {
		env.checkErrors();
	}
	return String();
}

static String Activity_getUserAgent(const jni::Env &env, const jni::Ref &activity) {
	static jmethodID j_getDefaultUserAgent = nullptr;
	auto settingsClass = env.findClass("android/webkit/WebSettings");
	if (settingsClass) {
		if (!j_getDefaultUserAgent) {
			j_getDefaultUserAgent = settingsClass.getStaticMethodID("getDefaultUserAgent", "(Landroid/content/Context;)Ljava/lang/String;");
		}
		if (j_getDefaultUserAgent) {
			auto jUserAgent = settingsClass.callStaticMethod<jstring>(j_getDefaultUserAgent, activity);
			return jUserAgent.getString().str<Interface>();
		} else {
			env.checkErrors();
		}
	} else {
		env.checkErrors();
	}
	return String();
}

ActivityInfo ActivityInfo::get(AConfiguration *conf, const jni::Env &env, const jni::RefClass &activityClass,
		const jni::Ref &activity, const ActivityInfo *prev) {
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
				j_getPackageName = activityClass.getMethodID("getPackageName", "()Ljava/lang/String;");
			}

			if (!j_getPackageName) {
				break;
			}

			auto jPackageName = activity.callMethod<jstring>(j_getPackageName);
			if (!jPackageName) {
				break;
			}

			if (activityInfo.bundleName.empty()) {
				activityInfo.bundleName = jPackageName.getString().str<Interface>();
			}
			if (activityInfo.applicationName.empty()) {
				activityInfo.applicationName = Activity_getApplicatioName(activityClass, activity);
			}
			if (activityInfo.applicationVersion.empty()) {
				activityInfo.applicationVersion = Activity_getApplicatioVersion(activityClass, activity, jPackageName);
			}
			if (activityInfo.systemAgent.empty()) {
				activityInfo.systemAgent = Activity_getSystemAgent(env);
			}
			if (activityInfo.userAgent.empty()) {
				activityInfo.userAgent = Activity_getUserAgent(env, activity);
			}
		} while (0);
	}

	int widthPixels = 0;
	int heightPixels = 0;
	float displayDensity = nan();
	if (!j_getResources) {
		j_getResources =activityClass.getMethodID("getResources", "()Landroid/content/res/Resources;");
	}

	if (auto resObj = activity.callMethod<jobject>(j_getResources)) {
		if (!j_getDisplayMetrics) {
			j_getDisplayMetrics = resObj.getClass().getMethodID("getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
		}
		if (auto dmObj = resObj.callMethod<jobject>(j_getDisplayMetrics)) {
			auto dmClass = dmObj.getClass();
			if (!j_density) {
				j_density = dmClass.getFieldID("density", "F");
			}
			if (!j_heightPixels) {
				j_heightPixels = dmClass.getFieldID("heightPixels", "I");
			}
			if (!j_widthPixels) {
				j_widthPixels = dmClass.getFieldID("widthPixels", "I");
			}
			displayDensity = dmObj.getField<float>(j_density);
			heightPixels = dmObj.getField<jint>(j_heightPixels);
			widthPixels = dmObj.getField<jint>(j_widthPixels);
		}
	}

	String language = "en-us";
	AConfiguration_getLanguage(conf, language.data());
	AConfiguration_getCountry(conf, language.data() + 3);
	language = string::tolower<Interface>(language);
	activityInfo.locale = language;

	if (isnan(displayDensity)) {
		auto densityValue = AConfiguration_getDensity(conf);
		switch (densityValue) {
			case ACONFIGURATION_DENSITY_LOW: displayDensity = 0.75f; break;
			case ACONFIGURATION_DENSITY_MEDIUM: displayDensity = 1.0f; break;
			case ACONFIGURATION_DENSITY_TV: displayDensity = 1.5f; break;
			case ACONFIGURATION_DENSITY_HIGH: displayDensity = 1.5f; break;
			case 280: displayDensity = 2.0f; break;
			case ACONFIGURATION_DENSITY_XHIGH: displayDensity = 2.0f; break;
			case 360: displayDensity = 3.0f; break;
			case 400: displayDensity = 3.0f; break;
			case 420: displayDensity = 3.0f; break;
			case ACONFIGURATION_DENSITY_XXHIGH: displayDensity = 3.0f; break;
			case 560: displayDensity = 4.0f; break;
			case ACONFIGURATION_DENSITY_XXXHIGH: displayDensity = 4.0f; break;
			default: break;
		}
	}

	activityInfo.density = displayDensity;

	int32_t orientation = AConfiguration_getOrientation(conf);
	int32_t width = AConfiguration_getScreenWidthDp(conf);
	int32_t height = AConfiguration_getScreenHeightDp(conf);

	switch (orientation) {
	case ACONFIGURATION_ORIENTATION_ANY:
	case ACONFIGURATION_ORIENTATION_SQUARE:
		activityInfo.sizeInPixels = Extent2(widthPixels, heightPixels);
		activityInfo.sizeInDp = Size2(widthPixels / displayDensity, heightPixels / displayDensity);
		break;
	case ACONFIGURATION_ORIENTATION_PORT:
		activityInfo.sizeInPixels = Extent2(std::min(widthPixels, heightPixels), std::max(widthPixels, heightPixels));
		activityInfo.sizeInDp = Size2(std::min(widthPixels, heightPixels) / displayDensity, std::max(widthPixels, heightPixels) / displayDensity);
		break;
	case ACONFIGURATION_ORIENTATION_LAND:
		activityInfo.sizeInPixels = Extent2(std::max(widthPixels, heightPixels), std::min(widthPixels, heightPixels));
		activityInfo.sizeInDp = Size2(std::max(widthPixels, heightPixels) / displayDensity, std::min(widthPixels, heightPixels) / displayDensity);
		break;
	}

	return activityInfo;
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
