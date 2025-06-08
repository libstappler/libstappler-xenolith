/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "AppInputTextTest.h"
#include "XLInputListener.h"
#include "XLDirector.h"
#include "XLTextInputManager.h"

namespace stappler::xenolith::app {

bool InputTextTest::init() {
	if (!LayoutTest::init(LayoutName::InputTextTest, "")) {
		return false;
	}

	_background = addChild(Rc<Layer>::create(Color::Grey_200));
	_background->setAnchorPoint(Anchor::Middle);

	_label = addChild(Rc<Label>::create());
	_label->setAnchorPoint(Anchor::Middle);
	_label->setFontSize(20);
	_label->setColor(Color::Grey_500);
	_label->setString("Placeholder");

	_inputHandler.onData = [this](const TextInputState &state) {
		if (_state.enabled != state.enabled) {
			if (state.enabled) {
				_background->setColor(Color::Red_100);
				_label->setColor(Color::Black);
			} else {
				_background->setColor(Color::Grey_200);
				if (state.empty()) {
					_label->setString("Placeholder");
				}
				_label->setColor(Color::Grey_500);
				_inputAcquired = false;
			}
		}

		_state = state;

		std::cout << "InputTextTest: onData: " << string::toUtf8<Interface>(state.getStringView())
				  << " " << state.cursor.start << ":" << state.cursor.length << "\n";
		if (!_state.empty()) {
			_label->setString(state.getStringView());
		}
	};

	auto l = addComponent(Rc<InputListener>::create());
	l->addTapRecognizer([this](const GestureTap &tap) {
		handleTap(tap.pos);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);

	return true;
}

void InputTextTest::handleContentSizeDirty() {
	LayoutTest::handleContentSizeDirty();

	_background->setPosition(_contentSize / 2.0f);
	_background->setContentSize(_contentSize * 0.7f);

	_label->setPosition(_contentSize / 2.0f);
}

void InputTextTest::handleTap(const Vec2 &pos) {
	if (!_inputAcquired) {
		if (_background->isTouched(pos)) {
			auto manager = _director->getTextInputManager();
			if (_inputHandler.run(manager, _state.getRequest())) {
				_inputAcquired = true;
			}
		}
	} else {
		_inputHandler.cancel();
	}
}

} // namespace stappler::xenolith::app
