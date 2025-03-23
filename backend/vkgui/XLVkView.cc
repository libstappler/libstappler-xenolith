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

	_instance = _glLoop->getInstance().get_cast<Instance>();
	_device = const_cast<Device *>(static_cast<const Device *>(&dev));
	return true;
}

void View::end() {
	xenolith::View::end();
}

void View::captureImage(StringView name, const Rc<core::ImageObject> &image, AttachmentLayout l) const {
	auto str = name.str<Interface>();
	_device->readImage(*(Loop *)_glLoop.get(), (Image *)image.get(), l,
		[str] (const ImageInfoData &info, BytesView view) mutable {
			if (!StringView(str).ends_with(".png")) {
				str = str + String(".png");
			}
			if (!view.empty()) {
				auto fmt = core::getImagePixelFormat(info.format);
				bitmap::PixelFormat pixelFormat = bitmap::PixelFormat::Auto;
				switch (fmt) {
				case core::PixelFormat::A: pixelFormat = bitmap::PixelFormat::A8; break;
				case core::PixelFormat::IA: pixelFormat = bitmap::PixelFormat::IA88; break;
				case core::PixelFormat::RGB: pixelFormat = bitmap::PixelFormat::RGB888; break;
				case core::PixelFormat::RGBA: pixelFormat = bitmap::PixelFormat::RGBA8888; break;
				default: break;
				}
				if (pixelFormat != bitmap::PixelFormat::Auto) {
					Bitmap bmp(view.data(), info.extent.width, info.extent.height, pixelFormat);
					bmp.save(str);
				}
			}
		});
}

void View::captureImage(Function<void(const ImageInfoData &info, BytesView view)> &&cb, const Rc<core::ImageObject> &image, AttachmentLayout l) const {
	_device->readImage(*(Loop *)_glLoop.get(), (Image *)image.get(), l, sp::move(cb));
}

void View::handleFramePresented(core::PresentationFrame *frame) {

}

core::SurfaceInfo View::getSurfaceOptions(core::SurfaceInfo &&opts) const {
	return std::move(opts);
}

}
