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

#ifndef XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROIDMESSAGEINTERFACE_H_
#define XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROIDMESSAGEINTERFACE_H_

#include "XLPlatformAndroidActivity.h"

#if ANDROID

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct MessagingActivityAdapter : public ActivityComponent {
	jobject thiz = nullptr;
	Activity *_activity = nullptr;
	jmethodID j_askNotificationPermission = nullptr;
	jmethodID j_acquireToken = nullptr;
	jmethodID j_parseResult = nullptr;
	int _requestId = 1;

	virtual ~MessagingActivityAdapter();

	virtual bool init(Activity *, int requestId);

	virtual void handleStart(Activity *a) override;
	virtual void handleDestroy(Activity *a) override;

	virtual bool handleActivityResult(Activity *a, int request_code, int result_code, jobject data) override;

	void askNotificationPermission(JNIEnv *);
	void acquireToken(JNIEnv *);
	void handleToken(StringView);
	void handleRemoveNotification(const Value &);
};

struct MessagingService  : public Ref {
	uint64_t refId = 0;
	jobject thiz = nullptr;
	String token;

	MessagingService(JNIEnv *, jobject);
	virtual ~MessagingService();

	void finalize(JNIEnv *);

	virtual void handleNewToken(JNIEnv *, jstring);
	virtual void handleRemoteNotification(JNIEnv *, jstring, jstring, jstring);
};

}

#endif

#endif /* XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROIDMESSAGEINTERFACE_H_ */
