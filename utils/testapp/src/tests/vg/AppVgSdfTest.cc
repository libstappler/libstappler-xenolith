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

#include "AppVgSdfTest.h"
#include "XL2dCommandList.h"
#include "XLFrameInfo.h"

namespace stappler::xenolith::app {

class VgSdfTestCircle : public VectorSprite {
public:
	virtual ~VgSdfTestCircle() { }

	virtual bool init(bool);

protected:
	bool _sdfShadow = false;
};

class VgSdfTestRect : public VectorSprite {
public:
	virtual ~VgSdfTestRect() { }

	virtual bool init(bool, float radius = 0.0f);

protected:
	bool _sdfShadow = false;
	float _radius = 0.0f;
};

class VgSdfTestTriangle : public VectorSprite {
public:
	virtual ~VgSdfTestTriangle() { }

	virtual bool init(bool);

protected:
	bool _sdfShadow = false;
};

class VgSdfTestPolygon : public VectorSprite {
public:
	virtual ~VgSdfTestPolygon() { }

	virtual bool init(bool);

protected:
	bool _sdfShadow = false;
};

bool VgSdfTestCircle::init(bool value) {
	if (!VectorSprite::init(Size2(16, 16))) {
		return false;
	}

	_sdfShadow = value;
	_image->addPath()->openForWriting([] (vg::PathWriter &writer) { writer.addCircle(8, 8, 8); });

	setDepthIndex(4.0f);
	setColor(Color::Grey_100);
	setAnchorPoint(Anchor::Middle);

	return true;
}

bool VgSdfTestRect::init(bool value, float radius) {
	if (!VectorSprite::init(Size2(16, 8))) {
		return false;
	}

	_sdfShadow = value;
	_radius = radius;

	if (_radius > 0.0f) {
		_image->addPath()->openForWriting([&] (vg::PathWriter &writer) { writer.addRect(Rect(0, 0, 16, 8), radius, radius); });
	} else {
		_image->addPath()->openForWriting([&] (vg::PathWriter &writer) { writer.addRect(Rect(0, 0, 16, 8)); });
	}

	setDepthIndex(4.0f);
	setColor(Color::Grey_100);
	setAnchorPoint(Anchor::Middle);

	return true;
}

bool VgSdfTestPolygon::init(bool value) {
	if (!VectorSprite::init(Size2(16, 20))) {
		return false;
	}

	_sdfShadow = value;
	_image->addPath()->openForWriting([&] (vg::PathWriter &writer) {
		writer.moveTo(0.0f, 0.0f).lineTo(16, 20).lineTo(0, 20).lineTo(16, 0).closePath();
	}).setAntialiased(false);

	setDepthIndex(4.0f);
	setColor(Color::Grey_100);
	setAnchorPoint(Anchor::Middle);

	return true;
}

bool VgSdfTestTriangle::init(bool value) {
	if (!VectorSprite::init(Size2(16, 16))) {
		return false;
	}

	_sdfShadow = value;
	_image->addPath()->openForWriting([&] (vg::PathWriter &writer) {
		writer.moveTo(0.0f, 0.0f).lineTo(8.0f, 16.0f).lineTo(16.0f, 0.0f).closePath();
	}).setAntialiased(false);

	setDepthIndex(4.0f);
	setColor(Color::Grey_100);
	setAnchorPoint(Anchor::Middle);

	return true;
}

bool VgSdfTest::init() {
	if (!LayoutTest::init(LayoutName::VgSdfTest, "")) {
		return false;
	}

	bool testsVisible = true;

	_circleSprite = addChild(Rc<VgSdfTestCircle>::create(true));
	_circleSprite->setContentSize(Size2(64, 64));
	_circleSprite->setVisible(true);

	_circleTestSprite = addChild(Rc<VgSdfTestCircle>::create(false));
	_circleTestSprite->setContentSize(Size2(64, 64));
	_circleTestSprite->setVisible(testsVisible);

	_rectSprite = addChild(Rc<VgSdfTestRect>::create(true));
	_rectSprite->setContentSize(Size2(64, 32));
	_rectSprite->setVisible(true);

	_rectTestSprite = addChild(Rc<VgSdfTestRect>::create(false));
	_rectTestSprite->setContentSize(Size2(64, 32));
	_rectTestSprite->setVisible(testsVisible);

	_roundedRectSprite = addChild(Rc<VgSdfTestRect>::create(true, 2.0f));
	_roundedRectSprite->setContentSize(Size2(64, 32));
	_roundedRectSprite->setVisible(true);

	_roundedRectTestSprite = addChild(Rc<VgSdfTestRect>::create(false, 2.0f));
	_roundedRectTestSprite->setContentSize(Size2(64, 32));
	_roundedRectTestSprite->setVisible(testsVisible);

	_triangleSprite = addChild(Rc<VgSdfTestTriangle>::create(true));
	_triangleSprite->setContentSize(Size2(64, 64));
	_triangleSprite->setVisible(true);

	_triangleTestSprite = addChild(Rc<VgSdfTestTriangle>::create(false));
	_triangleTestSprite->setContentSize(Size2(64, 64));
	_triangleTestSprite->setVisible(testsVisible);

	_polygonSprite = addChild(Rc<VgSdfTestPolygon>::create(true));
	_polygonSprite->setContentSize(Size2(64, 80));
	_polygonSprite->setVisible(true);

	_polygonTestSprite = addChild(Rc<VgSdfTestPolygon>::create(false));
	_polygonTestSprite->setContentSize(Size2(64, 80));
	_polygonTestSprite->setVisible(testsVisible);

	float initialScale = 1.0f;
	float initialShadow = 4.0f;
	float maxShadow = 20.0f;
	float initialRotation = 0.0f;

	_sliderScaleX = addChild(Rc<SliderWithLabel>::create(toString("Scale X: ", initialScale),
			(initialScale - 0.1f) / 2.9f, [this] (float val) {
		// updateScaleValue(val);
		_circleSprite->setScaleX(val * 2.9f + 0.1f);
		_circleTestSprite->setScaleX(val * 2.9f + 0.1f);
		_rectSprite->setScaleX(val * 2.9f + 0.1f);
		_rectTestSprite->setScaleX(val * 2.9f + 0.1f);
		_roundedRectSprite->setScaleX(val * 2.9f + 0.1f);
		_roundedRectTestSprite->setScaleX(val * 2.9f + 0.1f);
		_triangleSprite->setScaleX(val * 2.9f + 0.1f);
		_triangleTestSprite->setScaleX(val * 2.9f + 0.1f);
		_polygonSprite->setScaleX(val * 2.9f + 0.1f);
		_polygonTestSprite->setScaleX(val * 2.9f + 0.1f);
		_sliderScaleX->setString(toString("Scale X: ", val * 2.9f + 0.1f));
	}));
	_sliderScaleX->setAnchorPoint(Anchor::TopLeft);
	_sliderScaleX->setContentSize(Size2(128.0f, 32.0f));

	_sliderScaleY = addChild(Rc<SliderWithLabel>::create(toString("Scale Y: ", initialScale),
			(initialScale - 0.1f) / 2.9f, [this] (float val) {
		_circleSprite->setScaleY(val * 2.9f + 0.1f);
		_circleTestSprite->setScaleY(val * 2.9f + 0.1f);
		_rectSprite->setScaleY(val * 2.9f + 0.1f);
		_rectTestSprite->setScaleY(val * 2.9f + 0.1f);
		_roundedRectSprite->setScaleY(val * 2.9f + 0.1f);
		_roundedRectTestSprite->setScaleY(val * 2.9f + 0.1f);
		_triangleSprite->setScaleY(val * 2.9f + 0.1f);
		_triangleTestSprite->setScaleY(val * 2.9f + 0.1f);
		_polygonSprite->setScaleY(val * 2.9f + 0.1f);
		_polygonTestSprite->setScaleY(val * 2.9f + 0.1f);
		_sliderScaleY->setString(toString("Scale Y: ", val * 2.9f + 0.1f));
	}));
	_sliderScaleY->setAnchorPoint(Anchor::TopLeft);
	_sliderScaleY->setContentSize(Size2(128.0f, 32.0f));

	_sliderShadow = addChild(Rc<SliderWithLabel>::create(toString("Shadow: ", initialShadow),
			initialShadow / maxShadow, [this, maxShadow] (float val) {
		_circleSprite->setDepthIndex(val * maxShadow);
		_circleTestSprite->setDepthIndex(val * maxShadow);
		_rectSprite->setDepthIndex(val * maxShadow);
		_rectTestSprite->setDepthIndex(val * maxShadow);
		_roundedRectSprite->setDepthIndex(val * maxShadow);
		_roundedRectTestSprite->setDepthIndex(val * maxShadow);
		_triangleSprite->setDepthIndex(val * maxShadow);
		_triangleTestSprite->setDepthIndex(val * maxShadow);
		_polygonSprite->setDepthIndex(val * maxShadow);
		_polygonTestSprite->setDepthIndex(val * maxShadow);
		_sliderShadow->setString(toString("Shadow: ", _circleSprite->getDepthIndex()));
	}));
	_sliderShadow->setAnchorPoint(Anchor::TopLeft);
	_sliderShadow->setContentSize(Size2(128.0f, 32.0f));

	_sliderRotation = addChild(Rc<SliderWithLabel>::create(toString("Rotation: ", initialRotation),
			initialRotation / (numbers::pi * 2.0f), [this] (float val) {
		_circleSprite->setRotation(val * numbers::pi * 2.0f);
		_circleTestSprite->setRotation(val * numbers::pi * 2.0f);
		_rectSprite->setRotation(val * numbers::pi * 2.0f);
		_rectTestSprite->setRotation(val * numbers::pi * 2.0f);
		_roundedRectSprite->setRotation(val * numbers::pi * 2.0f);
		_roundedRectTestSprite->setRotation(val * numbers::pi * 2.0f);
		_triangleSprite->setRotation(val * numbers::pi * 2.0f);
		_triangleTestSprite->setRotation(val * numbers::pi * 2.0f);
		_polygonSprite->setRotation(val * numbers::pi * 2.0f);
		_polygonTestSprite->setRotation(val * numbers::pi * 2.0f);
		_sliderRotation->setString(toString("Rotation: ", val * numbers::pi * 2.0f));
	}));
	_sliderRotation->setAnchorPoint(Anchor::TopLeft);
	_sliderRotation->setContentSize(Size2(128.0f, 32.0f));

	return true;
}

void VgSdfTest::handleEnter(Scene *scene) {
	LayoutTest::handleEnter(scene);

	auto light = Rc<SceneLight>::create(SceneLightType::Ambient, Vec2(0.0f, 0.0f), 1.5f, Color::White);

	auto content = dynamic_cast<SceneContent2d *>(_scene->getContent());
	content->removeAllLights();
	content->addLight(move(light));
}

void VgSdfTest::handleContentSizeDirty() {
	LayoutTest::handleContentSizeDirty();

	_sliderScaleX->setPosition(Vec2(16.0f, _contentSize.height - 16.0f));
	_sliderScaleY->setPosition(Vec2(384.0f + 16.0f, _contentSize.height - 16.0f));
	_sliderShadow->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 48.0f));
	_sliderRotation->setPosition(Vec2(384.0f + 16.0f, _contentSize.height - 16.0f - 48.0f));

	_circleSprite->setPosition(Vec2(_contentSize / 2.0f) + Vec2(_contentSize.width / 3.0f, 100.0f));
	_circleTestSprite->setPosition(Vec2(_contentSize / 2.0f) + Vec2(-_contentSize.width / 3.0f, 100.0f));

	_rectSprite->setPosition(Vec2(_contentSize / 2.0f) + Vec2(_contentSize.width / 3.0f, 0.0f));
	_rectTestSprite->setPosition(Vec2(_contentSize / 2.0f) + Vec2(-_contentSize.width / 3.0f, 0.0f));

	_roundedRectSprite->setPosition(Vec2(_contentSize / 2.0f) + Vec2(_contentSize.width / 3.0f, -100.0f));
	_roundedRectTestSprite->setPosition(Vec2(_contentSize / 2.0f) + Vec2(-_contentSize.width / 3.0f, -100.0f));

	_triangleSprite->setPosition(Vec2(_contentSize / 2.0f) + Vec2(_contentSize.width / 6.0f, 100.0f));
	_triangleTestSprite->setPosition(Vec2(_contentSize / 2.0f) + Vec2(-_contentSize.width / 6.0f, 100.0f));

	_polygonSprite->setPosition(Vec2(_contentSize / 2.0f) + Vec2(_contentSize.width / 6.0f, -40.0f));
	_polygonTestSprite->setPosition(Vec2(_contentSize / 2.0f) + Vec2(-_contentSize.width / 6.0f, -40.0f));
}

}
