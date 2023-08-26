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

#ifndef XENOLITH_BACKEND_VKGUI_PLATFORM_LINUX_XLVKGUIVIEWIMPL_H_
#define XENOLITH_BACKEND_VKGUI_PLATFORM_LINUX_XLVKGUIVIEWIMPL_H_

#include "XLVkView.h"
#include "linux/XLPlatformLinuxView.h"

#if LINUX

namespace stappler::xenolith::vk::platform {

enum class SurfaceType : uint32_t {
	None,
	XCB = 1 << 0,
	Wayland = 1 << 1,
};

SP_DEFINE_ENUM_AS_MASK(SurfaceType)

class ViewImpl : public vk::View {
public:
	ViewImpl();
	virtual ~ViewImpl();

	virtual bool init(Application &loop, const core::Device &dev, ViewInfo &&);

	virtual void threadInit() override;
	virtual void threadDispose() override;
	virtual bool worker() override;

	virtual void wakeup() override;

	virtual void updateTextCursor(uint32_t pos, uint32_t len) override;
	virtual void updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) override;
	virtual void runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) override;
	virtual void cancelTextInput() override;

	virtual void presentWithQueue(vk::DeviceQueue &, Rc<ImageStorage> &&) override;

	virtual bool isInputEnabled() const override { return _inputEnabled; }
	xenolith::platform::LinuxViewInterface *getView() const { return _view; }

	vk::Device *getDevice() const { return _device; }

	// minimal poll interval
	virtual uint64_t getUpdateInterval() const override { return 1000; }

	virtual void mapWindow() override;

	virtual void linkWithNativeWindow(void *) override { }
	virtual void stopNativeWindow() override { }

protected:
	virtual bool pollInput(bool frameReady) override;

	virtual core::SurfaceInfo getSurfaceOptions() const override;

	virtual void finalize() override;

	Rc<xenolith::platform::LinuxViewInterface> _view;
	int _eventFd = -1;
	bool _inputEnabled = false;
};

}

#endif

#endif /* XENOLITH_BACKEND_VKGUI_PLATFORM_LINUX_XLVKGUIVIEWIMPL_H_ */
