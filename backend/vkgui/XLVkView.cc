/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLVkView.h"
#include "XLVkLoop.h"
#include "XLVkDevice.h"
#include "XLVkTextureSet.h"
#include "XLVkSwapchain.h"
#include "XLVkRenderPass.h"
#include "XLVkGuiConfig.h"
#include "XLDirector.h"
#include "XLCoreImageStorage.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameCache.h"
#include "XLVkPresentationEngine.h"
#include "SPBitmap.h"

#define XL_VKVIEW_DEBUG 0

#ifndef XL_VKAPI_LOG
#define XL_VKAPI_LOG(...)
#endif

#if XL_VKVIEW_DEBUG
#define XL_VKVIEW_LOG(...) log::debug("vk::View", __VA_ARGS__)
#else
#define XL_VKVIEW_LOG(...)
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

bool View::init(Application &app, const Device &dev, ViewInfo &&info) {
	if (!xenolith::View::init(app, move(info))) {
		return false;
	}

	_instance = _loop->getInstance().get_cast<Instance>();
	_device = const_cast<Device *>(static_cast<const Device *>(&dev));
	return true;
}

void View::end() { xenolith::View::end(); }

void View::handleFramePresented(core::PresentationFrame *frame) { }

core::SurfaceInfo View::getSurfaceOptions(core::SurfaceInfo &&opts) const {
	return std::move(opts);
}

} // namespace stappler::xenolith::vk
