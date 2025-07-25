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

#include "AppLayoutMenuItem.h"
#include "XL2dLabel.h"
#include "XLInputListener.h"

namespace stappler::xenolith::app {

bool LayoutMenuItem::init(StringView str, Function<void()> &&cb) {
	if (!Layer::init(Color::Grey_100)) {
		return false;
	}

	_label = addChild(Rc<Label>::create(str), ZOrder(2));
	_label->setAlignment(Label::TextAlign::Center);
	_label->setFontWeight(font::FontWeight::Normal);
	_label->setAnchorPoint(Anchor::Middle);
	_label->setFontSize(26);
	_label->setPersistentLayout(true);

	auto l = addComponent(Rc<InputListener>::create());
	l->setTouchFilter([](const InputEvent &event, const InputListener::DefaultEventFilter &) {
		return true;
	});
	l->setViewLayerFlags(ViewLayerFlags::CursorPointer);

	/*l->addMouseOverRecognizer([this] (const GestureData &ev) {
		switch (ev.event) {
		case GestureEvent::Began:
			handleMouseEnter();
			return true;
			break;
		default:
			handleMouseLeave();
			return true;
			break;
		}
		return true;
	}, false);*/

	l->addMoveRecognizer([this](const GestureData &ev) {
		bool touched = isTouched(ev.input->currentLocation);
		if (touched != _focus) {
			_focus = touched;
			if (_focus) {
				handleMouseEnter();
			} else {
				handleMouseLeave();
			}
		}
		return true;
	}, false);
	l->addPressRecognizer([this](const GesturePress &press) {
		if (isTouched(press.pos)) {
			switch (press.event) {
			case GestureEvent::Began: return true; break;
			case GestureEvent::Activated: return true; break;
			case GestureEvent::Ended: return handlePress(); break;
			case GestureEvent::Cancelled: return true; break;
			}
		}
		return false;
	});

	_callback = sp::move(cb);

	return true;
}

void LayoutMenuItem::handleMouseEnter() { _label->setFontWeight(font::FontWeight::Bold); }

void LayoutMenuItem::handleMouseLeave() { _label->setFontWeight(font::FontWeight::Normal); }

bool LayoutMenuItem::handlePress() {
	if (_callback) {
		_callback();
	}
	return true;
}

void LayoutMenuItem::handleContentSizeDirty() {
	Layer::handleContentSizeDirty();

	_label->setPosition(_contentSize / 2.0f);
}

} // namespace stappler::xenolith::app
