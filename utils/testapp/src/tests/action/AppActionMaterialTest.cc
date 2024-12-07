/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "AppActionMaterialTest.h"

namespace stappler::xenolith::app {

bool ActionMaterialTest::init() {
	if (!LayoutTest::init(LayoutName::ActionMaterialTest, "")) {
		return false;
	}

	_slider = addChild(Rc<SliderWithLabel>::create("Time: 1.0", 0.0f, [this] (float value) {
		for (auto &it : _nodes) {
			it->setTime(1.0f + 9.0f * value);
		}
		_slider->setString(toString("Time: ", 1.0f + 9.0f * value));
	}));
	_slider->setAnchorPoint(Anchor::Middle);

	_button = addChild(Rc<ButtonWithLabel>::create("Run all", [&] () {
		for (auto &it : _nodes) {
			it->run();
		}
	}));
	_button->setAnchorPoint(Anchor::Middle);

	auto node = addChild(Rc<ActionEaseNode>::create("Standard:", [this] (Rc<ActionInterval> &&a) {
		return makeAction(material2d::EasingType::Standard, move(a));
	}));
	node->setAnchorPoint(Anchor::Middle);
	_nodes.emplace_back(node);

	node = addChild(Rc<ActionEaseNode>::create("StandardDecelerate:", [this] (Rc<ActionInterval> &&a) {
		return makeAction(material2d::EasingType::StandardDecelerate, move(a));
	}));
	node->setAnchorPoint(Anchor::Middle);
	_nodes.emplace_back(node);

	node = addChild(Rc<ActionEaseNode>::create("StandardAccelerate:", [this] (Rc<ActionInterval> &&a) {
		return makeAction(material2d::EasingType::StandardAccelerate, move(a));
	}));
	node->setAnchorPoint(Anchor::Middle);
	_nodes.emplace_back(node);

	node = addChild(Rc<ActionEaseNode>::create("Emphasized:", [this] (Rc<ActionInterval> &&a) {
		return makeAction(material2d::EasingType::Emphasized, move(a));
	}));
	node->setAnchorPoint(Anchor::Middle);
	_nodes.emplace_back(node);

	node = addChild(Rc<ActionEaseNode>::create("EmphasizedDecelerate:", [this] (Rc<ActionInterval> &&a) {
		return makeAction(material2d::EasingType::EmphasizedDecelerate, move(a));
	}));
	node->setAnchorPoint(Anchor::Middle);
	_nodes.emplace_back(node);

	node = addChild(Rc<ActionEaseNode>::create("EmphasizedAccelerate:", [this] (Rc<ActionInterval> &&a) {
		return makeAction(material2d::EasingType::EmphasizedAccelerate, move(a));
	}));
	node->setAnchorPoint(Anchor::Middle);
	_nodes.emplace_back(node);

	return true;
}

void ActionMaterialTest::handleContentSizeDirty() {
	LayoutTest::handleContentSizeDirty();

	auto size = 28.0f * _nodes.size();
	auto offset = size / 2.0f;

	_slider->setPosition(_contentSize / 2.0f + Size2(0.0f, offset + 36.0f));
	_slider->setContentSize(Size2(200.0f, 24.0f));

	_button->setPosition(_contentSize / 2.0f + Size2(0.0f, offset + 72.0f));
	_button->setContentSize(Size2(200.0f, 36.0f));

	for (auto &it : _nodes) {
		it->setPosition(_contentSize / 2.0f + Size2(72.0f, offset));
		it->setContentSize(Size2(std::min(_contentSize.width - 160.0f, 600.0f), 24.0f));
		offset -= 28.0f;
	}
}

Rc<ActionInterval> ActionMaterialTest::makeAction(material2d::EasingType type, Rc<ActionInterval> &&a) const {
	return material2d::makeEasing(move(a), type);
}

}
