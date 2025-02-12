/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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
#include "XL2dSprite.h"
#include "XLAction.h"

namespace stappler::xenolith::app {

bool Renderer2dAnimationTest::init() {
	if (!LayoutTest::init(LayoutName::Renderer2dAnimationTest, "2d animation test")) {
		return false;
	}

	_sprite = addChild(Rc<Sprite>::create(), ZOrder(1));
	_sprite->setTextureAutofit(Autofit::Contain);
	_sprite->setAnchorPoint(Anchor::Middle);
	_sprite->setSamplerIndex(SamplerIndex::DefaultFilterNearest);

	_checkbox = addChild(Rc<CheckboxWithLabel>::create("Use linear filtration", false, [this] (bool val) {
		_sprite->setSamplerIndex(val ? SamplerIndex::DefaultFilterLinear : SamplerIndex::DefaultFilterNearest);
	}), ZOrder(2));
	_checkbox->setAnchorPoint(Anchor::Middle);

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

		StringView dirPath = "resources/anim";

		Vector<FilePath> paths;
		auto animPath = filesystem::loadableResourcePath<Interface>(dirPath);
		if (animPath.empty()) {
			log::error("Renderer2dAnimationTest", "Fail to find resource path for: ", dirPath);
			return;
		}

		filesystem::ftw(animPath, [&] (StringView path, bool isFile) {
			if (isFile) {
				paths.emplace_back(FilePath(path.pdup(builder.getPool())));
			}
		});

		std::sort(paths.begin(), paths.end());

		if (auto d = builder.addImage(name, core::ImageInfo(core::ImageFormat::R8G8B8A8_UNORM, core::ImageType::Image3D,
				core::ImageUsage::Sampled), makeSpanView(paths))) {
			if (auto tmp = cache->addTemporaryResource(Rc<core::Resource>::create(move(builder)))) {
				_sprite->setTexture(Rc<Texture>::create(d, tmp));
				_resource = tmp;
			}
		}
	}

	_sprite->runAction(Rc<RepeatForever>::create(Rc<ActionProgress>::create(5.0f, [this] (float value) {
		Texture *tex = _sprite->getTexture();
		auto maxLayer = float(tex->getImageData()->arrayLayers.get());

		_sprite->setTextureLayer(progress(0.0f, maxLayer, value));
	})));
}

void Renderer2dAnimationTest::handleExit() {
	LayoutTest::handleExit();
}

void Renderer2dAnimationTest::handleContentSizeDirty() {
	LayoutTest::handleContentSizeDirty();

	if (_sprite) {
		_sprite->setContentSize(_contentSize * 0.75f);
		_sprite->setPosition(_contentSize / 2.0f);
	}

	_checkbox->setContentSize(Size2(36.0f, 36.0f));
	_checkbox->setPosition(Vec2(_contentSize.width / 2.0f - 80.0f, 64.0f));
}

}
