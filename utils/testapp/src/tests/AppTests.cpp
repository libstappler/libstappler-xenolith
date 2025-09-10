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

#include "XLCommon.h" // IWYU pragma: keep
#include "AppTests.h"

#include "AppRootLayout.cc"

#include "utils/AppUtilsStorageTest.cc"
#include "utils/AppUtilsNetworkTest.cc"
#include "utils/AppUtilsAssetTest.cc"
#include "utils/AppUtilsWindowStateTest.cc"
#include "config/AppConfigMenu.cc"
#include "config/AppConfigPresentModeSwitcher.cc"

#include "2d/Renderer2dAnimationTest.h"
#include "2d/Renderer2dParticleTest.h"
#include "action/AppActionEaseTest.h"
#include "action/AppActionMaterialTest.h"
#include "action/AppActionRepeatTest.h"

#include "general/AppGeneralLabelTest.h"
#include "general/AppGeneralUpdateTest.h"
#include "general/AppGeneralZOrderTest.h"
#include "general/AppGeneralTransparencyTest.h"
#include "general/AppGeneralAutofitTest.h"
#include "general/AppGeneralTemporaryResourceTest.h"
#include "general/AppGeneralScissorTest.h"

#include "input/AppInputTouchTest.h"
#include "input/AppInputKeyboardTest.h"
#include "input/AppInputTapPressTest.h"
#include "input/AppInputSwipeTest.h"
#include "input/AppInputTextTest.h"
#include "input/AppInputPinchTest.h"

#include "material/AppMaterialColorPickerTest.h"
#include "material/AppMaterialDynamicFontTest.h"
#include "material/AppMaterialNodeTest.h"
#include "material/AppMaterialButtonTest.h"
#include "material/AppMaterialInputFieldTest.h"
#include "material/AppMaterialToolbarTest.h"
#include "material/AppMaterialMenuTest.h"
#include "material/AppMaterialTabBarTest.h"

#include "vg/AppVgTessTest.h"
#include "vg/AppVgIconTest.h"
#include "vg/AppVgIconList.h"
#include "vg/AppVgShadowTest.h"
#include "vg/AppVgSdfTest.h"
#include "vg/AppVgDynamicIcons.h"
#include "vg/AppVgLinearGradientTest.h"
#include "vg/AppVgImageAutofitTest.h"

namespace stappler::xenolith::app {

static Vector<MenuData> s_layouts{
	MenuData{LayoutName::Root, LayoutName::Root, "org.stappler.xenolith.test.Root", "Root",
		[](LayoutName name) {
	return Rc<RootLayout>::create(name,
			Vector<LayoutName>{
				LayoutName::GeneralTests,
				LayoutName::InputTests,
				LayoutName::ActionTests,
				LayoutName::VgTests,
				LayoutName::UtilsTests,
				LayoutName::MaterialTests,
				LayoutName::Renderer2dTests,
				LayoutName::Config,
			});
}},
	MenuData{LayoutName::GeneralTests, LayoutName::Root, "org.stappler.xenolith.test.GeneralTests",
		"General tests",
		[](LayoutName name) {
	return Rc<LayoutMenu>::create(name,
			Vector<LayoutName>{
				LayoutName::GeneralUpdateTest,
				LayoutName::GeneralZOrderTest,
				LayoutName::GeneralLabelTest,
				LayoutName::GeneralTransparencyTest,
				LayoutName::GeneralAutofitTest,
				LayoutName::GeneralTemporaryResourceTest,
				LayoutName::GeneralScissorTest,
			});
}},
	MenuData{LayoutName::InputTests, LayoutName::Root, "org.stappler.xenolith.test.InputTests",
		"Input tests",
		[](LayoutName name) {
	return Rc<LayoutMenu>::create(name,
			Vector<LayoutName>{
				LayoutName::InputTouchTest,
				LayoutName::InputKeyboardTest,
				LayoutName::InputTapPressTest,
				LayoutName::InputSwipeTest,
				LayoutName::InputTextTest,
				LayoutName::InputPinchTest,
			});
}},
	MenuData{LayoutName::ActionTests, LayoutName::Root, "org.stappler.xenolith.test.ActionTests",
		"Action tests",
		[](LayoutName name) {
	return Rc<LayoutMenu>::create(name,
			Vector<LayoutName>{
				LayoutName::ActionEaseTest,
				LayoutName::ActionMaterialTest,
				LayoutName::ActionRepeatTest,
			});
}},
	MenuData{LayoutName::VgTests, LayoutName::Root, "org.stappler.xenolith.test.VgTests",
		"VG tests",
		[](LayoutName name) {
	return Rc<LayoutMenu>::create(name,
			Vector<LayoutName>{
				LayoutName::VgTessTest,
				LayoutName::VgIconTest,
				LayoutName::VgIconList,
				LayoutName::VgShadowTest,
				LayoutName::VgSdfTest,
				LayoutName::VgDynamicIcons,
				LayoutName::VgLinearGradient,
				LayoutName::VgImageAutofitTest,
			});
}},
	MenuData{LayoutName::UtilsTests, LayoutName::Root, "org.stappler.xenolith.test.UtilsTests",
		"Utils tests",
		[](LayoutName name) {
	return Rc<LayoutMenu>::create(name,
			Vector<LayoutName>{
				LayoutName::UtilsStorageTest,
				LayoutName::UtilsNetworkTest,
				LayoutName::UtilsAssetTest,
				LayoutName::UtilsWindowStateTest,
			});
}},
	MenuData{LayoutName::MaterialTests, LayoutName::Root,
		"org.stappler.xenolith.test.MaterialTests", "Material tests",
		[](LayoutName name) {
	return Rc<LayoutMenu>::create(name,
			Vector<LayoutName>{
				LayoutName::MaterialColorPickerTest,
				LayoutName::MaterialDynamicFontTest,
				LayoutName::MaterialNodeTest,
				LayoutName::MaterialButtonTest,
				LayoutName::MaterialInputFieldTest,
				LayoutName::MaterialToolbarTest,
				LayoutName::MaterialMenuTest,
				LayoutName::MaterialTabBarTest,
			});
}},
	MenuData{LayoutName::Renderer2dTests, LayoutName::Root,
		"org.stappler.xenolith.test.Renderer2dTests", "2d renderer tests",
		[](LayoutName name) {
	return Rc<LayoutMenu>::create(name,
			Vector<LayoutName>{
				LayoutName::Renderer2dAnimationTest,
				LayoutName::Renderer2dParticleTest,
			});
}},

	MenuData{LayoutName::Config, LayoutName::Root, "org.stappler.xenolith.test.Config", "Config",
		[](LayoutName name) { return Rc<ConfigMenu>::create(); }},
	MenuData{LayoutName::GeneralUpdateTest, LayoutName::GeneralTests,
		"org.stappler.xenolith.test.GeneralUpdateTest", "Update test",
		[](LayoutName name) { return Rc<GeneralUpdateTest>::create(); }},
	MenuData{LayoutName::GeneralZOrderTest, LayoutName::GeneralTests,
		"org.stappler.xenolith.test.GeneralZOrderTest", "Z Order test",
		[](LayoutName name) { return Rc<GeneralZOrderTest>::create(); }},
	MenuData{LayoutName::GeneralLabelTest, LayoutName::GeneralTests,
		"org.stappler.xenolith.test.GeneralLabelTest", "Label test",
		[](LayoutName name) { return Rc<GeneralLabelTest>::create(); }},
	MenuData{LayoutName::GeneralTransparencyTest, LayoutName::GeneralTests,
		"org.stappler.xenolith.test.GeneralTransparencyTest", "Transparency Test",
		[](LayoutName name) { return Rc<GeneralTransparencyTest>::create(); }},
	MenuData{LayoutName::GeneralAutofitTest, LayoutName::GeneralTests,
		"org.stappler.xenolith.test.GeneralAutofitTest", "Autofit Test",
		[](LayoutName name) { return Rc<GeneralAutofitTest>::create(); }},
	MenuData{LayoutName::GeneralTemporaryResourceTest, LayoutName::GeneralTests,
		"org.stappler.xenolith.test.GeneralTemporaryResourceTest", "Temporary Resource Test",
		[](LayoutName name) { return Rc<GeneralTemporaryResourceTest>::create(); }},
	MenuData{LayoutName::GeneralScissorTest, LayoutName::GeneralTests,
		"org.stappler.xenolith.test.GeneralScissorTest", "Scissor Test",
		[](LayoutName name) { return Rc<GeneralScissorTest>::create(); }},

	MenuData{LayoutName::InputTouchTest, LayoutName::InputTests,
		"org.stappler.xenolith.test.InputTouchTest", "Touch test",
		[](LayoutName name) { return Rc<InputTouchTest>::create(); }},
	MenuData{LayoutName::InputKeyboardTest, LayoutName::InputTests,
		"org.stappler.xenolith.test.InputKeyboardTest", "Keyboard test",
		[](LayoutName name) { return Rc<InputKeyboardTest>::create(); }},
	MenuData{LayoutName::InputTapPressTest, LayoutName::InputTests,
		"org.stappler.xenolith.test.InputTapPressTest", "Tap Press test",
		[](LayoutName name) { return Rc<InputTapPressTest>::create(); }},
	MenuData{LayoutName::InputSwipeTest, LayoutName::InputTests,
		"org.stappler.xenolith.test.InputSwipeTest", "Swipe Test",
		[](LayoutName name) { return Rc<InputSwipeTest>::create(); }},
	MenuData{LayoutName::InputTextTest, LayoutName::InputTests,
		"org.stappler.xenolith.test.InputTextTest", "Text Test",
		[](LayoutName name) { return Rc<InputTextTest>::create(); }},
	MenuData{LayoutName::InputPinchTest, LayoutName::InputTests,
		"org.stappler.xenolith.test.InputPinchTest", "Pinch Test",
		[](LayoutName name) { return Rc<InputPinchTest>::create(); }},

	MenuData{LayoutName::ActionEaseTest, LayoutName::ActionTests,
		"org.stappler.xenolith.test.ActionEaseTest", "Ease test",
		[](LayoutName name) { return Rc<ActionEaseTest>::create(); }},
	MenuData{LayoutName::ActionMaterialTest, LayoutName::ActionTests,
		"org.stappler.xenolith.test.ActionMaterialTest", "Material test",
		[](LayoutName name) { return Rc<ActionMaterialTest>::create(); }},
	MenuData{LayoutName::ActionRepeatTest, LayoutName::ActionTests,
		"org.stappler.xenolith.test.ActionRepeatTest", "Repeat test",
		[](LayoutName name) { return Rc<ActionRepeatTest>::create(); }},

	MenuData{LayoutName::VgTessTest, LayoutName::VgTests, "org.stappler.xenolith.test.VgTessTest",
		"Tess test", [](LayoutName name) { return Rc<VgTessTest>::create(); }},
	MenuData{LayoutName::VgIconTest, LayoutName::VgTests, "org.stappler.xenolith.test.VgIconTest",
		"Icon test", [](LayoutName name) { return Rc<VgIconTest>::create(); }},
	MenuData{LayoutName::VgIconList, LayoutName::VgTests, "org.stappler.xenolith.test.VgIconList",
		"Icon list", [](LayoutName name) { return Rc<VgIconList>::create(); }},
	MenuData{LayoutName::VgShadowTest, LayoutName::VgTests,
		"org.stappler.xenolith.test.VgShadowTest", "Shadow Test",
		[](LayoutName name) { return Rc<VgShadowTest>::create(); }},
	MenuData{LayoutName::VgSdfTest, LayoutName::VgTests, "org.stappler.xenolith.test.VgSdfTest",
		"SDF Test", [](LayoutName name) { return Rc<VgSdfTest>::create(); }},
	MenuData{LayoutName::VgDynamicIcons, LayoutName::VgTests,
		"org.stappler.xenolith.test.VgDynamicIcons", "Dynamic icons",
		[](LayoutName name) { return Rc<VgDynamicIcons>::create(); }},
	MenuData{LayoutName::VgLinearGradient, LayoutName::VgTests,
		"org.stappler.xenolith.test.VgLinearGradient", "Linear gradient",
		[](LayoutName name) { return Rc<VgLinearGradientTest>::create(); }},
	MenuData{LayoutName::VgImageAutofitTest, LayoutName::VgTests,
		"org.stappler.xenolith.test.VgImageAutofitTest", "Image autofit",
		[](LayoutName name) { return Rc<VgImageAutofitTest>::create(); }},

	MenuData{LayoutName::UtilsStorageTest, LayoutName::UtilsTests,
		"org.stappler.xenolith.test.UtilsStorageTest", "Storage test",
		[](LayoutName name) { return Rc<UtilsStorageTest>::create(); }},
	MenuData{LayoutName::UtilsNetworkTest, LayoutName::UtilsTests,
		"org.stappler.xenolith.test.UtilsNetworkTest", "Network test",
		[](LayoutName name) { return Rc<UtilsNetworkTest>::create(); }},
	MenuData{LayoutName::UtilsAssetTest, LayoutName::UtilsTests,
		"org.stappler.xenolith.test.UtilsAssetTest", "Asset test",
		[](LayoutName name) { return Rc<UtilsAssetTest>::create(); }},
	MenuData{LayoutName::UtilsWindowStateTest, LayoutName::UtilsTests,
		"org.stappler.xenolith.test.UtilsWindowStateTest", "WindowState test",
		[](LayoutName name) { return Rc<UtilsWindowStateTest>::create(); }},

	MenuData{LayoutName::MaterialColorPickerTest, LayoutName::MaterialTests,
		"org.stappler.xenolith.test.MaterialColorPickerTest", "Color picker test",
		[](LayoutName name) { return Rc<MaterialColorPickerTest>::create(); }},
	MenuData{LayoutName::MaterialDynamicFontTest, LayoutName::MaterialTests,
		"org.stappler.xenolith.test.MaterialDynamicFontTest", "Dynamic font test",
		[](LayoutName name) { return Rc<MaterialDynamicFontTest>::create(); }},
	MenuData{LayoutName::MaterialNodeTest, LayoutName::MaterialTests,
		"org.stappler.xenolith.test.MaterialNodeTest", "Node test",
		[](LayoutName name) { return Rc<MaterialNodeTest>::create(); }},
	MenuData{LayoutName::MaterialButtonTest, LayoutName::MaterialTests,
		"org.stappler.xenolith.test.MaterialButtonTest", "Button test",
		[](LayoutName name) { return Rc<MaterialButtonTest>::create(); }},
	MenuData{LayoutName::MaterialInputFieldTest, LayoutName::MaterialTests,
		"org.stappler.xenolith.test.MaterialInputFieldTest", "Input field test",
		[](LayoutName name) { return Rc<MaterialInputFieldTest>::create(); }},
	MenuData{LayoutName::MaterialToolbarTest, LayoutName::MaterialTests,
		"org.stappler.xenolith.test.MaterialToolbarTest", "Toolbar test",
		[](LayoutName name) { return Rc<MaterialToolbarTest>::create(); }},
	MenuData{LayoutName::MaterialMenuTest, LayoutName::MaterialTests,
		"org.stappler.xenolith.test.MaterialMenuTest", "Menu test",
		[](LayoutName name) { return Rc<MaterialMenuTest>::create(); }},
	MenuData{LayoutName::MaterialTabBarTest, LayoutName::MaterialTests,
		"org.stappler.xenolith.test.MaterialTabBarTest", "Tab bar test",
		[](LayoutName name) { return Rc<MaterialTabBarTest>::create(); }},

	MenuData{LayoutName::Renderer2dAnimationTest, LayoutName::Renderer2dTests,
		"org.stappler.xenolith.test.Renderer2dAnimationTest", "Animation test",
		[](LayoutName name) { return Rc<Renderer2dAnimationTest>::create(); }},
	MenuData{LayoutName::Renderer2dParticleTest, LayoutName::Renderer2dTests,
		"org.stappler.xenolith.test.Renderer2dParticleTest", "Particle test",
		[](LayoutName name) { return Rc<Renderer2dParticleTest>::create(); }},
};

LayoutName getRootLayoutForLayout(LayoutName name) {
	for (auto &it : s_layouts) {
		if (it.layout == name) {
			return it.root;
		}
	}
	return LayoutName::Root;
}

StringView getLayoutNameId(LayoutName name) {
	for (auto &it : s_layouts) {
		if (it.layout == name) {
			return it.id;
		}
	}
	return StringView();
}

StringView getLayoutNameTitle(LayoutName name) {
	for (auto &it : s_layouts) {
		if (it.layout == name) {
			return it.title;
		}
	}
	return StringView();
}

LayoutName getLayoutNameById(StringView name) {
	for (auto &it : s_layouts) {
		if (it.id == name) {
			return it.layout;
		}
	}
	return LayoutName::Root;
}

Rc<SceneLayout2d> makeLayoutNode(LayoutName name) {
	for (auto &it : s_layouts) {
		if (it.layout == name) {
			return it.constructor(name);
		}
	}
	return nullptr;
}

} // namespace stappler::xenolith::app
