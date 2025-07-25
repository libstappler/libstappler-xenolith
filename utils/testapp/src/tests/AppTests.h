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

#ifndef TEST_SRC_ROOT_APPTESTS_H_
#define TEST_SRC_ROOT_APPTESTS_H_

#include "XLCommon.h"
#include "AppWidgets.h"
#include "XL2dSceneLayout.h"

namespace stappler::xenolith::app {

enum class LayoutName {
	Root = 256 * 0,
	GeneralTests,
	InputTests,
	ActionTests,
	VgTests,
	UtilsTests,
	MaterialTests,
	Renderer2dTests,
	Config,

	GeneralUpdateTest = 256 * 1,
	GeneralZOrderTest,
	GeneralLabelTest,
	GeneralTransparencyTest,
	GeneralAutofitTest,
	GeneralTemporaryResourceTest,
	GeneralScissorTest,

	InputTouchTest = 256 * 2,
	InputKeyboardTest,
	InputTapPressTest,
	InputSwipeTest,
	InputTextTest,
	InputPinchTest,

	ActionEaseTest = 256 * 3,
	ActionMaterialTest,
	ActionRepeatTest,

	VgTessTest = 256 * 4,
	VgIconTest,
	VgIconList,
	VgShadowTest,
	VgSdfTest,
	VgDynamicIcons,
	VgLinearGradient,
	VgImageAutofitTest,

	UtilsStorageTest = 256 * 5,
	UtilsNetworkTest,
	UtilsAssetTest,

	MaterialColorPickerTest = 256 * 6,
	MaterialDynamicFontTest,
	MaterialNodeTest,
	MaterialButtonTest,
	MaterialInputFieldTest,
	MaterialToolbarTest,
	MaterialMenuTest,
	MaterialTabBarTest,

	Renderer2dAnimationTest = 256 * 7,
	Renderer2dParticleTest,
};

struct MenuData {
	LayoutName layout;
	LayoutName root;
	String id;
	String title;
	Function<Rc<basic2d::SceneLayout2d>(LayoutName)> constructor;
};

LayoutName getRootLayoutForLayout(LayoutName);
StringView getLayoutNameId(LayoutName);
StringView getLayoutNameTitle(LayoutName);
LayoutName getLayoutNameById(StringView);

Rc<basic2d::SceneLayout2d> makeLayoutNode(LayoutName);

} // namespace stappler::xenolith::app

#endif /* TEST_SRC_ROOT_APPTESTS_H_ */
