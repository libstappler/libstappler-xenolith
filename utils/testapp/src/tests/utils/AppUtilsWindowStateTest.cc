/**
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

#include "AppUtilsWindowStateTest.h"
#include "AppButton.h"
#include "XLInputListener.h"
#include "XLScene.h"
#include "XLDirector.h"
#include "XLInputDispatcher.h"
#include "XLAppWindow.h"

namespace stappler::xenolith::app {

class WindowStateNode : public Node {
public:
	virtual ~WindowStateNode() = default;

	virtual bool init(WindowState);

	virtual void handleContentSizeDirty() override;
	virtual void handleComponentsDirty() override;
	virtual void handleEnter(Scene *) override;

protected:
	void updateState(WindowState);
	void enableState();
	void disableState();

	Label *_nameLabel = nullptr;
	ButtonWithLabel *_enableButton = nullptr;
	ButtonWithLabel *_disableButton = nullptr;
	WindowState _stateFlag = WindowState::None;
};

// Use component to reduce InputDispatcher overhead
struct WindowStateComponent {
	static ComponentId Id;

	WindowState state = WindowState::None;
};

ComponentId WindowStateComponent::Id;

bool WindowStateNode::init(WindowState state) {
	if (!Node::init()) {
		return false;
	}

	// set flag to handle parent component changes
	_eventFlags |= NodeEventFlags::HandleComponents;

	_stateFlag = state;

	auto name = toString(_stateFlag);
	auto str = StringView(name);
	str.trimChars<StringView::WhiteSpace>();

	if (_stateFlag == WindowState::Maximized) {
		str = StringView("Maximized");
	}

	_nameLabel = addChild(Rc<Label>::create());
	_nameLabel->setAlignment(Label::TextAlign::Left);
	_nameLabel->setString(str);
	_nameLabel->setFontSize(20);
	_nameLabel->setAnchorPoint(Anchor::MiddleLeft);
	_nameLabel->setColor(Color::Grey_800);
	_nameLabel->setFontWeight(font::FontWeight::SemiBold);
	_nameLabel->setPersistentGlyphData(true);

	_enableButton = addChild(Rc<ButtonWithLabel>::create("Enable", [this]() { enableState(); }));
	_enableButton->setAnchorPoint(Anchor::MiddleRight);

	_disableButton = addChild(Rc<ButtonWithLabel>::create("Disable", [this]() { disableState(); }));
	_disableButton->setAnchorPoint(Anchor::MiddleRight);

	return true;
}

void WindowStateNode::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	_nameLabel->setPosition(Vec2(8.0f, _contentSize.height / 2.0f));

	_enableButton->setContentSize(Size2(82.0f, _contentSize.height - 4.0f));
	_enableButton->setPosition(Vec2(_contentSize.width - 94.0f, _contentSize.height / 2.0f));

	_disableButton->setContentSize(Size2(82.0f, _contentSize.height - 4.0f));
	_disableButton->setPosition(Vec2(_contentSize.width - 8.0f, _contentSize.height / 2.0f));
}

void WindowStateNode::handleComponentsDirty() {
	Node::handleComponentsDirty();
	findParentWithComponent<WindowStateComponent>(
			[&](NotNull<Node>, NotNull<const WindowStateComponent> c, uint32_t) {
		updateState(c->state);
		return false;
	});
}

void WindowStateNode::handleEnter(Scene *scene) {
	Node::handleEnter(scene);

	updateState(scene->getDirector()->getInputDispatcher()->getWindowState());
}

void WindowStateNode::updateState(WindowState state) {
	if (hasFlagAll(state, _stateFlag)) {
		_nameLabel->setColor(Color::Grey_800);
	} else {
		_nameLabel->setColor(Color::Grey_300);
	}
	auto updatableState = getDirector()->getWindow()->getUpdatableStateFlags();
	if (hasFlag(updatableState, _stateFlag)) {
		_enableButton->setVisible(true);
		_disableButton->setVisible(true);
	} else {
		_enableButton->setVisible(false);
		_disableButton->setVisible(false);
	}
}

void WindowStateNode::enableState() { getDirector()->getWindow()->enableState(_stateFlag); }

void WindowStateNode::disableState() { getDirector()->getWindow()->disableState(_stateFlag); }

bool UtilsWindowStateTest::init() {
	if (!LayoutTest::init(LayoutName::UtilsWindowStateTest, "")) {
		return false;
	}

	auto l = addSystem(Rc<InputListener>::create());
	l->setWindowStateCallback([this](WindowState state, WindowState changes) {
		handleWindowStateChanged(state);
		return true;
	});

	_scroll = addChild(Rc<ScrollView>::create(ScrollView::Vertical));
	_scroll->setAnchorPoint(Anchor::MiddleTop);
	_scroll->setIndicatorColor(Color::Grey_500);

	_controller = _scroll->setController(Rc<ScrollController>::create());

	return true;
}

void UtilsWindowStateTest::handleContentSizeDirty() {
	LayoutTest::handleContentSizeDirty();
	_scroll->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height));
	_scroll->setContentSize(Size2(std::min(512.0f, _contentSize.width), _contentSize.height));
}

void UtilsWindowStateTest::handleEnter(Scene *scene) {
	LayoutTest::handleEnter(scene);
	updateScroll();

	handleWindowStateChanged(scene->getDirector()->getInputDispatcher()->getWindowState());
}

void UtilsWindowStateTest::handleWindowStateChanged(WindowState state) {
	setOrUpdateComponent<WindowStateComponent>([&](NotNull<WindowStateComponent> c) {
		if (c->state != state) {
			c->state = state;
			return true;
		}
		return false;
	});
}

void UtilsWindowStateTest::updateScroll() {
	_controller->clear();
	for (auto v : flags(WindowState::All)) {
		if (v == WindowState::MaximizedVert) {
			v = WindowState::Maximized;
		} else if (v == WindowState::MaximizedHorz) {
			continue;
		}

		_controller->addItem([v](const ScrollController::Item &) -> Rc<Node> {
			return Rc<WindowStateNode>::create(v);
		}, 28.0f);
	}
}

} // namespace stappler::xenolith::app
