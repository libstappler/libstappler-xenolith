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

#include "XL2dWindowHeader.h"
#include "XL2dLayer.h"
#include "XLScene.h"
#include "XLDirector.h"
#include "XLAppWindow.h"
#include "XLInputListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

bool WindowHeader::init() {
	if (!Node::init()) {
		return false;
	}

	InputListener *l = nullptr;

	_move = addChild(Rc<Layer>::create(Color4F(0.0f, 0.0f, 0.0f, 0.2f)));
	_move->setAnchorPoint(Anchor::MiddleTop);
	_move->setVisible(true);
	l = _move->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::MoveGrip);

	_topLeft = addChild(Rc<Node>::create());
	_topLeft->setAnchorPoint(Anchor::BottomRight);
	_topLeft->setVisible(true);
	l = _topLeft->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::TopLeftGrip);
	l->setCursor(WindowCursor::ResizeTopLeft);

	_top = addChild(Rc<Node>::create());
	_top->setAnchorPoint(Anchor::MiddleBottom);
	_top->setVisible(true);
	l = _top->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::TopGrip);
	l->setCursor(WindowCursor::ResizeTop);

	_topRight = addChild(Rc<Node>::create());
	_topRight->setAnchorPoint(Anchor::BottomLeft);
	_topRight->setVisible(true);
	l = _topRight->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::TopRightGrip);
	l->setCursor(WindowCursor::ResizeTopRight);

	_right = addChild(Rc<Node>::create());
	_right->setAnchorPoint(Anchor::MiddleLeft);
	_right->setVisible(true);
	l = _right->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::RightGrip);
	l->setCursor(WindowCursor::ResizeRight);

	_bottomRight = addChild(Rc<Node>::create());
	_bottomRight->setAnchorPoint(Anchor::TopLeft);
	_bottomRight->setVisible(true);
	l = _bottomRight->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::BottomRightGrip);
	l->setCursor(WindowCursor::ResizeBottomRight);

	_bottom = addChild(Rc<Node>::create());
	_bottom->setAnchorPoint(Anchor::MiddleTop);
	_bottom->setVisible(true);
	l = _bottom->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::BottomGrip);
	l->setCursor(WindowCursor::ResizeBottom);

	_bottomLeft = addChild(Rc<Node>::create());
	_bottomLeft->setAnchorPoint(Anchor::TopRight);
	_bottomLeft->setVisible(true);
	l = _bottomLeft->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::BottomLeftGrip);
	l->setCursor(WindowCursor::ResizeBottomLeft);

	_left = addChild(Rc<Node>::create());
	_left->setAnchorPoint(Anchor::MiddleRight);
	_left->setVisible(true);
	l = _left->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::LeftGrip);
	l->setCursor(WindowCursor::ResizeLeft);

	return true;
}

bool WindowHeader::shouldBePresentedOnScene(Scene *scene) const {
	auto window = scene->getDirector()->getWindow();
	if (hasFlag(window->getInfo()->flags, WindowCreationFlags::UserSpaceDecorations)
			&& !hasFlag(window->getWindowState(), core::WindowState::Fullscreen)) {
		return true;
	}
	return false;
}

void WindowHeader::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	float width = 12.0f;
	float cornerWidth = 16.0f;
	float cornerInset = 4.0f;

	_move->setContentSize(Size2(_contentSize.width, 20.0f));
	_move->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height));

	_topLeft->setContentSize(Size2(cornerWidth, cornerWidth));
	_topLeft->setPosition(Vec2(cornerInset, _contentSize.height - cornerInset));

	_top->setContentSize(Size2(_contentSize.width - cornerInset * 2.0f, width));
	_top->setPosition(Vec2(_contentSize.width / 2, _contentSize.height));

	_topRight->setContentSize(Size2(cornerWidth, cornerWidth));
	_topRight->setPosition(
			Vec2(_contentSize.width - cornerInset, _contentSize.height - cornerInset));

	_right->setContentSize(Size2(width, _contentSize.height - cornerInset * 2.0f));
	_right->setPosition(Vec2(_contentSize.width, _contentSize.height / 2));

	_bottomRight->setContentSize(Size2(cornerWidth, cornerWidth));
	_bottomRight->setPosition(Vec2(_contentSize.width - cornerInset, cornerInset));

	_bottom->setContentSize(Size2(_contentSize.width - cornerInset * 2.0f, width));
	_bottom->setPosition(Vec2(_contentSize.width / 2, 0.0f));

	_bottomLeft->setContentSize(Size2(cornerWidth, cornerWidth));
	_bottomLeft->setPosition(Vec2(cornerInset, cornerInset));

	_left->setContentSize(Size2(width, _contentSize.height - cornerInset * 2.0f));
	_left->setPosition(Vec2(0.0f, _contentSize.height / 2));
}

void WindowHeader::handleLayout(Node *parent) {
	if (!shouldBePresentedOnScene(parent->getScene())) {
		setVisible(false);
		return;
	}

	auto cs = parent->getContentSize();

	setVisible(true);
	setContentSize(Size2(cs.width, cs.height));
	setPosition(cs / 2.0f);
	setAnchorPoint(Anchor::Middle);
}

} // namespace stappler::xenolith::basic2d
