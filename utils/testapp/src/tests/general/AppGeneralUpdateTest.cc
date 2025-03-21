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

#include "AppGeneralUpdateTest.h"
#include "XL2dLayer.h"

namespace stappler::xenolith::app {

bool GeneralUpdateTest::init() {
	if (!LayoutTest::init(LayoutName::GeneralUpdateTest, "Gradient should spin monotonically")) {
		return false;
	}

	_background = addChild(Rc<Layer>::create(Color::White));
	_background->setAnchorPoint(Anchor::Middle);
	//_background->setVisible(false);

	scheduleUpdate();

	return true;
}

void GeneralUpdateTest::handleEnter(Scene *scene) {
	LayoutTest::handleEnter(scene);

	runAction(Rc<RenderContinuously>::create());
}

void GeneralUpdateTest::handleExit() {
	stopAllActions();

	LayoutTest::handleExit();
}

void GeneralUpdateTest::handleContentSizeDirty() {
	LayoutTest::handleContentSizeDirty();

	_background->setContentSize(_contentSize);
	_background->setPosition(_contentSize / 2.0f);
}

void GeneralUpdateTest::update(const UpdateTime &time) {
	LayoutTest::update(time);

	auto t = time.app % (5_sec).toMicros();

	if (_background) {
		_background->setGradient(SimpleGradient(Color::Red_500, Color::Green_500,
				Vec2::forAngle(M_PI * 2.0 * (float(t) / (5_sec).toMicros()))));
	}
}

}
