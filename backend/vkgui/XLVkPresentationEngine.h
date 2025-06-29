/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_BACKEND_VKGUI_XLVKPRESENTATIONENGINE_H_
#define XENOLITH_BACKEND_VKGUI_XLVKPRESENTATIONENGINE_H_

#include "XLCorePresentationEngine.h"
#include "XLVkView.h"
#include "XLVkSwapchain.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class SP_PUBLIC PresentationEngine final : public core::PresentationEngine {
public:
	virtual ~PresentationEngine() = default;

	bool init(Device *, View *, Rc<Surface> &&, core::FrameConstraints &&,
			uint64_t frameInterval = 0);

	virtual bool run() override;

	virtual bool recreateSwapchain() override;
	virtual bool createSwapchain(const core::SurfaceInfo &, core::SwapchainConfig &&cfg,
			core::PresentMode presentMode) override;

	virtual void handleFramePresented(core::PresentationFrame *) override;

protected:
	virtual void acquireFrameData(core::PresentationFrame *,
			Function<void(core::PresentationFrame *)> &&) override;

	bool isImagePresentable(const core::ImageObject &image, VkFilter &filter) const;

	View *_view = nullptr;
	Rc<Surface> _surface;
};

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VKGUI_XLVKPRESENTATIONENGINE_H_ */
