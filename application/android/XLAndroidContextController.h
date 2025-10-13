/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_APPLICATION_ANDROID_XLANDROIDCONTEXTCONTROLLER_H_
#define XENOLITH_APPLICATION_ANDROID_XLANDROIDCONTEXTCONTROLLER_H_

#include "XLAndroidInput.h"
#include "XLContextInfo.h"
#include "platform/XLContextController.h"
#include "XLAndroidNetworkConnectivity.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class AndroidActivity;

class AndroidContextController : public ContextController {
public:
	static constexpr StringView NetworkConnectivityClassName =
			"org.stappler.xenolith.appsupport.NetworkConnectivity";
	static constexpr StringView NetworkConnectivityClassPath =
			"org/stappler/xenolith/appsupport/NetworkConnectivity";

	static constexpr int FLAG_ACTIVITY_NEW_TASK = 268'435'456;

	static void acquireDefaultConfig(ContextConfig &);

	virtual ~AndroidContextController();

	virtual bool init(NotNull<Context>, ContextConfig &&);

	virtual int run(NotNull<ContextContainer>) override;

	virtual bool isCursorSupported(WindowCursor, bool serverSide) const override;
	virtual WindowCapabilities getCapabilities() const override;

	bool loadActivity(ANativeActivity *a, BytesView data);
	void destroyActivity(NotNull<AndroidActivity>);

	jni::Ref getSelf() const;

	Rc<WindowInfo> makeWindowInfo(ANativeWindow *) const;

protected:
	Rc<core::Instance> loadInstance();

	Rc<NetworkConnectivity> _networkConnectivity;
	Rc<ContextContainer> _container;
	Set<Rc<AndroidActivity>> _activities;
};

} // namespace stappler::xenolith::platform

#endif // XENOLITH_APPLICATION_ANDROID_XLANDROIDCONTEXTCONTROLLER_H_
