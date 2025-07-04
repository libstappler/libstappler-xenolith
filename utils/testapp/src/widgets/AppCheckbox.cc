/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "AppCheckbox.h"
#include "XLInputListener.h"
#include "MaterialLabel.h"

namespace stappler::xenolith::app {

bool Checkbox::init(bool value, Function<void(bool)> &&cb) {
	if (!Layer::init(Color::Grey_200)) {
		return false;
	}

	_value = value;
	_callback = sp::move(cb);

	setColor(_backgroundColor);
	setContentSize(Size2(32.0f, 32.0f));

	_input = addComponent(Rc<InputListener>::create());
	_input->addTapRecognizer([this](const GestureTap &data) {
		switch (data.event) {
		case GestureEvent::Activated: setValue(!_value); break;
		default: break;
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);

	updateValue();

	return true;
}

void Checkbox::setValue(bool value) {
	if (_value != value) {
		_value = value;
		updateValue();
		if (_callback) {
			_callback(_value);
		}
	}
}

bool Checkbox::getValue() const { return _value; }

void Checkbox::setForegroundColor(const Color4F &color) {
	if (_foregroundColor != color) {
		_foregroundColor = color;
		updateValue();
	}
}

Color4F Checkbox::getForegroundColor() const { return _foregroundColor; }

void Checkbox::setBackgroundColor(const Color4F &color) {
	if (_backgroundColor != color) {
		_backgroundColor = color;
		updateValue();
	}
}

Color4F Checkbox::getBackgroundColor() const { return _backgroundColor; }

void Checkbox::updateValue() {
	if (_value) {
		setColor(_foregroundColor);
	} else {
		setColor(_backgroundColor);
	}
}

bool CheckboxWithLabel::init(StringView title, bool value, Function<void(bool)> &&cb) {
	if (!Checkbox::init(value, sp::move(cb))) {
		return false;
	}

	_label = addChild(
			Rc<material2d::TypescaleLabel>::create(material2d::TypescaleRole::HeadlineSmall));
	_label->setAnchorPoint(Anchor::MiddleLeft);
	_label->setString(title);

	return true;
}

void CheckboxWithLabel::handleContentSizeDirty() {
	Checkbox::handleContentSizeDirty();

	_label->setPosition(Vec2(_contentSize.width + 16.0f, _contentSize.height / 2.0f));
}

void CheckboxWithLabel::setLabelColor(const Color4F &color) { _label->setColor(color); }

} // namespace stappler::xenolith::app
