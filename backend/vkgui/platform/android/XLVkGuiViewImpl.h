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

#ifndef XENOLITH_BACKEND_VKGUI_PLATFORM_ANDROID_XLVKGUIVIEWIMPL_H_
#define XENOLITH_BACKEND_VKGUI_PLATFORM_ANDROID_XLVKGUIVIEWIMPL_H_

#include "XLVkView.h"
#include "android/XLPlatformAndroidActivity.h"

#if ANDROID

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

class ViewImpl : public vk::View {
public:
	ViewImpl();
	virtual ~ViewImpl();

	virtual bool init(Application &loop, core::Device &dev, ViewInfo &&info);

	virtual void run() override;
	virtual void update(bool displayLink) override;
	virtual void end() override;

	virtual bool isTextInputEnabled() const override; // from view thread

	virtual bool updateTextInput(const TextInputRequest &,
			xenolith::platform::TextInputFlags flags) override; // from view thread

	virtual void cancelTextInput() override; // from view thread

	void runWithWindow(ANativeWindow *);

	void setActivity(xenolith::platform::Activity *);

	virtual void setDecorationTone(float) override;
	virtual void setDecorationVisible(bool) override;

	virtual void linkWithNativeWindow(void *) override;

	virtual void readFromClipboard(Function<void(BytesView, StringView)> &&, Ref *) override;
	virtual void writeToClipboard(BytesView, StringView contentType) override;

	virtual core::SurfaceInfo getSurfaceOptions(core::SurfaceInfo &&opts) const override;

protected:
	using vk::View::init;

	void doSetDecorationTone(float);
	void doSetDecorationVisible(bool);
	void updateDecorations();

	void doReadFromClipboard(Function<void(BytesView, StringView)> &&, Ref *);
	void doWriteToClipboard(BytesView, StringView contentType);

	ANativeWindow *_nativeWindow = nullptr;
	Extent2 _identityExtent;
	xenolith::platform::Activity *_activity = nullptr;

	float _decorationTone = 0.0f;
	bool _decorationVisible = true;
	bool _usePreRotation = true;
	bool _decorationShown = true;

	std::mutex _windowMutex;
	std::condition_variable _windowCond;
};

} // namespace stappler::xenolith::vk::platform

#endif

#endif /* XENOLITH_BACKEND_VKGUI_PLATFORM_ANDROID_XLVKGUIVIEWIMPL_H_ */
