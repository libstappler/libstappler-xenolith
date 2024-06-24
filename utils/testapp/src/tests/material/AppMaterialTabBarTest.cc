/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "AppMaterialTabBarTest.h"

namespace stappler::xenolith::app {

bool MaterialTabBarTest::init() {
	if (!LayoutTest::init(LayoutName::MaterialTabBarTest, "")) {
		return false;
	}

	_background = addChild(Rc<MaterialBackground>::create(), ZOrder(-1));
	_background->setAnchorPoint(Anchor::Middle);

	auto source = Rc<material2d::MenuSource>::create();
	source->addButton("Test1", IconName::Action_bookmarks_solid, std::bind([] { }))->setSelected(true);
	source->addButton("Test2", IconName::Action_history_solid, std::bind([] { }));
	source->addButton("Test3", IconName::Action_list_solid, std::bind([] { }));

	_tabBar = _background->addChild(Rc<material2d::TabBar>::create(source,
			material2d::TabBar::ButtonStyle::TitleIcon, material2d::TabBar::BarStyle::Layout, material2d::TabBar::Alignment::Justify
	), ZOrder(1));
	_tabBar->setAnchorPoint(Anchor::MiddleTop);

	return true;
}

void MaterialTabBarTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_background->setContentSize(_contentSize);
	_background->setPosition(Vec2(_contentSize / 2.0f));

	_tabBar->setContentSize(Size2(std::min(_contentSize.width - 32.0f, 400.0f), 72.0f));
	_tabBar->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height - 96.0f));
}

}
