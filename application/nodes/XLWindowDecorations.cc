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

#include "XLWindowDecorations.h"
#include "XLEventListener.h"
#include "XLInputListener.h"
#include "XLDirector.h"
#include "XLScene.h"
#include "XLAppWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

bool WindowDecorations::init() {
	if (!Node::init()) {
		return false;
	}

	InputListener *l = nullptr;

	_resizeTopLeft = addChild(Rc<Node>::create());
	_resizeTopLeft->setAnchorPoint(Anchor::BottomRight);
	_resizeTopLeft->setVisible(true);
	l = _resizeTopLeft->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::ResizeTopLeftGrip);
	l->setCursor(WindowCursor::ResizeTopLeft);

	_resizeTop = addChild(Rc<Node>::create());
	_resizeTop->setAnchorPoint(Anchor::MiddleBottom);
	_resizeTop->setVisible(true);
	l = _resizeTop->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::ResizeTopGrip);
	l->setCursor(WindowCursor::ResizeTop);

	_resizeTopRight = addChild(Rc<Node>::create());
	_resizeTopRight->setAnchorPoint(Anchor::BottomLeft);
	_resizeTopRight->setVisible(true);
	l = _resizeTopRight->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::ResizeTopRightGrip);
	l->setCursor(WindowCursor::ResizeTopRight);

	_resizeRight = addChild(Rc<Node>::create());
	_resizeRight->setAnchorPoint(Anchor::MiddleLeft);
	_resizeRight->setVisible(true);
	l = _resizeRight->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::ResizeRightGrip);
	l->setCursor(WindowCursor::ResizeRight);

	_resizeBottomRight = addChild(Rc<Node>::create());
	_resizeBottomRight->setAnchorPoint(Anchor::TopLeft);
	_resizeBottomRight->setVisible(true);
	l = _resizeBottomRight->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::ResizeBottomRightGrip);
	l->setCursor(WindowCursor::ResizeBottomRight);

	_resizeBottom = addChild(Rc<Node>::create());
	_resizeBottom->setAnchorPoint(Anchor::MiddleTop);
	_resizeBottom->setVisible(true);
	l = _resizeBottom->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::ResizeBottomGrip);
	l->setCursor(WindowCursor::ResizeBottom);

	_resizeBottomLeft = addChild(Rc<Node>::create());
	_resizeBottomLeft->setAnchorPoint(Anchor::TopRight);
	_resizeBottomLeft->setVisible(true);
	l = _resizeBottomLeft->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::ResizeBottomLeftGrip);
	l->setCursor(WindowCursor::ResizeBottomLeft);

	_resizeLeft = addChild(Rc<Node>::create());
	_resizeLeft->setAnchorPoint(Anchor::MiddleRight);
	_resizeLeft->setVisible(true);
	l = _resizeLeft->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::ResizeLeftGrip);
	l->setCursor(WindowCursor::ResizeLeft);

	l = addComponent(Rc<InputListener>::create());
	l->setWindowStateCallback([this](WindowState state, WindowState changes) {
		if (state != _currentState) {
			updateWindowState(state);
		}
		return true;
	});

	auto el = addComponent(Rc<EventListener>::create());
	el->listenForEvent(AppThread::onThemeInfo, [this](const Event &event) {
		updateWindowTheme(event.getObject<AppThread>()->getThemeInfo());
	});

	return true;
}

bool WindowDecorations::shouldBePresentedOnScene(Scene *scene) const {
	auto window = scene->getDirector()->getWindow();
	if (hasFlag(window->getInfo()->flags, WindowCreationFlags::UserSpaceDecorations)
			&& !hasFlag(window->getWindowState(), core::WindowState::Fullscreen)) {
		return true;
	}
	return false;
}

void WindowDecorations::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	const float resizeBarWidth = 12.0f;
	const float cornerWidth = 16.0f;
	const float cornerInset = 4.0f;

	_resizeTopLeft->setContentSize(Size2(cornerWidth, cornerWidth));
	_resizeTopLeft->setPosition(Vec2(cornerInset, _contentSize.height - cornerInset));

	_resizeTop->setContentSize(Size2(_contentSize.width - cornerInset * 2.0f, resizeBarWidth));
	_resizeTop->setPosition(Vec2(_contentSize.width / 2, _contentSize.height));

	_resizeTopRight->setContentSize(Size2(cornerWidth, cornerWidth));
	_resizeTopRight->setPosition(
			Vec2(_contentSize.width - cornerInset, _contentSize.height - cornerInset));

	_resizeRight->setContentSize(Size2(resizeBarWidth, _contentSize.height - cornerInset * 2.0f));
	_resizeRight->setPosition(Vec2(_contentSize.width, _contentSize.height / 2));

	_resizeBottomRight->setContentSize(Size2(cornerWidth, cornerWidth));
	_resizeBottomRight->setPosition(Vec2(_contentSize.width - cornerInset, cornerInset));

	_resizeBottom->setContentSize(Size2(_contentSize.width - cornerInset * 2.0f, resizeBarWidth));
	_resizeBottom->setPosition(Vec2(_contentSize.width / 2, 0.0f));

	_resizeBottomLeft->setContentSize(Size2(cornerWidth, cornerWidth));
	_resizeBottomLeft->setPosition(Vec2(cornerInset, cornerInset));

	_resizeLeft->setContentSize(Size2(resizeBarWidth, _contentSize.height - cornerInset * 2.0f));
	_resizeLeft->setPosition(Vec2(0.0f, _contentSize.height / 2));
}

void WindowDecorations::handleLayout(Node *parent) {
	Node::handleLayout(parent);

	if (!shouldBePresentedOnScene(parent->getScene())) {
		setVisible(false);
		return;
	}

	auto cs = parent->getContentSize();

	setVisible(true);
	setContentSize(Size2(cs.width, cs.height));
	setPosition(cs / 2.0f);
	setAnchorPoint(Anchor::Middle);

	auto newState = parent->getDirector()->getWindow()->getWindowState();
	if (newState != _currentState) {
		updateWindowState(parent->getDirector()->getWindow()->getWindowState());
	}
	updateWindowTheme(parent->getDirector()->getApplication()->getThemeInfo());
}

void WindowDecorations::updateWindowState(WindowState state) {
	_currentState = state;

	bool allowedResize = hasFlag(state, WindowState::AllowedResize)
			&& !hasFlag(state, WindowState::Fullscreen)
			&& !hasFlagAll(state, WindowState::Maximized);

	_resizeTopLeft->setVisible(allowedResize && !hasFlag(state, WindowState::TiledTopLeft));
	_resizeTop->setVisible(allowedResize && !hasFlag(state, WindowState::TiledTop));
	_resizeTopRight->setVisible(allowedResize && !hasFlag(state, WindowState::TiledTopRight));
	_resizeRight->setVisible(allowedResize && !hasFlag(state, WindowState::TiledRight));
	_resizeBottomRight->setVisible(allowedResize && !hasFlag(state, WindowState::TiledBottomRight));
	_resizeBottom->setVisible(allowedResize && !hasFlag(state, WindowState::TiledBottom));
	_resizeBottomLeft->setVisible(allowedResize && !hasFlag(state, WindowState::TiledBottomLeft));
	_resizeLeft->setVisible(allowedResize && !hasFlag(state, WindowState::TiledLeft));
}

void WindowDecorations::updateWindowTheme(const ThemeInfo &theme) {
	log::debug("WindowDecorations", "updateWindowTheme: ", theme.colorScheme);
}

} // namespace stappler::xenolith
