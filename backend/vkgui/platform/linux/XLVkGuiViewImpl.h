/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_BACKEND_VKGUI_PLATFORM_LINUX_XLVKGUIVIEWIMPL_H_
#define XENOLITH_BACKEND_VKGUI_PLATFORM_LINUX_XLVKGUIVIEWIMPL_H_

#include "XLVkView.h"

#if 0 && LINUX

#include "linux/XLPlatformLinuxView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

using xenolith::platform::TextInputFlags;

enum class SurfaceType : uint32_t {
	None,
	XCB = 1 << 0,
	Wayland = 1 << 1,
};

SP_DEFINE_ENUM_AS_MASK(SurfaceType)

class ViewImpl : public vk::View {
public:
	virtual ~ViewImpl();

	virtual bool init(Application &loop, const core::Device &dev, ViewInfo &&);

	virtual void run() override;
	virtual void end() override;

	xenolith::platform::LinuxViewInterface *getView() const { return _view; }

	vk::Device *getDevice() const { return _device; }

	virtual void mapWindow() override;

	virtual void linkWithNativeWindow(void *) override { }

	virtual void readFromClipboard(Function<void(BytesView, StringView)> &&, Ref *) override;
	virtual void writeToClipboard(BytesView, StringView contentType = StringView()) override;

	virtual void handleFramePresented(core::PresentationFrame *) override;

	virtual core::SurfaceInfo getSurfaceOptions(core::SurfaceInfo &&) const override;

protected:
	//virtual bool recreateSwapchain(core::PresentMode) override;

	virtual bool updateTextInput(const TextInputRequest &, TextInputFlags flags) override;
	virtual void cancelTextInput() override;

	virtual bool isTextInputEnabled() const override { return _inputEnabled; }

	virtual void handleLayerUpdate(const ViewLayer &) override;

	Rc<event::Handle> _pollHandle;
	Rc<xenolith::platform::LinuxViewInterface> _view;
	bool _inputEnabled = false;
};

} // namespace stappler::xenolith::vk::platform

#endif

#endif /* XENOLITH_BACKEND_VKGUI_PLATFORM_LINUX_XLVKGUIVIEWIMPL_H_ */
