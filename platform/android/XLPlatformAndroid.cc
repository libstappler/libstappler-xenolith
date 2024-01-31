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

#include "XLPlatformAndroid.h"

#if ANDROID

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static std::mutex s_dataMutex;

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
