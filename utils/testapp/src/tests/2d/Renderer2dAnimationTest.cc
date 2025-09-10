/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

#include "Renderer2dAnimationTest.h"
#include "SPFilepath.h"
#include "XL2dSprite.h"
#include "XLAction.h"
#include "XLDirector.h"
#include "XLTexture.h"

namespace stappler::xenolith::app {

bool Renderer2dAnimationTest::init() {
	if (!LayoutTest::init(LayoutName::Renderer2dAnimationTest, "2d animation test")) {
		return false;
	}

	_sprite1 = addChild(Rc<Sprite>::create(), ZOrder(1));
	_sprite1->setTextureAutofit(Autofit::Contain);
	_sprite1->setAnchorPoint(Anchor::Middle);
	_sprite1->setSamplerIndex(SamplerIndex::DefaultFilterNearest);

	_sprite2 = addChild(Rc<Sprite>::create(), ZOrder(1));
	_sprite2->setTextureAutofit(Autofit::Contain);
	_sprite2->setAnchorPoint(Anchor::Middle);
	_sprite2->setSamplerIndex(SamplerIndex::DefaultFilterLinear);

	return true;
}

void Renderer2dAnimationTest::handleEnter(Scene *scene) {
	LayoutTest::handleEnter(scene);

	auto cache = _director->getResourceCache();

	StringView name("external://resources/anim");

	if (auto res = cache->getTemporaryResource(name)) {
		_resource = res;
	} else {
		core::Resource::Builder builder(name);

		Vector<FileInfo> paths;

		filesystem::ftw(FileInfo{"resources/anim"}, [&](const FileInfo &path, FileType type) {
			if (type == FileType::File) {
				paths.emplace_back(FileInfo(path.path.pdup(builder.getPool()), path.category));
			}
			return true;
		});

		std::sort(paths.begin(), paths.end());

		if (auto d = builder.addImage(name,
					core::ImageInfo(core::ImageFormat::R8G8B8A8_UNORM, core::ImageType::Image3D,
							core::ImageUsage::Sampled),
					makeSpanView(paths))) {
			if (auto tmp = cache->addTemporaryResource(Rc<core::Resource>::create(move(builder)))) {
				auto tex = Rc<Texture>::create(d, tmp);
				_sprite1->setTexture(tex);
				_sprite2->setTexture(tex);
				_resource = tmp;
			}
		}
	}

	runAction(Rc<RepeatForever>::create(Rc<ActionProgress>::create(5.0f, [this](float value) {
		Texture *tex = _sprite1->getTexture();
		auto maxLayer1 = float(tex->getImageData()->arrayLayers.get());

		_sprite1->setTextureLayer(progress(0.0f, maxLayer1, value));
		_sprite2->setTextureLayer(progress(0.0f, maxLayer1, value));
	})));
}

void Renderer2dAnimationTest::handleExit() { LayoutTest::handleExit(); }

void Renderer2dAnimationTest::handleContentSizeDirty() {
	LayoutTest::handleContentSizeDirty();

	if (_sprite1) {
		_sprite1->setContentSize(_contentSize * 0.5f);
		_sprite1->setPosition(_contentSize / 2.0f - Vec2(-160.0f, 0.0f));
	}

	if (_sprite2) {
		_sprite2->setContentSize(_contentSize * 0.5f);
		_sprite2->setPosition(_contentSize / 2.0f + Vec2(-160.0f, 0.0f));
	}
}

} // namespace stappler::xenolith::app
