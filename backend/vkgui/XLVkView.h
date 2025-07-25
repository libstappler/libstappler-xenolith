/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_BACKEND_VKGUI_XLVKVIEW_H_
#define XENOLITH_BACKEND_VKGUI_XLVKVIEW_H_

#include "XLVkSwapchain.h"
#include "XLAppWindow.h"
#include "XLCorePresentationFrame.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class SP_PUBLIC VkWindow : public AppWindow {
public:
	virtual ~VkWindow() = default;

	virtual bool init(NotNull<Context>, NotNull<AppThread>, NotNull<NativeWindow>, const Device &);

	virtual void end() override;

	vk::Device *getDevice() const { return _device; }

	virtual void handleFramePresented(core::PresentationFrame *);

	virtual core::SurfaceInfo getSurfaceOptions(core::SurfaceInfo &&) const;

protected:
	bool _readyForNextFrame = false;

	Rc<Instance> _instance;
	Rc<Device> _device;
};

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VKGUI_XLVKVIEW_H_ */
