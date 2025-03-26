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

#ifndef XENOLITH_BACKEND_VKGUI_PLATFORM_WIN32_XLVKGUIVIEWIMPL_H_
#define XENOLITH_BACKEND_VKGUI_PLATFORM_WIN32_XLVKGUIVIEWIMPL_H_

#include "XLVkView.h"

#if WIN32

#include "SPPlatformUnistd.h"
#include "win32/XLPlatformWin32View.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

class ViewImpl : public vk::View {
public:
	ViewImpl();
	virtual ~ViewImpl();

	virtual bool init(Application &loop, const core::Device &dev, ViewInfo &&);

	virtual void run() override;
	virtual void end() override;

	virtual void updateTextCursor(uint32_t pos, uint32_t len) override;
	virtual void updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) override;
	virtual void runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) override;
	virtual void cancelTextInput() override;

	virtual bool isInputEnabled() const override { return _inputEnabled; }

	vk::Device *getDevice() const { return _device; }

	virtual void mapWindow() override;

	virtual void linkWithNativeWindow(void *) override { }

	void captureWindow();
	void releaseWindow();

	void handlePaint();

	virtual void readFromClipboard(Function<void(BytesView, StringView)> &&, Ref *) override;
	virtual void writeToClipboard(BytesView, StringView contentType = StringView()) override;

protected:

	Rc<xenolith::platform::Win32View> _view;
	bool _inputEnabled = false;
	bool _windowCaptured = false;
	std::mutex _captureMutex;
	std::condition_variable _captureCondVar;
};

}

#endif

#endif /* XENOLITH_BACKEND_VKGUI_PLATFORM_WIN32_XLVKGUIVIEWIMPL_H_ */
