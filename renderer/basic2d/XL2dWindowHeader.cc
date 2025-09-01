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
#include "XL2dVectorSprite.h"
#include "XLAction.h"
#include "XLInputListener.h"
#include "XLDirector.h"
#include "XLAppWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

enum class WindowHeaderButtonType {
	Close,
	Maximize,
	Minimize,
	Fullscreen,
	ContextMenu,
};

static constexpr StringView s_windowHeaderClose =
		R"(<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24">
<path fill="white" d="M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z"/>
</svg>
)";

static constexpr StringView s_windowHeaderMinimize =
		R"(<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24">
<path fill="white" d="M6 19h12v2H6z"/>
</svg>
)";

static constexpr StringView s_windowHeaderMaximize =
		R"(<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24">
<path fill="white" d="M19,4H5C3.9,4,3,4.9,3,6v12c0,1.1,0.9,2,2,2h14c1.1,0,2-0.9,2-2V6C21,4.9,20.1,4,19,4z M19,18H5V6h14V18z"/>
</svg>
)";

static constexpr StringView s_windowHeaderMaximizeExit =
		R"(<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24">
<path fill="white" d="M3 5H1v16c0 1.1.9 2 2 2h16v-2H3V5zm18-4H7c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V3c0-1.1-.9-2-2-2zm0 16H7V3h14v14z"/>
</svg>
)";

static constexpr StringView s_windowHeaderFullscreen =
		R"(<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24">
<path fill="white" d="M7 14H5v5h5v-2H7v-3zm-2-4h2V7h3V5H5v5zm12 7h-3v2h5v-5h-2v3zM14 5v2h3v3h2V5h-5z"/>
</svg>
)";

static constexpr StringView s_windowHeaderFullscreenExit =
		R"(<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24">
<path fill="white" d="M5 16h3v3h2v-5H5v2zm3-8H5v2h5V5H8v3zm6 11h2v-3h3v-2h-5v5zm2-11V5h-2v5h5V8h-3z"/>
</svg>
)";

static constexpr StringView s_windowHeaderMenu =
		R"(<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24">
<path fill="white" d="M3 18h18v-2H3v2zm0-5h18v-2H3v2zm0-7v2h18V6H3z"/>
</svg>
)";

class WindowHeaderButton : public Node {
public:
	virtual ~WindowHeaderButton() = default;

	virtual bool init(WindowHeaderButtonType);

	virtual void handleContentSizeDirty() override;

	virtual void updateWindowState(WindowState);

protected:
	virtual void handleTap();

	WindowHeaderButtonType _type = WindowHeaderButtonType::Close;
	VectorSprite *_icon = nullptr;
	VectorSprite *_background = nullptr;
	WindowState _state = WindowState::None;
	bool _selected = false;
};

bool WindowHeaderButton::init(WindowHeaderButtonType type) {
	if (!Node::init()) {
		return false;
	}

	_type = type;

	switch (_type) {
	case WindowHeaderButtonType::Close:
		_icon = addChild(Rc<VectorSprite>::create(s_windowHeaderClose), ZOrder(2));
		break;
	case WindowHeaderButtonType::Minimize:
		_icon = addChild(Rc<VectorSprite>::create(s_windowHeaderMinimize), ZOrder(2));
		break;
	case WindowHeaderButtonType::Maximize:
		_icon = addChild(Rc<VectorSprite>::create(s_windowHeaderMaximize), ZOrder(2));
		break;
	case WindowHeaderButtonType::Fullscreen:
		_icon = addChild(Rc<VectorSprite>::create(s_windowHeaderFullscreen), ZOrder(2));
		break;
	case WindowHeaderButtonType::ContextMenu:
		_icon = addChild(Rc<VectorSprite>::create(s_windowHeaderMenu), ZOrder(2));
		break;
	}

	if (_icon) {
		_icon->setColor(Color::Grey_500);
	}

	_background = addChild(Rc<VectorSprite>::create(Size2(24.0f, 24.0f)), ZOrder(1));
	_background->getImage()
			->addPath()
			->openForWriting([](vg::PathWriter &writer) { writer.addCircle(12.0f, 12.0f, 12.0f); })
			.setStyle(vg::DrawStyle::Fill)
			.setFillColor(Color::White);
	_background->setColor(Color::White);

	auto l = addComponent(Rc<InputListener>::create());

	if (_type == WindowHeaderButtonType::ContextMenu) {
		l->setLayerFlags(WindowLayerFlags::WindowMenuLeft);
	}

	l->addMouseOverRecognizer([this](const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began:
			if (!_selected) {
				_selected = true;
				_background->stopAllActions();
				_background->runAction(Rc<TintTo>::create(0.1f, Color::Grey_300));
			}
			break;
		case GestureEvent::Ended:
			if (_selected) {
				_selected = false;
				_background->stopAllActions();
				_background->runAction(Rc<TintTo>::create(0.1f, Color::White));
			}
			break;
		default: break;
		}
		return true;
	});
	l->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated) {
			handleTap();
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);

	return true;
}

void WindowHeaderButton::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	if (_icon) {
		_icon->setAnchorPoint(Anchor::Middle);
		_icon->setContentSize(_contentSize - Size2(6.0f, 6.0f));
		_icon->setPosition(_contentSize / 2.0f);
	}

	if (_background) {
		_background->setAnchorPoint(Anchor::Middle);
		_background->setContentSize(_contentSize);
		_background->setPosition(_contentSize / 2.0f);
	}
}

void WindowHeaderButton::updateWindowState(WindowState state) {
	switch (_type) {
	case WindowHeaderButtonType::Close:
		setVisible(hasFlag(state, WindowState::AllowedClose));
		break;
	case WindowHeaderButtonType::Minimize:
		setVisible(hasFlag(state, WindowState::AllowedMinimize));
		break;
	case WindowHeaderButtonType::Maximize:
		if (hasFlagAll(state, WindowState::Maximized)) {
			_icon->setImage(Rc<VectorImage>::create(s_windowHeaderMaximizeExit));
		} else {
			_icon->setImage(Rc<VectorImage>::create(s_windowHeaderMaximize));
		}
		setVisible(hasFlag(state, WindowState::AllowedWindowMenu));
		break;
	case WindowHeaderButtonType::Fullscreen:
		if (hasFlagAll(state, WindowState::Fullscreen)) {
			_icon->setImage(Rc<VectorImage>::create(s_windowHeaderFullscreenExit));
		} else {
			_icon->setImage(Rc<VectorImage>::create(s_windowHeaderFullscreen));
		}
		setVisible(hasFlag(state, WindowState::AllowedFullscreen));
		break;
	case WindowHeaderButtonType::ContextMenu:
		setVisible(hasFlag(state, WindowState::AllowedWindowMenu));
		break;
	}
	_state = state;
}

void WindowHeaderButton::handleTap() {
	if (!_director) {
		return;
	}

	auto w = _director->getWindow();
	if (!w) {
		return;
	}

	switch (_type) {
	case WindowHeaderButtonType::Close: w->close(true); break;
	case WindowHeaderButtonType::Minimize: w->enableState(WindowState::Minimized); break;
	case WindowHeaderButtonType::Maximize:
		if (hasFlagAll(_state, WindowState::Maximized)) {
			w->disableState(WindowState::Maximized);
		} else {
			w->enableState(WindowState::Maximized);
		}
		break;
	case WindowHeaderButtonType::Fullscreen:
		if (hasFlagAll(_state, WindowState::Fullscreen)) {
			w->disableState(WindowState::Fullscreen);
		} else {
			w->enableState(WindowState::Fullscreen);
		}
		break;
	case WindowHeaderButtonType::ContextMenu: break;
	}
}

bool WindowDecorationsDefault::init() {
	if (!WindowDecorations::init()) {
		return false;
	}

	_header = addChild(Rc<Layer>::create(Color4F(0.0f, 0.0f, 0.0f, 0.2f)));
	_header->setAnchorPoint(Anchor::MiddleTop);
	_header->setVisible(true);

	auto l = _header->addComponent(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::MoveGrip | WindowLayerFlags::WindowMenuRight);

	_buttonClose = addChild(Rc<WindowHeaderButton>::create(WindowHeaderButtonType::Close));
	_buttonMaximize = addChild(Rc<WindowHeaderButton>::create(WindowHeaderButtonType::Maximize));
	_buttonMinimize = addChild(Rc<WindowHeaderButton>::create(WindowHeaderButtonType::Minimize));
	_buttonFullscreen =
			addChild(Rc<WindowHeaderButton>::create(WindowHeaderButtonType::Fullscreen));
	_buttonMenu = addChild(Rc<WindowHeaderButton>::create(WindowHeaderButtonType::ContextMenu));

	return true;
}

void WindowDecorationsDefault::handleContentSizeDirty() {
	WindowDecorations::handleContentSizeDirty();

	float buttonSize = HeaderHeight - 4.0f;
	float buttonPadding = 2.0f;

	_header->setContentSize(Size2(_contentSize.width, HeaderHeight));
	_header->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height));

	auto pos = Vec2(_contentSize.width - (HeaderHeight - buttonSize),
			_contentSize.height - HeaderHeight / 2.0f);
	auto size = Size2(buttonSize, buttonSize);
	auto increment = -(size.width + (HeaderHeight - buttonSize) + buttonPadding);
	auto anchor = Anchor::MiddleRight;

	if (StringView(_theme).starts_with("Aqua")) {
		pos.x = (HeaderHeight - buttonSize);
		increment = -increment;
		anchor = Anchor::MiddleLeft;
	}

	if (_buttonClose->isVisible()) {
		_buttonClose->setAnchorPoint(anchor);
		_buttonClose->setPosition(pos);
		_buttonClose->setContentSize(size);
		pos.x += increment;
	}

	if (_buttonMaximize->isVisible()) {
		_buttonMaximize->setAnchorPoint(anchor);
		_buttonMaximize->setPosition(pos);
		_buttonMaximize->setContentSize(size);
		pos.x += increment;
	}

	if (_buttonMinimize->isVisible()) {
		_buttonMinimize->setAnchorPoint(anchor);
		_buttonMinimize->setPosition(pos);
		_buttonMinimize->setContentSize(size);
		pos.x += increment;
	}

	if (_buttonFullscreen->isVisible()) {
		_buttonFullscreen->setAnchorPoint(anchor);
		_buttonFullscreen->setPosition(pos);
		_buttonFullscreen->setContentSize(size);
		pos.x += increment;
	}

	if (_buttonMenu->isVisible()) {
		_buttonMenu->setAnchorPoint(anchor);
		_buttonMenu->setPosition(pos);
		_buttonMenu->setContentSize(size);
	}
}

Padding WindowDecorationsDefault::getPadding() const { return Padding(HeaderHeight, 0.0f, 0.0f); }

void WindowDecorationsDefault::updateWindowState(WindowState state) {
	WindowDecorations::updateWindowState(state);

	auto allowedMove = hasFlag(state, WindowState::AllowedMove)
			&& !hasFlag(state, WindowState::Fullscreen)
			&& !hasFlagAll(state, WindowState::Maximized);

	_header->getComponentByType<InputListener>()->setEnabled(allowedMove);

	_buttonClose->updateWindowState(state);
	_buttonMaximize->updateWindowState(state);
	_buttonMinimize->updateWindowState(state);
	_buttonFullscreen->updateWindowState(state);
	_buttonMenu->updateWindowState(state);

	_contentSizeDirty = true;
}

void WindowDecorationsDefault::updateWindowTheme(const ThemeInfo &theme) {
	if (_theme != theme.iconTheme || _colorScheme != theme.colorScheme) {
		_theme = theme.iconTheme;
		_colorScheme = theme.colorScheme;
		_contentSizeDirty = true;

		_theme = "Aqua";
	}
}

} // namespace stappler::xenolith::basic2d
