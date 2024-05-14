/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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

#include "AppVgIconTest.h"

namespace stappler::xenolith::app {

bool VgIconTest::init() {
	if (!LayoutTest::init(LayoutName::VgIconTest, "")) {
		return false;
	}

	float initialQuality = 2.0f;
	float initialScale = 2.0f;

	do {
		auto image = Rc<VectorImage>::create(Size2(24, 24));

		_sprite = addChild(Rc<VectorSprite>::create(move(image)), ZOrder(0));
		_sprite->setContentSize(Size2(256, 256));
		_sprite->setAnchorPoint(Anchor::Middle);
		_sprite->setColor(Color::Black);
		_sprite->setOpacity(0.5f);
		_sprite->setQuality(initialQuality);
		_sprite->setScale(initialScale);
	} while (0);

	do {
		auto image = Rc<VectorImage>::create(Size2(24, 24));

		_triangles = addChild(Rc<VectorSprite>::create(move(image)), ZOrder(1));
		_triangles->setContentSize(Size2(256, 256));
		_triangles->setAnchorPoint(Anchor::Middle);
		_triangles->setColor(Color::Green_500);
		_triangles->setOpacity(0.5f);
		_triangles->setLineWidth(1.0f);
		_triangles->setQuality(initialQuality);
		_triangles->setVisible(false);
		_triangles->setScale(initialScale);
	} while (0);

	_spriteLayer = addChild(Rc<LayerRounded>::create(Color::Grey_100, 20.0f), ZOrder(-1));
	_spriteLayer->setContentSize(Size2(256, 256));
	_spriteLayer->setAnchorPoint(Anchor::Middle);
	_spriteLayer->setDepthIndex(1.0);
	_spriteLayer->setVisible(false);

	_label = addChild(Rc<Label>::create());
	_label->setFontSize(32);
	_label->setString(getIconName(_currentName));
	_label->setAnchorPoint(Anchor::MiddleTop);

	_info = addChild(Rc<Label>::create());
	_info->setFontSize(24);
	_info->setString("Test");
	_info->setAnchorPoint(Anchor::MiddleTop);

	_sliderQuality = addChild(Rc<SliderWithLabel>::create(toString("Quality: ", initialQuality),
			(initialQuality - 0.1f) / 4.9f, [this] (float val) {
		updateQualityValue(val);
	}));
	_sliderQuality->setAnchorPoint(Anchor::TopLeft);
	_sliderQuality->setContentSize(Size2(128.0f, 32.0f));

	_sliderScale = addChild(Rc<SliderWithLabel>::create(toString("Scale: ", initialScale),
			(initialScale - 0.1f) / 2.9f, [this] (float val) {
		updateScaleValue(val);
	}));
	_sliderScale->setAnchorPoint(Anchor::TopLeft);
	_sliderScale->setContentSize(Size2(128.0f, 32.0f));

	_checkboxVisible = addChild(Rc<CheckboxWithLabel>::create("Triangles", false, [this] (bool value) {
		_triangles->setVisible(value);
	}));
	_checkboxVisible->setAnchorPoint(Anchor::TopLeft);
	_checkboxVisible->setContentSize(Size2(32.0f, 32.0f));

	_checkboxAntialias = addChild(Rc<CheckboxWithLabel>::create("Antialias", _antialias, [this] (bool value) {
		updateAntialiasValue(value);
	}));
	_checkboxAntialias->setAnchorPoint(Anchor::TopLeft);
	_checkboxAntialias->setContentSize(Size2(32.0f, 32.0f));

	auto l = _sprite->addInputListener(Rc<InputListener>::create());
	l->addTouchRecognizer([this] (const GestureData &data) -> bool {
		if (data.event == GestureEvent::Ended) {
			if (data.input->data.button == InputMouseButton::Mouse8 || data.input->data.button == InputMouseButton::MouseScrollRight) {
				if (_currentName == IconName::Action_3d_rotation_outline) {
					updateIcon(IconName::Toggle_toggle_on_solid);
				} else {
					updateIcon(IconName(toInt(_currentName) - 1));
				}
			} else if (data.input->data.button == InputMouseButton::Mouse9 || data.input->data.button == InputMouseButton::MouseScrollLeft) {
				if (_currentName == IconName::Toggle_toggle_on_solid) {
					updateIcon(IconName::Action_3d_rotation_outline);
				} else {
					updateIcon(IconName(toInt(_currentName) + 1));
				}
			}
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::MouseScrollLeft, InputMouseButton::MouseScrollRight, InputMouseButton::Mouse8, InputMouseButton::Mouse9}));

	l->addKeyRecognizer([this] (const GestureData &ev) {
		if (ev.event == GestureEvent::Ended) {
			if (ev.input->data.key.keycode == InputKeyCode::LEFT) {
				if (_currentName == IconName::Action_3d_rotation_outline) {
					updateIcon(IconName::Toggle_toggle_on_solid);
				} else {
					updateIcon(IconName(toInt(_currentName) - 1));
				}
			} else if (ev.input->data.key.keycode == InputKeyCode::RIGHT) {
				if (_currentName == IconName::Toggle_toggle_on_solid) {
					updateIcon(IconName::Action_3d_rotation_outline);
				} else {
					updateIcon(IconName(toInt(_currentName) + 1));
				}
			}
		}
		return true;
	}, InputListener::makeKeyMask({InputKeyCode::LEFT, InputKeyCode::RIGHT}));

	scheduleUpdate();
	updateIcon(_currentName);

	return true;
}

void VgIconTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_sprite->setPosition(Size2(_contentSize.width / 2.0f, _contentSize.height / 2.0f));
	_triangles->setPosition(Size2(_contentSize.width / 2.0f, _contentSize.height / 2.0f));
	_spriteLayer->setPosition(Size2(_contentSize.width / 2.0f, _contentSize.height / 2.0f));

	_label->setPosition(Vec2(_contentSize / 2.0f) - Vec2(0.0f, 128.0f));
	_info->setPosition(Vec2(_contentSize / 2.0f) - Vec2(0.0f, 180.0f));

	_sliderQuality->setPosition(Vec2(16.0f, _contentSize.height - 16.0f));
	_sliderScale->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 48.0f));

	_checkboxVisible->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 96.0f));
	_checkboxAntialias->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 144.0f));
}

void VgIconTest::update(const UpdateTime &t) {
	LayoutTest::update(t);

	uint32_t ntriangles = _sprite->getTrianglesCount();
	uint32_t nvertexes = _sprite->getVertexesCount();

	_info->setString(toString("V: ", nvertexes, "; T: ", ntriangles));
}

void VgIconTest::setDataValue(Value &&data) {
	if (data.isInteger("icon")) {
		auto icon = IconName(data.getInteger("icon"));
		if (icon != _currentName) {
			updateIcon(icon);
			return;
		}
	}

	LayoutTest::setDataValue(move(data));
}

void VgIconTest::updateIcon(IconName name) {
	_currentName = name;
	_label->setString(toString(getIconName(_currentName), " ", toInt(_currentName), "/", toInt(IconName::Toggle_toggle_on_solid)));

	StringView imageData =
R"svg(
<svg height='20pt' version='1.1' width='20pt' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink'>
<path transform="translate(0 12)" d='
m1.44 -3.578181
v-0.261818c0 -2.759999 1.352727 -3.152726 1.90909 -3.152726
c0.261818 0 0.72 0.065455 0.96 0.436364
c-0.163636 0 -0.6 0 -0.6 0.490909
c0 0.338182 0.261818 0.501818 0.501818 0.501818
c0.174545 0 0.501818 -0.098182 0.501818 -0.523636
c0 -0.654545 -0.48 -1.178181 -1.385454 -1.178181
c-1.396363 0 -2.86909 1.407272 -2.86909 3.818181
c0 2.912726 1.265454 3.687272 2.279999 3.687272
c1.210909 0 2.247272 -1.025454 2.247272 -2.465454
c0 -1.385454 -0.970909 -2.432727 -2.181818 -2.432727
c-0.741818 0 -1.145454 0.556363 -1.363636 1.08
z'/>
</svg>
)svg";

	[[maybe_unused]] StringView pathData(
			//"M 10,8 L 21,8 L 21,5 C 21,3.9000000953674316 20.100000381469727,3 19,3 L 10,3 L 10,8 Z "
//"M 3,8 L 8,8 L 8,3 L 5,3 C 3.9000000953674316,3 3,3.9000000953674316 3,5 L 3,8 Z "
//"M 5,21 L 8,21 L 8,10 L 3,10 L 3,19 C 3,20.100000381469727 3.9000000953674316,21 5,21 Z "
//"M 13,22 L 9,18 L 13,14 Z "
"M 14,13 L 18,9 L 22,13 Z "
"M 14.579999923706055,19 L 17,13 L 19,13 Z"
	);

	do {
		_sprite->clear();
		_sprite->setAutofit(Sprite::Autofit::Contain);
		_sprite->setImage(Rc<vg::VectorImage>::create(imageData));
		/*auto path = _sprite->addPath();
		//path->getPath()->init(pathData);
		getIconData(_currentName, [&] (BytesView bytes) {
			path->getPath()->init(bytes);
		});
		path->setWindingRule(vg::Winding::EvenOdd);
		path->setAntialiased(_antialias);
		auto t = Mat4::IDENTITY;
		t.scale(1, -1, 1);
		t.translate(0, -24, 0);
		path->setTransform(t);*/
	} while (0);

	do {
		_triangles->clear();
		/*auto path = _triangles->addPath();
		//path->getPath()->init(pathData);
		getIconData(_currentName, [&] (BytesView bytes) {
			path->getPath()->init(bytes);
		});
		path->setWindingRule(vg::Winding::EvenOdd);
		path->setAntialiased(false);
		auto t = Mat4::IDENTITY;
		t.scale(1, -1, 1);
		t.translate(0, -24, 0);
		path->setTransform(t);*/
	} while (0);

	LayoutTest::setDataValue(Value({
		pair("icon", Value(toInt(_currentName)))
	}));
}

void VgIconTest::updateQualityValue(float value) {
	auto q = 0.1f + 4.9f * value;

	_sliderQuality->setString(toString("Quality: ", q));
	_sprite->setQuality(q);
	_triangles->setQuality(q);
}

void VgIconTest::updateScaleValue(float value) {
	auto q = 0.1f + 2.9f * value;

	_sliderScale->setString(toString("Scale: ", q));
	_sprite->setScale(q);
	_triangles->setScale(q);
}

void VgIconTest::updateAntialiasValue(bool value) {
	if (_antialias != value) {
		_antialias = value;
		updateIcon(_currentName);
	}
}

}
