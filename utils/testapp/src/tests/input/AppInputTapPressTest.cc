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

#include "AppInputTapPressTest.h"
#include "XLInputListener.h"

namespace stappler::xenolith::app {

bool InputTapPressTestNode::init(StringView text) {
	auto color = Color::Red_500;
	if (!Layer::init(Color::Red_500)) {
		return false;
	}

	_label = addChild(Rc<Label>::create(), ZOrder(1));
	_label->setString(toString(text, ": ", _index));
	_label->setAnchorPoint(Anchor::Middle);
	_label->setFontSize(24);
	_label->setFontWeight(font::FontWeight::Bold);
	_label->setColor(color.text());

	_text = text.str<Interface>();

	return true;
}

void InputTapPressTestNode::handleContentSizeDirty() {
	Layer::handleContentSizeDirty();

	_label->setPosition(_contentSize / 2.0f);
}

void InputTapPressTestNode::handleTap() {
	++_index;

	Color color(Color::Tone(_index % 16), Color::Level::b500);

	setColor(color);
	_label->setColor(color.text());
	_label->setString(toString(_text, ": ", _index));
}

bool InputTapPressTest::init() {
	if (!LayoutTest::init(LayoutName::InputTapPressTest, "Tap on node to change its color")) {
		return false;
	}

	InputListener *l = nullptr;

	_nodeTap = addChild(Rc<InputTapPressTestNode>::create("Tap"));
	_nodeTap->setAnchorPoint(Anchor::Middle);
	l = _nodeTap->addComponent(Rc<InputListener>::create());
	l->addTapRecognizer([node = _nodeTap](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated && tap.count == 1) {
			node->handleTap();
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);

	_nodeDoubleTap = addChild(Rc<InputTapPressTestNode>::create("Double tap"));
	_nodeDoubleTap->setAnchorPoint(Anchor::Middle);
	l = _nodeDoubleTap->addComponent(Rc<InputListener>::create());
	l->addTapRecognizer([node = _nodeDoubleTap](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated && tap.count == 2) {
			node->handleTap();
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 2);

	_nodePress = addChild(Rc<InputTapPressTestNode>::create("Press"));
	_nodePress->setAnchorPoint(Anchor::Middle);
	l = _nodePress->addComponent(Rc<InputListener>::create());
	l->addPressRecognizer([node = _nodePress](const GesturePress &tap) {
		if (tap.event == GestureEvent::Ended) {
			node->handleTap();
		}
		return true;
	});

	_nodeLongPress = addChild(Rc<InputTapPressTestNode>::create("Long press"));
	_nodeLongPress->setAnchorPoint(Anchor::Middle);
	l = _nodeLongPress->addComponent(Rc<InputListener>::create());
	l->addPressRecognizer([node = _nodeLongPress](const GesturePress &tap) {
		if (tap.event == GestureEvent::Activated) {
			node->handleTap();
		}
		return true;
	});

	_nodeTick = addChild(Rc<InputTapPressTestNode>::create("Press tick"));
	_nodeTick->setAnchorPoint(Anchor::Middle);
	l = _nodeTick->addComponent(Rc<InputListener>::create());
	l->addPressRecognizer([node = _nodeTick](const GesturePress &tap) {
		if (tap.event == GestureEvent::Activated) {
			node->handleTap();
		}
		return true;
	}, TapIntervalAllowed, true);

	return true;
}

void InputTapPressTest::handleContentSizeDirty() {
	LayoutTest::handleContentSizeDirty();

	float nodeHeight = 64.0f;

	Vec2 center(_contentSize / 2.0f);
	Size2 nodeSize(std::min(_contentSize.width / 2.0f, 256.0f), nodeHeight);

	_nodeTap->setContentSize(nodeSize);
	_nodeTap->setPosition(center + Vec2(0.0f, ((nodeHeight + 4.0f) / 2.0f) * 3.0f));

	_nodeDoubleTap->setContentSize(nodeSize);
	_nodeDoubleTap->setPosition(center + Vec2(0.0f, ((nodeHeight + 4.0f) / 2.0f) * 1.0f));

	_nodePress->setContentSize(nodeSize);
	_nodePress->setPosition(center + Vec2(0.0f, -((nodeHeight + 4.0f) / 2.0f) * 1.0f));

	_nodeLongPress->setContentSize(nodeSize);
	_nodeLongPress->setPosition(center + Vec2(0.0f, -((nodeHeight + 4.0f) / 2.0f) * 3.0f));

	_nodeTick->setContentSize(nodeSize);
	_nodeTick->setPosition(center + Vec2(0.0f, -((nodeHeight + 4.0f) / 2.0f) * 5.0f));
}

} // namespace stappler::xenolith::app
