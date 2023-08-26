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

#ifndef XENOLITH_BACKEND_VKGUI_PLATFORM_ANDROID_XLVKGUIVIEWIMPL_H_
#define XENOLITH_BACKEND_VKGUI_PLATFORM_ANDROID_XLVKGUIVIEWIMPL_H_

#include "XLVkView.h"
#include "android/XLPlatformAndroidActivity.h"

#if ANDROID

namespace stappler::xenolith::vk::platform {

class ViewImpl : public vk::View {
public:
	ViewImpl();
	virtual ~ViewImpl();

	virtual bool init(Application &loop, core::Device &dev, ViewInfo &&info);

	virtual void run() override;
	virtual void threadInit() override;
	virtual void threadDispose() override;
	virtual bool worker() override;
	virtual void update(bool displayLink) override;
	virtual void end() override;

	virtual void wakeup() override;
	virtual void mapWindow() override;

	virtual void updateTextCursor(uint32_t pos, uint32_t len) override;
	virtual void updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) override;
	virtual void runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) override;
	virtual void cancelTextInput() override;

	void runWithWindow(ANativeWindow *);
	void initWindow();
	void stopWindow();

	void setActivity(xenolith::platform::Activity *);

	virtual void setDecorationTone(float) override;
	virtual void setDecorationVisible(bool) override;

	virtual bool isInputEnabled() const override;

	virtual void linkWithNativeWindow(void *) override;
	virtual void stopNativeWindow() override;

protected:
	using vk::View::init;

	virtual bool pollInput(bool frameReady) override;
	virtual core::SurfaceInfo getSurfaceOptions() const override;

	void doSetDecorationTone(float);
    void doSetDecorationVisible(bool);
	void updateDecorations();

	bool _started = false;
	ANativeWindow *_nativeWindow = nullptr;
	Extent2 _identityExtent;
	xenolith::platform::Activity *_activity = nullptr;

	float _decorationTone = 0.0f;
	bool _decorationVisible = true;
	bool _usePreRotation = true;

	std::mutex _windowMutex;
	std::condition_variable _windowCond;
};

}

#endif

#endif /* XENOLITH_BACKEND_VKGUI_PLATFORM_ANDROID_XLVKGUIVIEWIMPL_H_ */
