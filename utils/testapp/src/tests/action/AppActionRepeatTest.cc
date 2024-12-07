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

#include "AppActionRepeatTest.h"

namespace stappler::xenolith::app {

bool ActionRepeatTest::init() {
	if (!LayoutTest::init(LayoutName::ActionRepeatTest, "")) {
		return false;
	}

	_buttonRun = addChild(Rc<ButtonWithLabel>::create("Run all", [&] () {
		runAll();
	}));
	_buttonRun->setAnchorPoint(Anchor::MiddleLeft);

	_buttonStop = addChild(Rc<ButtonWithLabel>::create("Stop all", [&] () {
		stopAll();
	}));
	_buttonStop->setAnchorPoint(Anchor::MiddleRight);

	_buttonPlus = addChild(Rc<ButtonWithLabel>::create("+", [&] () {
		if (_count < 16) {
			++ _count;
			_countLabel->setString(toString(_count));
		}
	}));
	_buttonPlus->setAnchorPoint(Anchor::MiddleRight);

	_buttonMinus = addChild(Rc<ButtonWithLabel>::create("-", [&] () {
		if (_count > 1) {
			-- _count;
			_countLabel->setString(toString(_count));
		}
	}));
	_buttonMinus->setAnchorPoint(Anchor::MiddleLeft);

	_countLabel = addChild(Rc<Label>::create());
	_countLabel->setAnchorPoint(Anchor::Middle);
	_countLabel->setString(toString(_count));
	_countLabel->setFontSize(font::FontSize(24));

	_currentLabel = addChild(Rc<Label>::create(), ZOrder(1));
	_currentLabel->setAnchorPoint(Anchor::Middle);
	_currentLabel->setString(toString(_current));
	_currentLabel->setFontSize(font::FontSize(24));

	_result = addChild(Rc<Layer>::create(Color::Red_300));
	_result->setAnchorPoint(Anchor::Middle);

	_checkboxForever = addChild(Rc<CheckboxWithLabel>::create("Forever", _forever, [this] (bool val) {
		_forever = val;
	}));
	_checkboxForever->setAnchorPoint(Anchor::MiddleLeft);

	return true;
}

void ActionRepeatTest::handleContentSizeDirty() {
	LayoutTest::handleContentSizeDirty();

	auto size = 28.0f * 0.0f;
	auto offset = size / 2.0f;

	_buttonRun->setPosition(_contentSize / 2.0f + Size2(-100.0f, offset + 72.0f));
	_buttonRun->setContentSize(Size2(98.0f, 36.0f));

	_buttonStop->setPosition(_contentSize / 2.0f + Size2(100.0f, offset + 72.0f));
	_buttonStop->setContentSize(Size2(98.0f, 36.0f));

	_buttonPlus->setPosition(_contentSize / 2.0f + Size2(100.0f, offset + 28.0f));
	_buttonPlus->setContentSize(Size2(64.0f, 36.0f));

	_buttonMinus->setPosition(_contentSize / 2.0f + Size2(-100.0f, offset + 28.0f));
	_buttonMinus->setContentSize(Size2(64.0f, 36.0f));

	_countLabel->setPosition(_contentSize / 2.0f + Size2(0.0f, offset + 28.0f));

	_checkboxForever->setPosition(_contentSize / 2.0f + Size2(-100.0f, offset - 12.0f));

	_currentLabel->setPosition(_contentSize / 2.0f + Size2(0.0f, offset - 50.0f));

	_result->setPosition(_contentSize / 2.0f + Size2(0.0f, offset - 50.0f));
	_result->setContentSize(Size2(0.0f, 36.0f));
}

void ActionRepeatTest::runAll() {
	stopAll();
	_result->setContentSize(Size2(0.0f, 36.0f));
	_current = 0;
	_currentLabel->setString(toString(_current));
	if (_forever) {
		_result->runAction(
			Rc<RepeatForever>::create(
				Rc<Sequence>::create(
					[this] {
						_result->setContentSize(Size2(0.0f, 36.0f));
					},
					Rc<ResizeTo>::create(2.0f, Size2(200.0f, 36.0f)),
					[this] {
						_currentLabel->setString(toString(++ _current));
					}
			)),
			"ActionRepeatTestResize"_tag);
	} else {
		_result->runAction(
			Rc<Repeat>::create(
				Rc<Sequence>::create(
					[this] {
						_result->setContentSize(Size2(0.0f, 36.0f));
					},
					Rc<ResizeTo>::create(2.0f, Size2(200.0f, 36.0f)),
					[this] {
						_currentLabel->setString(toString(++ _current));
					}
			), _count),
			"ActionRepeatTestResize"_tag);
	}
}

void ActionRepeatTest::stopAll() {
	_result->stopAllActionsByTag("ActionRepeatTestResize"_tag);
}

}
