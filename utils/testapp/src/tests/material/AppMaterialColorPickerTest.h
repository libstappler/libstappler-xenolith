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

#ifndef TEST_SRC_TESTS_MATERIAL_APPMATERIALCOLORPICKERTEST_H_
#define TEST_SRC_TESTS_MATERIAL_APPMATERIALCOLORPICKERTEST_H_

#include "MaterialSurface.h"
#include "MaterialColorScheme.h"
#include "AppMaterialTest.h"
#include "AppMaterialColorPicker.h"
#include "AppSlider.h"
#include "XL2dLayerRounded.h"

namespace stappler::xenolith::app {

class MaterialColorSchemeNode : public Layer {
public:
	virtual ~MaterialColorSchemeNode() { }

	virtual bool init(material2d::ColorRole);

	virtual void handleContentSizeDirty() override;

	virtual void setSchemeColor(material2d::ThemeType, Color4F background, Color4F label);

protected:
	void updateLabels();

	Label *_labelName = nullptr;
	Label *_labelDesc = nullptr;
	material2d::ThemeType _type;
	material2d::ColorRole _name;
};

class MaterialColorPickerTest : public MaterialTest {
public:
	virtual ~MaterialColorPickerTest() { }

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	virtual void handleContentSizeDirty() override;

protected:
	using MaterialTest::init;

	void updateColor(ColorHCT &&);

	material2d::StyleContainer *_style = nullptr;
	material2d::Surface *_background = nullptr;
	CheckboxWithLabel *_lightCheckbox = nullptr;
	CheckboxWithLabel *_contentCheckbox = nullptr;

	MaterialColorPicker *_huePicker = nullptr;
	MaterialColorPicker *_chromaPicker = nullptr;
	MaterialColorPicker *_tonePicker = nullptr;

	LayerRounded *_spriteLayer = nullptr;
	ColorHCT _colorHct;
	material2d::ColorScheme _colorScheme;
	material2d::ThemeType _themeType = material2d::ThemeType::LightTheme;
	bool _isContentColor = false;

	std::array<MaterialColorSchemeNode *, toInt(material2d::ColorRole::Max)> _nodes;
};

}

#endif /* TEST_SRC_TESTS_MATERIAL_APPMATERIALCOLORPICKERTEST_H_ */
