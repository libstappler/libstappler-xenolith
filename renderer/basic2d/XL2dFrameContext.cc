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

#include "XL2dFrameContext.h"
#include "XLFrameInfo.h"
#include "XLCoreLoop.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameQueue.h"
#include "XLDirector.h"
#include "SPBitmap.h"

namespace stappler::xenolith::basic2d {

bool ShadowLightInput::addAmbientLight(const Vec4 &pos, const Color4F &color, bool softShadow) {
	if (ambientLightCount >= config::MaxAmbientLights) {
		return false;
	}

	ambientLights[ambientLightCount++] = AmbientLightData{pos, color, uint32_t(softShadow ? 1 : 0)};
	return true;
}

bool ShadowLightInput::addDirectLight(const Vec4 &pos, const Color4F &color, const Vec4 &data) {
	if (directLightCount >= config::MaxDirectLights) {
		return false;
	}

	directLights[directLightCount++] = DirectLightData{pos, color, data};
	return true;
}

Extent2 ShadowLightInput::getShadowExtent(Size2 frameSize) const {
	return Extent2(std::ceil((frameSize.width / sceneDensity) * shadowDensity),
			std::ceil((frameSize.height / sceneDensity) * shadowDensity));
}

Size2 ShadowLightInput::getShadowSize(Size2 frameSize) const {
	return Size2((frameSize.width / sceneDensity) * shadowDensity,
			(frameSize.height / sceneDensity) * shadowDensity);
}

void FrameContext2d::onEnter(Scene *scene) {
	FrameContext::onEnter(scene);
	if (!_init || _queue) {
		if (!initWithQueue(_queue)) {
			log::error("FrameContext2d", "Fail to initialize with queue:", _queue->getName());
		}
	}
}

void FrameContext2d::onExit() {
	FrameContext::onExit();
}

Rc<FrameContextHandle> FrameContext2d::makeHandle(FrameInfo &frame) {
	auto h = Rc<FrameContextHandle2d>::alloc();
	h->director = frame.director;
	h->context = this;
	h->shadows = Rc<CommandList>::create(frame.pool);
	h->commands = Rc<CommandList>::create(frame.pool);
	return h;
}

void FrameContext2d::submitHandle(FrameInfo &frame, FrameContextHandle *handle) {
	auto h = static_cast<FrameContextHandle2d *>(handle);

	frame.resolvedInputs.emplace(_vertexAttachmentData);
	frame.resolvedInputs.emplace(_lightAttachmentData);
	frame.resolvedInputs.emplace(_shadowVertexAttachmentData);
	frame.resolvedInputs.emplace(_sdfImageAttachmentData);

	if (_materialDependency) {
		handle->waitDependencies.emplace_back(_materialDependency);
	}

	frame.director->getGlLoop()->performOnGlThread(
			[this, req = frame.request, q = _queue, dir = frame.director,
			 h = Rc<FrameContextHandle2d>(h)] () mutable {

		req->addInput(_vertexAttachmentData, Rc<FrameContextHandle2d>(h));
		req->addInput(_lightAttachmentData, Rc<FrameContextHandle2d>(h));
		req->addInput(_shadowVertexAttachmentData, Rc<FrameContextHandle2d>(h));
		req->addInput(_sdfImageAttachmentData, Rc<FrameContextHandle2d>(h));

		req->setOutput(_sdfImageAttachmentData, [loop = dir->getGlLoop()] (core::FrameAttachmentData &data, bool success, Rc<Ref> &&ref) {
			loop->captureImage([] (const core::ImageInfoData &info, BytesView view) {
				Bitmap bmpSdf;
				bmpSdf.alloc(info.extent.width, info.extent.height, bitmap::PixelFormat::A8);

				Bitmap bmpHeight;
				bmpHeight.alloc(info.extent.width, info.extent.height, bitmap::PixelFormat::A8);

				auto dSdf = bmpSdf.dataPtr();
				auto dHgt = bmpHeight.dataPtr();

				while (!view.empty()) {
					auto value = view.readFloat16() / 16.0f;

					*dSdf = uint8_t(value * 255.0f);
					++ dSdf;

					*dHgt = uint8_t((view.readFloat16() / 50.0f) * 255.0f);
					++ dHgt;
				}

				bmpSdf.save(toString("sdf-image-", Time::now().toMicros(), ".png"));
				bmpHeight.save(toString("sdf-height-", Time::now().toMicros(), ".png"));
			}, data.image->getImage(), data.image->getLayout());
			return true;
		});
	}, this);

	FrameContext::submitHandle(frame, handle);
}

bool FrameContext2d::initWithQueue(core::Queue *queue) {
	for (auto &it : queue->getAttachments()) {
		if (it->key == MaterialAttachmentName) {
			if (auto m = dynamic_cast<core::MaterialAttachment *>(it->attachment.get())) {
				_materialAttachmentData = it;
				readMaterials(m);
			}
		}
	}

	for (auto &it : queue->getInputAttachments()) {
		if (it->key == VertexAttachmentName) {
			_vertexAttachmentData = it;
		} else if (it->key == LightDataAttachmentName) {
			_lightAttachmentData = it;
		} else if (it->key == ShadowVertexAttachmentName) {
			_shadowVertexAttachmentData = it;
		} else if (it->key == SdfImageAttachmentName) {
			_sdfImageAttachmentData = it;
		}
	}

	return _materialAttachmentData && _vertexAttachmentData && _lightAttachmentData
			&& _shadowVertexAttachmentData && _sdfImageAttachmentData;
}

}
