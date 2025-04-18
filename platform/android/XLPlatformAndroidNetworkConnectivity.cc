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

#include "XLPlatformAndroidNetworkConnectivity.h"
#include "XLPlatformAndroidClassLoader.h"
#include "XLPlatformAndroidActivity.h"

#if ANDROID

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static int flag_NET_CAPABILITY_INTERNET = 0;
static int flag_NET_CAPABILITY_NOT_CONGESTED = 0;
static int flag_NET_CAPABILITY_NOT_METERED = 0;
static int flag_NET_CAPABILITY_NOT_RESTRICTED = 0;
static int flag_NET_CAPABILITY_NOT_ROAMING = 0;
static int flag_NET_CAPABILITY_NOT_SUSPENDED = 0;
static int flag_NET_CAPABILITY_NOT_VPN = 0;
static int flag_NET_CAPABILITY_PRIORITIZE_BANDWIDTH = 0;
static int flag_NET_CAPABILITY_PRIORITIZE_LATENCY = 0;
static int flag_NET_CAPABILITY_TEMPORARILY_NOT_METERED = 0;
static int flag_NET_CAPABILITY_TRUSTED = 0;
static int flag_NET_CAPABILITY_VALIDATED = 0;
static int flag_NET_CAPABILITY_WIFI_P2P = 0;

static void NetworkConnectivity_nativeOnCreated(JNIEnv *env, jobject thiz, jlong nativePointer, jobject caps, jobject props) {
	auto native = reinterpret_cast<NetworkConnectivity *>(nativePointer);
	native->handleCreated(env, caps, props);
}

static void NetworkConnectivity_nativeOnFinalized(JNIEnv *env, jobject thiz, jlong nativePointer) {
	auto native = reinterpret_cast<NetworkConnectivity *>(nativePointer);
	native->handleFinalized(env);
}

static void NetworkConnectivity_nativeOnAvailable(JNIEnv *env, jobject thiz, jlong nativePointer, jobject caps, jobject props) {
	auto native = reinterpret_cast<NetworkConnectivity *>(nativePointer);
	native->handleAvailable(env, caps, props);
}

static void NetworkConnectivity_nativeOnLost(JNIEnv *env, jobject thiz, jlong nativePointer) {
	auto native = reinterpret_cast<NetworkConnectivity *>(nativePointer);
	native->handleLost(env);
}

static void NetworkConnectivity_nativeOnCapabilitiesChanged(JNIEnv *env, jobject thiz, jlong nativePointer, jobject caps) {
	auto native = reinterpret_cast<NetworkConnectivity *>(nativePointer);
	native->handleCapabilitiesChanged(env, caps);
}

static void NetworkConnectivity_nativeOnLinkPropertiesChanged(JNIEnv *env, jobject thiz, jlong nativePointer, jobject props) {
	auto native = reinterpret_cast<NetworkConnectivity *>(nativePointer);
	native->handleLinkPropertiesChanged(env, props);
}

static JNINativeMethod methods[] = {
	{"nativeOnCreated","(JLandroid/net/NetworkCapabilities;Landroid/net/LinkProperties;)V", reinterpret_cast<void *>(&NetworkConnectivity_nativeOnCreated)},
	{"nativeOnFinalized","(J)V", reinterpret_cast<void *>(&NetworkConnectivity_nativeOnFinalized)},
	{"nativeOnAvailable", "(JLandroid/net/NetworkCapabilities;Landroid/net/LinkProperties;)V", reinterpret_cast<void *>(&NetworkConnectivity_nativeOnAvailable)},
	{"nativeOnLost", "(J)V", reinterpret_cast<void *>(&NetworkConnectivity_nativeOnLost)},
	{"nativeOnCapabilitiesChanged", "(JLandroid/net/NetworkCapabilities;)V", reinterpret_cast<void *>(&NetworkConnectivity_nativeOnCapabilitiesChanged)},
	{"nativeOnLinkPropertiesChanged", "(JLandroid/net/LinkProperties;)V", reinterpret_cast<void *>(&NetworkConnectivity_nativeOnLinkPropertiesChanged)},
};

static void loadCapabilitiesFlags(JNIEnv *env, jclass cl, int32_t sdk) {
	if (sdk >= 21) {
		jfieldID id_NET_CAPABILITY_INTERNET = env->GetStaticFieldID(cl, "NET_CAPABILITY_INTERNET", "I");
		flag_NET_CAPABILITY_INTERNET = env->GetStaticIntField(cl, id_NET_CAPABILITY_INTERNET);

		jfieldID id_NET_CAPABILITY_NOT_METERED = env->GetStaticFieldID(cl, "NET_CAPABILITY_NOT_METERED", "I");
		flag_NET_CAPABILITY_NOT_METERED = env->GetStaticIntField(cl, id_NET_CAPABILITY_NOT_METERED);

		jfieldID id_NET_CAPABILITY_NOT_RESTRICTED = env->GetStaticFieldID(cl, "NET_CAPABILITY_NOT_RESTRICTED", "I");
		flag_NET_CAPABILITY_NOT_RESTRICTED = env->GetStaticIntField(cl, id_NET_CAPABILITY_NOT_RESTRICTED);

		jfieldID id_NET_CAPABILITY_NOT_VPN = env->GetStaticFieldID(cl, "NET_CAPABILITY_NOT_VPN", "I");
		flag_NET_CAPABILITY_NOT_VPN = env->GetStaticIntField(cl, id_NET_CAPABILITY_NOT_VPN);

		jfieldID id_NET_CAPABILITY_TRUSTED = env->GetStaticFieldID(cl, "NET_CAPABILITY_TRUSTED", "I");
		flag_NET_CAPABILITY_TRUSTED = env->GetStaticIntField(cl, id_NET_CAPABILITY_TRUSTED);

		jfieldID id_NET_CAPABILITY_WIFI_P2P = env->GetStaticFieldID(cl, "NET_CAPABILITY_WIFI_P2P", "I");
		flag_NET_CAPABILITY_WIFI_P2P = env->GetStaticIntField(cl, id_NET_CAPABILITY_WIFI_P2P);
	}

	if (sdk >= 23) {
		jfieldID id_NET_CAPABILITY_VALIDATED = env->GetStaticFieldID(cl, "NET_CAPABILITY_VALIDATED", "I");
		flag_NET_CAPABILITY_VALIDATED = env->GetStaticIntField(cl, id_NET_CAPABILITY_VALIDATED);
	}

	if (sdk >= 28) {
		jfieldID id_NET_CAPABILITY_NOT_CONGESTED = env->GetStaticFieldID(cl, "NET_CAPABILITY_NOT_CONGESTED", "I");
		flag_NET_CAPABILITY_NOT_CONGESTED = env->GetStaticIntField(cl, id_NET_CAPABILITY_NOT_CONGESTED);

		jfieldID id_NET_CAPABILITY_NOT_ROAMING = env->GetStaticFieldID(cl, "NET_CAPABILITY_NOT_ROAMING", "I");
		flag_NET_CAPABILITY_NOT_ROAMING = env->GetStaticIntField(cl, id_NET_CAPABILITY_NOT_ROAMING);

		jfieldID id_NET_CAPABILITY_NOT_SUSPENDED = env->GetStaticFieldID(cl, "NET_CAPABILITY_NOT_SUSPENDED", "I");
		flag_NET_CAPABILITY_NOT_SUSPENDED = env->GetStaticIntField(cl, id_NET_CAPABILITY_NOT_SUSPENDED);
	}

	if (sdk >= 30) {
		jfieldID id_NET_CAPABILITY_TEMPORARILY_NOT_METERED = env->GetStaticFieldID(cl, "NET_CAPABILITY_TEMPORARILY_NOT_METERED", "I");
		flag_NET_CAPABILITY_TEMPORARILY_NOT_METERED = env->GetStaticIntField(cl, id_NET_CAPABILITY_TEMPORARILY_NOT_METERED);
	}

	if (sdk >= 33) {
		jfieldID id_NET_CAPABILITY_PRIORITIZE_BANDWIDTH = env->GetStaticFieldID(cl, "NET_CAPABILITY_PRIORITIZE_BANDWIDTH", "I");
		flag_NET_CAPABILITY_PRIORITIZE_BANDWIDTH = env->GetStaticIntField(cl, id_NET_CAPABILITY_PRIORITIZE_BANDWIDTH);

		jfieldID id_NET_CAPABILITY_PRIORITIZE_LATENCY = env->GetStaticFieldID(cl, "NET_CAPABILITY_PRIORITIZE_LATENCY", "I");
		flag_NET_CAPABILITY_PRIORITIZE_LATENCY = env->GetStaticIntField(cl, id_NET_CAPABILITY_PRIORITIZE_LATENCY);
	}
}

static NetworkCapabilities readCapabilities(JNIEnv *env, jmethodID hasCapability, jobject caps) {
	NetworkCapabilities ret = NetworkCapabilities::None;
	if (flag_NET_CAPABILITY_INTERNET && env->CallBooleanMethod(caps, hasCapability, flag_NET_CAPABILITY_INTERNET)) {
		ret |= NetworkCapabilities::Internet;
	}
	if (flag_NET_CAPABILITY_NOT_CONGESTED && env->CallBooleanMethod(caps, hasCapability, flag_NET_CAPABILITY_NOT_CONGESTED)) {
		ret |= NetworkCapabilities::NotCongested;
	}
	if (flag_NET_CAPABILITY_NOT_METERED && env->CallBooleanMethod(caps, hasCapability, flag_NET_CAPABILITY_NOT_METERED)) {
		ret |= NetworkCapabilities::NotMetered;
	}
	if (flag_NET_CAPABILITY_NOT_RESTRICTED && env->CallBooleanMethod(caps, hasCapability, flag_NET_CAPABILITY_NOT_RESTRICTED)) {
		ret |= NetworkCapabilities::NotRestricted;
	}
	if (flag_NET_CAPABILITY_NOT_ROAMING && env->CallBooleanMethod(caps, hasCapability, flag_NET_CAPABILITY_NOT_ROAMING)) {
		ret |= NetworkCapabilities::NotRoaming;
	}
	if (flag_NET_CAPABILITY_NOT_SUSPENDED && env->CallBooleanMethod(caps, hasCapability, flag_NET_CAPABILITY_NOT_SUSPENDED)) {
		ret |= NetworkCapabilities::NotSuspended;
	}
	if (flag_NET_CAPABILITY_NOT_VPN && env->CallBooleanMethod(caps, hasCapability, flag_NET_CAPABILITY_NOT_VPN)) {
		ret |= NetworkCapabilities::NotVpn;
	}
	if (flag_NET_CAPABILITY_PRIORITIZE_BANDWIDTH && env->CallBooleanMethod(caps, hasCapability, flag_NET_CAPABILITY_PRIORITIZE_BANDWIDTH)) {
		ret |= NetworkCapabilities::PrioritizeBandwidth;
	}
	if (flag_NET_CAPABILITY_PRIORITIZE_LATENCY && env->CallBooleanMethod(caps, hasCapability, flag_NET_CAPABILITY_PRIORITIZE_LATENCY)) {
		ret |= NetworkCapabilities::PrioritizeLatency;
	}
	if (flag_NET_CAPABILITY_TEMPORARILY_NOT_METERED && env->CallBooleanMethod(caps, hasCapability, flag_NET_CAPABILITY_TEMPORARILY_NOT_METERED)) {
		ret |= NetworkCapabilities::TemporarilyNotMetered;
	}
	if (flag_NET_CAPABILITY_TRUSTED && env->CallBooleanMethod(caps, hasCapability, flag_NET_CAPABILITY_TRUSTED)) {
		ret |= NetworkCapabilities::Trusted;
	}
	if (flag_NET_CAPABILITY_VALIDATED && env->CallBooleanMethod(caps, hasCapability, flag_NET_CAPABILITY_VALIDATED)) {
		ret |= NetworkCapabilities::Validated;
	}
	if (flag_NET_CAPABILITY_WIFI_P2P && env->CallBooleanMethod(caps, hasCapability, flag_NET_CAPABILITY_WIFI_P2P)) {
		ret |= NetworkCapabilities::WifiP2P;
	}
	return ret;
}

bool NetworkConnectivity::init(ClassLoader *classLoader, const jni::Ref &context, Function<void(NetworkCapabilities)> &&cb) {
	sdkVersion = classLoader->sdkVersion;

	jni::Env env = context.getEnv();

	auto networkConnectivityClass = classLoader->findClass(env, Activity::NetworkConnectivityClassName);
	if (networkConnectivityClass) {
		networkConnectivityClass.registerNatives(methods, 6);

		auto capabilitiesClass = env.findClass("android/net/NetworkCapabilities");
		if (capabilitiesClass) {
			loadCapabilitiesFlags(env, capabilitiesClass, sdkVersion);

			j_hasCapability = capabilitiesClass.getMethodID("hasCapability", "(I)Z");
		}

		env.checkErrors();

		auto sig = toString("(Landroid/content/Context;J)L", Activity::NetworkConnectivityClassPath, ";");

		jmethodID networkConnectivityCreate = networkConnectivityClass.getStaticMethodID("create", sig.data());
		if (networkConnectivityCreate) {
			auto conn = networkConnectivityClass.callStaticMethod<jobject>(networkConnectivityCreate, context, jlong(this));
			if (conn) {
				thiz = conn;
				clazz = networkConnectivityClass;
				callback = sp::move(cb);
				if (callback) {
					callback(capabilities);
				}
				return true;
			}
		} else {
			env.checkErrors();
		}
	}
	return false;
}

void NetworkConnectivity::finalize() {
	if (thiz && clazz) {
		jmethodID networkConnectivityFinalize = clazz.getMethodID("finalize", "()V");
		if (networkConnectivityFinalize) {
			thiz.callMethod<void>(networkConnectivityFinalize);
		}
	}

	j_hasCapability = nullptr;
	thiz = nullptr;
	clazz = nullptr;
}

void NetworkConnectivity::handleCreated(JNIEnv *env, jobject caps, jobject props) {
	//log::verbose("NetworkConnectivity", "onCreated");
	if (j_hasCapability && caps) {
		capabilities = readCapabilities(env, j_hasCapability, caps);
		if (callback) {
			callback(capabilities);
		}
	} else if (capabilities != NetworkCapabilities::None) {
		capabilities = NetworkCapabilities::None;
		if (callback) {
			callback(capabilities);
		}
	}
}

void NetworkConnectivity::handleFinalized(JNIEnv *env) {
	//log::verbose("NetworkConnectivity", "onFinalized");
	capabilities = NetworkCapabilities::None;
	callback = nullptr;
}

void NetworkConnectivity::handleAvailable(JNIEnv *env, jobject caps, jobject props) {
	//log::verbose("NetworkConnectivity", "onAvailable");
	if (j_hasCapability && caps) {
		capabilities = readCapabilities(env, j_hasCapability, caps);
		if (callback) {
			callback(capabilities);
		}
	} else if (capabilities != NetworkCapabilities::None) {
		capabilities = NetworkCapabilities::None;
		if (callback) {
			callback(capabilities);
		}
	}
}

void NetworkConnectivity::handleLost(JNIEnv *env) {
	//log::verbose("NetworkConnectivity", "onLost");
	capabilities = NetworkCapabilities::None;
	if (callback) {
		callback(capabilities);
	}
}

void NetworkConnectivity::handleCapabilitiesChanged(JNIEnv *env, jobject caps) {
	//log::verbose("NetworkConnectivity", "onCapabilitiesChanged");
	if (j_hasCapability && caps) {
		capabilities = readCapabilities(env, j_hasCapability, caps);
		if (callback) {
			callback(capabilities);
		}
	} else if (capabilities != NetworkCapabilities::None) {
		capabilities = NetworkCapabilities::None;
		if (callback) {
			callback(capabilities);
		}
	}
}

void NetworkConnectivity::handleLinkPropertiesChanged(JNIEnv *env, jobject props) {
	//log::verbose("NetworkConnectivity", "onLinkPropertiesChanged");
}

}

#endif
