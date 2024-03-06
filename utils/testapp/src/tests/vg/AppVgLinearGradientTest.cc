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

#include "AppVgLinearGradientTest.h"
#include "XL2dLinearGradient.h"

namespace stappler::xenolith::app {

bool VgLinearGradientTest::init() {
	if (!LayoutTest::init(LayoutName::VgLinearGradient, "")) {
		return false;
	}

	_sprite = addChild(Rc<Sprite>::create("xenolith-1-480.png"));
	//_sprite->setAutofit(Sprite::Autofit::Contain);
	_sprite->setSamplerIndex(Sprite::SamplerIndexDefaultFilterLinear);
	_sprite->setAnchorPoint(Anchor::Middle);

	updateAngle(0.5f);

	_sliderAngle = addChild(Rc<SliderWithLabel>::create(toString("Angle: ", _angle),
			(_angle + 180.0_to_rad) / 360.0_to_rad, [this] (float val) {
		updateAngle(val);
	}));
	_sliderAngle->setAnchorPoint(Anchor::TopLeft);
	_sliderAngle->setContentSize(Size2(128.0f, 32.0f));

	return true;
}

void VgLinearGradientTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_sprite->setPosition(_contentSize / 2.0f);
	_sprite->setContentSize(Size2(750.0f, 600.0f));

	_sliderAngle->setPosition(Vec2(16.0f, _contentSize.height - 16.0f));
}

void VgLinearGradientTest::updateAngle(float val) {
	auto q = val * 360.0_to_rad - 180.0_to_rad;

	_angle = q;

	if (_sliderAngle) {
		_sliderAngle->setString(toString("Quality: ", q));
	}

	Vec2 center(0.5f, 0.5f);
	Vec2 angle = Vec2::forAngle(_angle);

	Vec2 start = center - angle * 0.5f;
	Vec2 end = center + angle * 0.5f;

	auto g = Rc<LinearGradient>::create(start, end, Vector<GradientStep>{
		GradientStep{0.0f, 1.0f, Color::Blue_500},
		GradientStep{0.45f, 0.0f, Color::Red_500},
		GradientStep{0.55f, 0.0f, Color::Blue_500},
		GradientStep{1.0f, 1.0f, Color::Red_500},
	});

	_sprite->setLinearGradient(move(g));
}

}
