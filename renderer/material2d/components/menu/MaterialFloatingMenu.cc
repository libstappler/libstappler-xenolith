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

#include "MaterialFloatingMenu.h"
#include "MaterialMenuButton.h"
#include "XL2dSceneLayout.h"
#include "XLApplication.h"
#include "XLFontController.h"

namespace stappler::xenolith::material2d {

class FloatingMenuLayout : public SceneLayout2d {
public:
	virtual ~FloatingMenuLayout() { }

	virtual bool init(MenuSource *source, const Vec2 &globalOrigin, FloatingMenu::Binding b, Menu *root);

	virtual void onContentSizeDirty() override;

	virtual void onPushTransitionEnded(SceneContent2d *l, bool replace) override;
	virtual void onPopTransitionBegan(SceneContent2d *l, bool replace) override;

protected:
	void emplaceMenu(const Vec2 &o, FloatingMenu::Binding b);

	Layer *_layer = nullptr;
	FloatingMenu *_menu = nullptr;
	StyleContainer *_styleContainer = nullptr;
	Vec2 _globalOrigin;
	Size2 _fullSize;
	FloatingMenu::Binding _binding = FloatingMenu::Binding::Anchor;
};

bool FloatingMenuLayout::init(MenuSource *source, const Vec2 &globalOrigin, FloatingMenu::Binding b, Menu *root) {
	if (!SceneLayout2d::init()) {
		return false;
	}

	//_layer = addChild(Rc<Layer>::create(Color::Green_500));
	//_layer->setContentSize(Size2(100.0f, 100.0f));
	//_layer->setAnchorPoint(Anchor::MiddleBottom);

	_menu = addChild(Rc<FloatingMenu>::create(source, root));
	_menu->setAnchorPoint(Anchor::MiddleTop);

	_globalOrigin = globalOrigin;
	_binding = b;

	return true;
}

void FloatingMenuLayout::onContentSizeDirty() {
	SceneLayout2d::onContentSizeDirty();

	if (_layer) {
		_layer->setPosition(_contentSize / 2.0f);
	}

	//_menu->setPosition(_contentSize / 2.0f);
}

void FloatingMenuLayout::onPushTransitionEnded(SceneContent2d *l, bool replace) {
	SceneLayout2d::onPushTransitionEnded(l, replace);

	float w = _menu->getMenuWidth(this);
	float h = _menu->getMenuHeight(this, w);

	_fullSize = Size2(w, h);

	emplaceMenu(_globalOrigin, _binding);
}

void FloatingMenuLayout::onPopTransitionBegan(SceneContent2d *l, bool replace) {
	SceneLayout2d::onPopTransitionBegan(l, replace);
}

void FloatingMenuLayout::emplaceMenu(const Vec2 &origin, FloatingMenu::Binding b) {
	const float incr = Menu::MenuHorizontalIncrement;
	auto size = _sceneContent->getContentSize();

	switch (b) {
	case FloatingMenu::Binding::Relative:
		_menu->setPositionY(origin.y);
		if (origin.x < incr / 4) {
			_menu->setPositionX(incr / 4);
			_menu->setAnchorPoint(Vec2(0, 1.0f));
		} else if (origin.x > size.width - incr / 4) {
			_menu->setPositionX(size.width - incr / 4);
			_menu->setAnchorPoint(Vec2(1, 1.0f));
		} else {
			float rel = (origin.x - incr / 4) / (size.width - incr / 2);
			_menu->setPositionX(origin.x);
			_menu->setAnchorPoint(Vec2(rel, 1.0f));
		}
		_menu->setContentSize(Size2(1, 1));
		break;
	case FloatingMenu::Binding::OriginLeft:
		_menu->setPositionY(origin.y);
		if (origin.x - incr / 4 < _fullSize.width) {
			_menu->setAnchorPoint(Vec2(0, 1.0f));
			_menu->setPositionX(incr / 4);
		} else {
			_menu->setAnchorPoint(Vec2(1, 1.0f));
			_menu->setPositionX(origin.x);
		}
		_menu->setContentSize(Size2(_fullSize.width, 1));
		break;
	case FloatingMenu::Binding::OriginRight:
		_menu->setPositionY(origin.y);
		if (size.width - origin.x - incr / 4 < _fullSize.width) {
			_menu->setAnchorPoint(Vec2(1, 1.0f));
			_menu->setPositionX(size.width - incr / 4);
		} else {
			_menu->setAnchorPoint(Vec2(0, 1.0f));
			_menu->setPositionX(origin.x);
		}
		_menu->setContentSize(Size2(_fullSize.width, 1));
		break;
	case FloatingMenu::Binding::Anchor:
		_menu->setPosition(origin);

		break;
	}

	if (b != FloatingMenu::Binding::Anchor && b != FloatingMenu::Binding::Relative) {
		if (_fullSize.height > origin.y - incr / 4) {
			if (origin.y - incr / 4 < incr * 4) {
				if (_fullSize.height > incr * 4) {
					_fullSize.height = incr * 4;
				}

				_menu->setPositionY(_fullSize.height + incr / 4);
			} else {
				_fullSize.height = origin.y - incr / 4;
			}
		}
	} else if (b == FloatingMenu::Binding::Relative) {
		if (_fullSize.height > origin.y - incr / 4) {
			_menu->setAnchorPoint(Vec2(getAnchorPoint().x, (origin.y - incr / 4) / _fullSize.height));
		}
	}

	if (origin.y > size.height - incr / 4) {
		_menu->setPositionY(size.height - incr / 4);
	}

	//f->pushNode(this, std::bind(&FloatingMenu::close, this));

	//auto a = Rc<ResizeTo>::create(0.2f, _fullSize);
	//_menu->runAction(Rc<Sequence>::create(a, [this] {
		//_scroll->setVisible(true);
	//}));

	_menu->setContentSize(_fullSize);
}

void FloatingMenu::push(SceneContent2d *content, MenuSource *source, const Vec2 &globalOrigin, Binding b, Menu *root) {
	auto l = Rc<FloatingMenuLayout>::create(source, globalOrigin, b, root);

	content->pushOverlay(l);
}

bool FloatingMenu::init(MenuSource *source, Menu *root) {
	if (!Menu::init(source)) {
		return false;
	}

	setMenuButtonCallback(std::bind(&FloatingMenu::onMenuButton, this, std::placeholders::_1));

	_root = root;

	setElevation(Elevation(toInt(_root->getStyleOrigin().elevation) + 2));

	return true;
}

void FloatingMenu::setCloseCallback(const CloseCallback &cb) {
	_closeCallback = cb;
}
const FloatingMenu::CloseCallback & FloatingMenu::getCloseCallback() const {
	return _closeCallback;
}

void FloatingMenu::close() {
	stopAllActions();
	/*_scroll->setVisible(false);
	auto a = Rc<ResizeTo>::create(0.2f, (_binding == Binding::Relative?Size(1, 1):Size(_fullSize.width, 1)));
	runAction(action::sequence(a, [this] {
		if (_closeCallback) {
			_closeCallback();
		}
		Scene::getRunningScene()->popForegroundNode(this);
	}));*/
}

void FloatingMenu::closeRecursive() {
	if (_root) {
		stopAllActions();
		/*_scroll->setVisible(false);
		auto a = Rc<ResizeTo>::create(0.15f, (_binding == Binding::Relative?Size(1, 1):Size(_fullSize.width, 1)));
		runAction(action::sequence(a, [this] {
			auto r = _root;
			Scene::getRunningScene()->popForegroundNode(this);
			if (r) {
				r->closeRecursive();
			}
		}));*/
	} else {
		close();
	}
}

void FloatingMenu::onCapturedTap() {
	close();
}

float FloatingMenu::getMenuWidth(Node *root) {
	float minWidth = 0;
	auto &items = _menuListener->getSubscription()->getItems();
	for (auto &item : items) {
		if (item->getType() == MenuSourceItem::Type::Custom) {
			float w = static_cast<MenuSourceCustom *>(item.get())->getMinWidth();
			if (w > minWidth) {
				minWidth = w;
			}
		} else if (item->getType() == MenuSourceItem::Type::Button) {
			auto btn = static_cast<MenuSourceButton *>(item.get());
			auto c = Application::getInstance()->getExtension<font::FontController>();

			float w = MenuButton::getMaxWidthForButton(btn, c, root->getInputDensity());
			if (w > minWidth) {
				minWidth = w;
			}
		}
	}

	const float incr = MenuHorizontalIncrement;
	const auto &size = root->getContentSize();

	minWidth = incr * ceilf(minWidth / incr);
	if (minWidth > size.width - incr / 2) {
		minWidth = size.width - incr / 2;
	}

	return minWidth;
}

float FloatingMenu::getMenuHeight(Node *root, float width) {
	float height = MenuLeadingHeight + MenuTrailingHeight;
	auto &items = _menuListener->getSubscription()->getItems();
	for (auto &item : items) {
		if (item->getType() == MenuSourceItem::Type::Custom) {
			height += static_cast<MenuSourceCustom *>(item.get())->getHeight(this, width);
		} else if (item->getType() == MenuSourceItem::Type::Button) {
			height += MenuItemHeight;
		} else {
			height += MenuVerticalPadding;
		}
	}

	const float incr = MenuHorizontalIncrement;
	const auto &size = root->getContentSize();

	if (height > size.height - incr / 2) {
		height = size.height - incr / 2;
	}

	return height;
}

void FloatingMenu::onMenuButton(MenuButton *btn) {
	if (!btn->getMenuSourceButton()->getNextMenu()) {
		setEnabled(false);
		runAction(Rc<Sequence>::create(0.3f, std::bind(&FloatingMenu::closeRecursive, this)));
	}
}

}
