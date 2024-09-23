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

#include "XL2dVectorSprite.h"
#include "XLApplication.h"
#include "XLTexture.h"
#include "XL2dFrameContext.h"
#include "XLFrameInfo.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

static Rc<VectorCanvasDeferredResult> VectorSprite_runDeferredVectorCavas(thread::TaskQueue &queue, Rc<VectorImageData> &&image,
		Size2 targetSize, const VectorCanvasConfig &config, bool waitOnReady) {
	auto result = new std::promise<Rc<VectorCanvasResult>>;
	Rc<VectorCanvasDeferredResult> ret = Rc<VectorCanvasDeferredResult>::create(result->get_future(), waitOnReady);
	queue.perform([queue = Rc<thread::TaskQueue>(&queue), image = move(image), targetSize, config, ret, result] () mutable {
		auto canvas = VectorCanvas::getInstance();
		canvas->setConfig(config);
		auto res = canvas->draw(move(image), targetSize);
		result->set_value(res);

		queue->onMainThread([ret = move(ret), res = move(res), result] () mutable {
			ret->handleReady(move(res));
			delete result;
		}, queue);
	}, ret);
	return ret;
}

static float VectorSprite_getPseudoSdfOffset(float value) {
	const float prefixSdf = 15.0f;
	const float prefixDepth = 3.0f;

	if (value <= 0.0) {
		return 0.0f;
	} else if (value < prefixDepth) {
		return prefixSdf;
	} else {
		return prefixSdf + (std::floor(value) - prefixDepth) / (40.0f - prefixDepth) * (config::VGPseudoSdfOffset - prefixSdf);
	}
}

VectorSprite::VectorSprite() { }

bool VectorSprite::init(Rc<VectorImage> &&img) {
	XL_ASSERT(img, "Image should not be nullptr");

	if (!Sprite::init() || !img) {
		return false;
	}

	_image = img;
	if (_image) {
		_contentSize = _image->getImageSize();
	}
	return true;
}

bool VectorSprite::init(Size2 size, StringView data) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(size, data);
	if (_image) {
		_contentSize = _image->getImageSize();
	}
	return _image != nullptr;
}

bool VectorSprite::init(Size2 size, VectorPath &&path) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(size, move(path));
	if (_image) {
		_contentSize = _image->getImageSize();
	}
	return _image != nullptr;
}

bool VectorSprite::init(Size2 size) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(size);
	if (_image) {
		_contentSize = _image->getImageSize();
	}
	return _image != nullptr;
}

bool VectorSprite::init(StringView data) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(data);
	if (_image) {
		_contentSize = _image->getImageSize();
	}
	return _image != nullptr;
}

bool VectorSprite::init(BytesView data) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(data);
	if (_image) {
		_contentSize = _image->getImageSize();
	}
	return _image != nullptr;
}

bool VectorSprite::init(FilePath path) {
	if (!Sprite::init()) {
		return false;
	}

	_image = Rc<VectorImage>::create(path);
	if (_image) {
		_contentSize = _image->getImageSize();
	}
	return _image != nullptr;
}

Rc<VectorPathRef> VectorSprite::addPath(StringView id, StringView cache, Mat4 pos) {
	return _image ? _image->addPath(id, cache, pos) : nullptr;
}

Rc<VectorPathRef> VectorSprite::addPath(const VectorPath & path, StringView id, StringView cache, Mat4 pos) {
	return _image ? _image->addPath(path, id, cache, pos) : nullptr;
}

Rc<VectorPathRef> VectorSprite::addPath(VectorPath && path, StringView id, StringView cache, Mat4 pos) {
	return _image ? _image->addPath(move(path), id, cache, pos) : nullptr;
}

Rc<VectorPathRef> VectorSprite::getPath(StringView id) {
	return _image ? _image->getPath(id) : nullptr;
}

void VectorSprite::removePath(const Rc<VectorPathRef> &path) {
	if (_image) {
		_image->removePath(path);
	}
}

void VectorSprite::removePath(StringView id) {
	if (_image) {
		_image->removePath(id);
	}
}

void VectorSprite::clear() {
	if (_image) {
		_image->clear();
	}
}

void VectorSprite::setImage(Rc<VectorImage> &&img) {
	if (_image != img) {
		_image = move(img);
		if (_image) {
			_image->setDirty();
		}
	}
}

const Rc<VectorImage> &VectorSprite::getImage() const {
	return _image;
}

void VectorSprite::setQuality(float val) {
	if (_quality != val) {
		_quality = val;
		if (_image) {
			_image->setDirty();
		}
	}
}

void VectorSprite::onTransformDirty(const Mat4 &parent) {
	_vertexesDirty = true;
	Sprite::onTransformDirty(parent);
}

bool VectorSprite::visitDraw(FrameInfo &frame, NodeFlags parentFlags) {
	if (_image && _image->isDirty()) {
		_vertexesDirty = true;
	}
	return Sprite::visitDraw(frame, parentFlags);
}

uint32_t VectorSprite::getTrianglesCount() const {
	uint32_t ret = 0;
	if (_deferredResult) {
		if (_deferredResult->isReady()) {
			for (auto &it : _deferredResult->getResult()->data) {
				ret += it.data->indexes.size() / 3;
			}
		}
	} else if (_result) {
		for (auto &it : _result->data) {
			ret += it.data->indexes.size() / 3;
		}
	}
	return ret;
}

uint32_t VectorSprite::getVertexesCount() const {
	uint32_t ret = 0;
	if (_deferredResult) {
		if (_deferredResult->isReady()) {
			for (auto &it : _deferredResult->getResult()->data) {
				ret += it.data->data.size();
			}
		}
	} else if (_result) {
		for (auto &it : _result->data) {
			ret += it.data->data.size();
		}
	}
	return ret;
}

void VectorSprite::setDeferred(bool val) {
	if (val != _deferred) {
		_deferred = val;
		_vertexesDirty = true;
	}
}

void VectorSprite::pushCommands(FrameInfo &frame, NodeFlags flags) {
	if (!_image) {
		return;
	}

	if (!_deferredResult && (!_result || _result->data.empty())) {
		return;
	}

	FrameContextHandle2d *handle = static_cast<FrameContextHandle2d *>(frame.currentContext);

	if (_result) {
		handle->commands->pushVertexArray([&] (memory::pool_t *pool) {
			auto &targetData = _result->mut;
			auto reqMemSize = sizeof(InstanceVertexData) * targetData.size();

			// pool memory is 16-bytes aligned, no problems with Mat4
			auto tmpData = new (memory::pool::palloc(frame.pool->getPool(), reqMemSize)) InstanceVertexData[targetData.size()];
			auto target = tmpData;
			if (_normalized) {
				auto transform = frame.modelTransformStack.back() * _targetTransform;
				for (auto &it : targetData) {
					target->instances = it.instances.pdup(pool);
					for (auto &inst : target->instances) {
						auto modelTransform = transform * inst.transform;

						Mat4 newMV;
						newMV.m[12] = floorf(modelTransform.m[12]);
						newMV.m[13] = floorf(modelTransform.m[13]);
						newMV.m[14] = floorf(modelTransform.m[14]);

						const_cast<TransformData &>(inst).transform = frame.viewProjectionStack.back() * newMV;
					}
					target->data = it.data;
					++ target;
				}
			} else {
				auto transform = frame.viewProjectionStack.back() * frame.modelTransformStack.back() * _targetTransform;
				for (auto &it : targetData) {
					target->instances = it.instances.pdup(pool);
					for (auto &inst : target->instances) {
						const_cast<TransformData &>(inst).transform = transform * inst.transform;
					}
					target->data = it.data;
					++ target;
				}
			}

			return makeSpanView(tmpData, targetData.size());
		}, frame.zPath, _materialId, handle->getCurrentState(), _realRenderingLevel, frame.depthStack.back(), _commandFlags);
	} else if (_deferredResult) {
		if (_deferredResult->isReady() && _deferredResult->getResult()->data.empty()) {
			return;
		}

		handle->commands->pushDeferredVertexResult(_deferredResult, frame.viewProjectionStack.back(),
				frame.modelTransformStack.back() * _targetTransform, _normalized, frame.zPath,
						_materialId, handle->getCurrentState(), _realRenderingLevel, frame.depthStack.back(), _commandFlags);
	}
}

void VectorSprite::initVertexes() {
	// prevent to do anything
}

void VectorSprite::updateVertexes(FrameInfo &frame) {
	if (!_image || !_director) {
		return;
	}

	Vec3 viewScale;
	_modelViewTransform.decompose(&viewScale, nullptr, nullptr);

	Size2 imageSize = _image->getImageSize();
	Size2 targetViewSpaceSize(_contentSize.width * viewScale.x / _textureRect.size.width,
			_contentSize.height * viewScale.y / _textureRect.size.height);

	float targetScaleX = _textureRect.size.width;
	float targetScaleY = _textureRect.size.height;
	float targetOffsetX = -_textureRect.origin.x * imageSize.width;
	float targetOffsetY = -_textureRect.origin.y * imageSize.height;

	Size2 texSize(imageSize.width * _textureRect.size.width, imageSize.height * _textureRect.size.height);

	if (_autofit != Autofit::None) {
		float scale = 1.0f;
		switch (_autofit) {
		case Autofit::None: break;
		case Autofit::Width: scale = texSize.width / _contentSize.width; break;
		case Autofit::Height: scale = texSize.height / _contentSize.height; break;
		case Autofit::Contain: scale = std::max(texSize.width / _contentSize.width, texSize.height / _contentSize.height); break;
		case Autofit::Cover: scale = std::min(texSize.width / _contentSize.width, texSize.height / _contentSize.height); break;
		}

		auto texSizeInView = Size2(texSize.width / scale, texSize.height / scale);
		targetOffsetX = targetOffsetX + (_contentSize.width - texSizeInView.width) * _autofitPos.x;
		targetOffsetY = targetOffsetY + (_contentSize.height - texSizeInView.height) * _autofitPos.y;

		targetViewSpaceSize = Size2(texSizeInView.width * viewScale.x,
				texSizeInView.height * viewScale.y);

		targetScaleX =_textureRect.size.width;
		targetScaleY =_textureRect.size.height;
	}

	Mat4 targetTransform(
		targetScaleX, 0.0f, 0.0f, targetOffsetX,
		0.0f, targetScaleY, 0.0f, targetOffsetY,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	bool isDirty = false;

	if (_targetSize != targetViewSpaceSize) {
		isDirty = true;
		_targetSize = targetViewSpaceSize;
	}

	auto targetDepth = VectorSprite_getPseudoSdfOffset(_depthIndex);
	if (_savedSdfValue != targetDepth) {
		isDirty = true;
		_savedSdfValue = targetDepth;
	}

	_targetTransform = targetTransform;

	if (isDirty || _image->isDirty()) {
		_image->clearDirty();
		_result = nullptr;
		_deferredResult = nullptr;

		auto imageData = _image->popData();
		VectorCanvasConfig config;
		config.color = _displayedColor;
		config.quality = _quality;

		// Canvas uses pixel-wide extents, but for sdf we need dp-wide
		config.sdfBoundaryInset *= frame.request->getFrameConstraints().density;
		//config.sdfBoundaryOffset *= frame.request->getFrameConstraints().density;
		config.sdfBoundaryOffset = _savedSdfValue * frame.request->getFrameConstraints().density;

		if (_depthIndex > 0.0f) {
			config.forcePseudoSdf = true;
		}

		if (_deferred) {
			_deferredResult = VectorSprite_runDeferredVectorCavas(*_director->getApplication()->getQueue(),
					move(imageData), _targetSize, config, _waitDeferred);
			_result = nullptr;
		} else {
			auto canvas = VectorCanvas::getInstance();
			canvas->setConfig(config);
			_result = canvas->draw(move(imageData), _targetSize);
		}
		_vertexColorDirty = false; // color will be already applied
	}

	Mat4 scaleTransform;
	scaleTransform.scale(viewScale);
	scaleTransform.inverse();

	_targetTransform *= scaleTransform;

	auto isSolidImage = [&, this] {
		for (auto &it : _image->getPaths()) {
			if (it.second->isAntialiased()) {
				return false;
			}

			auto s = it.second->getStyle();
			switch (s) {
			case vg::DrawStyle::Fill:
				if (it.second->getFillOpacity() != 255) {
					return false;
				}
				break;
			case vg::DrawStyle::FillAndStroke:
				if (it.second->getFillOpacity() != 255) {
					return false;
				}
				if (it.second->getStrokeOpacity() != 255) {
					return false;
				}
				break;
			case vg::DrawStyle::Stroke:
				if (it.second->getStrokeOpacity() != 255) {
					return false;
				}
				break;
			default:
				break;
			}
		}
		return true;
	};

	auto isSolid = isSolidImage();
	if (isSolid != _imageIsSolid) {
		_materialDirty = true;
		_imageIsSolid = isSolid;
	}
}

void VectorSprite::updateVertexesColor() {
	if (_deferred) {
		if (_deferredResult) {
			_deferredResult->updateColor(_displayedColor);
		}
	} else {
		if (_result) {
			_result->updateColor(_displayedColor);
		}
	}
}

RenderingLevel VectorSprite::getRealRenderingLevel() const {
	auto level = _renderingLevel;
	if (level == RenderingLevel::Default) {
		if (_displayedColor.a < 1.0f || !_texture || _materialInfo.getLineWidth() != 0.0f) {
			level = RenderingLevel::Transparent;
		} else if (_colorMode.getMode() == core::ColorMode::Solid) {
			if (_texture->hasAlpha()) {
				level = RenderingLevel::Transparent;
			} else {
				level = _imageIsSolid ? RenderingLevel::Solid : RenderingLevel::Transparent;
			}
		} else {
			auto alphaMapping = _colorMode.getA();
			switch (alphaMapping) {
			case core::ComponentMapping::Identity:
				if (_texture->hasAlpha()) {
					level = RenderingLevel::Transparent;
				} else {
					level = _imageIsSolid ? RenderingLevel::Solid : RenderingLevel::Transparent;
				}
				break;
			case core::ComponentMapping::Zero:
				level = RenderingLevel::Transparent;
				break;
			case core::ComponentMapping::One:
				level = _imageIsSolid ? RenderingLevel::Solid : RenderingLevel::Transparent;
				break;
			default:
				level = RenderingLevel::Transparent;
				break;
			}
		}
	}
	return level;
}

bool VectorSprite::checkVertexDirty() const {
	auto targetDepth = VectorSprite_getPseudoSdfOffset(_depthIndex);
	return Sprite::checkVertexDirty() || _savedSdfValue != targetDepth;
}

}
