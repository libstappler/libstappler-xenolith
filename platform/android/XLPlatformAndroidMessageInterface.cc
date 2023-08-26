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

#include "XLPlatformAndroidMessageInterface.h"

#if ANDROID

namespace stappler::xenolith::platform {

thread_local MessagingActivityAdapter *tl_adapter = nullptr;

MessagingActivityAdapter::~MessagingActivityAdapter() {
	if (tl_adapter) {
		tl_adapter = nullptr;
	}
}

bool MessagingActivityAdapter::init(Activity *a, int requestId) {
	auto env = a->getNativeActivity()->env;
	auto cl = a->getClassLoader()->findClass(env, "org.stappler.xenolith.firebase.MessagingActivityAdapter");

	auto contructor = env->GetMethodID(cl, "<init>", "(Landroid/app/Activity;JI)V");
	auto obj = env->NewObject(cl, contructor, a->getNativeActivity()->clazz, reinterpret_cast<jlong>(this), requestId);
	if (!obj) {
		return false;
	}

	_requestId = requestId;
	_activity = a;
	thiz = env->NewGlobalRef(obj);

	j_askNotificationPermission = env->GetMethodID(cl, "askNotificationPermission", "()V");
	j_acquireToken = env->GetMethodID(cl, "acquireToken", "()V");
	j_parseResult = env->GetMethodID(cl, "parseResult", "(ILandroid/content/Intent;)V");

	tl_adapter = this;

	return true;
}

void MessagingActivityAdapter::handleStart(Activity *a) {
	acquireToken(a->getNativeActivity()->env);
}

void MessagingActivityAdapter::handleDestroy(Activity *a) {
	if (thiz) {
		auto env = a->getNativeActivity()->env;
		env->DeleteGlobalRef(thiz);
		thiz = nullptr;
	}
}

bool MessagingActivityAdapter::handleActivityResult(Activity *a, int request_code, int result_code, jobject data) {
	if (request_code == _requestId) {
		auto env = a->getNativeActivity()->env;
		env->CallVoidMethod(thiz, j_parseResult, result_code, data);
		return true;
	}
	return false;
}

void MessagingActivityAdapter::askNotificationPermission(JNIEnv *env) {
	env->CallVoidMethod(thiz, j_askNotificationPermission);
}

void MessagingActivityAdapter::acquireToken(JNIEnv *env) {
	env->CallVoidMethod(thiz, j_acquireToken);
}

void MessagingActivityAdapter::handleToken(StringView str) {
	_activity->setMessageToken(str);
}

void MessagingActivityAdapter::handleRemoveNotification(const Value &val) {
	_activity->handleRemoteNotification(val);
}

MessagingService::MessagingService(JNIEnv *env, jobject obj) {
	thiz = env->NewGlobalRef(obj);
	refId = retain();

	auto appData = loadApplicationInfo();

	auto &d = appData.getValue("drawables");
	if (d.isDictionary()) {
		auto icStatName = d.getInteger("ic_stat_name");
		if (icStatName) {
			auto cl = env->GetObjectClass(thiz);
			auto m = env->GetMethodID(cl, "setDefaultIcon", "(I)V");
			if (m) {
				env->CallVoidMethod(thiz, m, icStatName);
			}
			env->DeleteLocalRef(cl);
		}
	}
}

MessagingService::~MessagingService() {
	if (thiz) {
		log::error("MessageInterface", "MessageService link was not deleted: memory leak");
	}
}

void MessagingService::finalize(JNIEnv *env) {
	if (thiz) {
		env->DeleteGlobalRef(thiz);
		thiz = nullptr;
	}
	release(refId);
}

void MessagingService::handleNewToken(JNIEnv *env, jstring tok) {
	const char *c_tok = env->GetStringUTFChars(tok, 0);

	if (tl_adapter) {
		tl_adapter->handleToken(c_tok);
	}

	saveMessageToken(c_tok);

	env->ReleaseStringUTFChars(tok, c_tok);
}

void MessagingService::handleRemoteNotification(JNIEnv *env, jstring header, jstring message, jstring url) {
	const char *c_header = env->GetStringUTFChars(header, 0);
	const char *c_message = env->GetStringUTFChars(message, 0);
	const char *c_url = env->GetStringUTFChars(url, 0);

	log::info("MessageInterface", "handleRemoteNotification: ", c_header, " - ", c_message);

	if (tl_adapter) {
		tl_adapter->handleRemoveNotification(Value({
			pair("header", Value(c_header)),
			pair("message", Value(c_message)),
			pair("url", Value(c_url))
		}));
	}

	env->ReleaseStringUTFChars(header, c_header);
	env->ReleaseStringUTFChars(message, c_message);
	env->ReleaseStringUTFChars(url, c_url);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_org_stappler_xenolith_firebase_MessagingService_onCreated(JNIEnv *env, jobject thiz) {
	auto iface = Rc<platform::MessagingService>::alloc(env, thiz);

	return reinterpret_cast<jlong>(iface.get());
}

extern "C"
JNIEXPORT void JNICALL
Java_org_stappler_xenolith_firebase_MessagingService_onDestroyed(JNIEnv *env, jobject thiz,
		jlong native_pointer) {
	auto iface = reinterpret_cast<platform::MessagingService *>(native_pointer);
	iface->finalize(env);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_stappler_xenolith_firebase_MessagingService_onRemoteNotification(JNIEnv *env,
		jobject thiz, jlong native_pointer, jstring header, jstring message, jstring url) {
	auto iface = reinterpret_cast<platform::MessagingService *>(native_pointer);
	iface->handleRemoteNotification(env, header, message, url);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_stappler_xenolith_firebase_MessagingService_onTokenReceived(JNIEnv *env, jobject thiz,
		jlong native_pointer, jstring value) {
	auto iface = reinterpret_cast<platform::MessagingService *>(native_pointer);
	iface->handleNewToken(env, value);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_stappler_xenolith_firebase_MessagingActivityAdapter_onTokenReceived(JNIEnv *env, jobject thiz,
		jlong native_pointer, jstring tok) {
	const char *c_tok = env->GetStringUTFChars(tok, 0);

	saveMessageToken(c_tok);

	auto a = reinterpret_cast<MessagingActivityAdapter *>(native_pointer);
	a->handleToken(c_tok);

	env->ReleaseStringUTFChars(tok, c_tok);
}

}

#endif
