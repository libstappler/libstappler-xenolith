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

#include "AppMaterialBackground.h"
#include "MaterialStyleContainer.h"
#include "AppMaterialColorPicker.h"
#include "XL2dSceneLight.h"
#include "XL2dSceneContent.h"

namespace stappler::xenolith::app {

bool MaterialBackground::init() {
	if (!BackgroundSurface::init()) {
		return false;
	}

	_huePicker = addChild(Rc<MaterialColorPicker>::create(MaterialColorPicker::Hue, ColorHCT(), [this] (float val) {
		auto color = ColorHCT(val, 100.0f, 50.0f, 1.0f);
		_styleContainer->setPrimaryScheme(_lightCheckbox->getValue() ? material2d::ThemeType::DarkTheme : material2d::ThemeType::LightTheme, color, false);

		if (_sceneStyleContainer) {
			_sceneStyleContainer->setPrimaryScheme(_lightCheckbox->getValue() ? material2d::ThemeType::DarkTheme : material2d::ThemeType::LightTheme, color, false);
		}
		_huePicker->setTargetColor(color);
	}));
	_huePicker->setAnchorPoint(Anchor::TopLeft);
	_huePicker->setContentSize(Size2(240.0f, 24.0f));

	_lightCheckbox = addChild(Rc<CheckboxWithLabel>::create("Dark theme", false, [this] (bool value) {
		if (value) {
			_styleContainer->setPrimaryScheme(material2d::ThemeType::DarkTheme, _huePicker->getTargetColor(), false);
		} else {
			_styleContainer->setPrimaryScheme(material2d::ThemeType::LightTheme, _huePicker->getTargetColor(), false);
		}
		if (_sceneStyleContainer) {
			if (value) {
				_sceneStyleContainer->setPrimaryScheme(material2d::ThemeType::DarkTheme, _huePicker->getTargetColor(), false);
			} else {
				_sceneStyleContainer->setPrimaryScheme(material2d::ThemeType::LightTheme, _huePicker->getTargetColor(), false);
			}
		}
	}));
	_lightCheckbox->setAnchorPoint(Anchor::TopLeft);
	_lightCheckbox->setContentSize(Size2(24.0f, 24.0f));

	return true;
}

void MaterialBackground::onContentSizeDirty() {
	BackgroundSurface::onContentSizeDirty();

	_huePicker->setPosition(Vec2(16.0f, _contentSize.height - 16.0f));
	_huePicker->setContentSize(Size2(std::min(std::max(160.0f, _contentSize.width - 200.0f - 98.0f - 48.0f), 360.0f), 24.0f));
	_lightCheckbox->setPosition(Vec2(16.0f, _contentSize.height - 48.0f));
}

void MaterialBackground::onEnter(xenolith::Scene *scene) {
	BackgroundSurface::onEnter(scene);

	if (auto sceneStyle = scene->getComponentByType<material2d::StyleContainer>()) {
		auto color = sceneStyle->getPrimaryScheme().hct(material2d::ColorRole::Primary);
		auto themeType = sceneStyle->getPrimaryScheme().type;

		_styleContainer->setPrimaryScheme(themeType, color, false);
		_sceneStyleContainer = sceneStyle;
		_huePicker->setTargetColor(color);
		_huePicker->setValue(color.data.hue / 360.0f);
	}

	auto content = dynamic_cast<SceneContent2d *>(_scene->getContent());

	auto color = Color4F::WHITE;
	color.a = 0.5f;

	content->removeAllLights();

	auto light = Rc<SceneLight>::create(SceneLightType::Ambient, Vec2(0.0f, 0.3f), 1.5f, color);
	auto ambient = Rc<SceneLight>::create(SceneLightType::Ambient, Vec2(0.0f, 0.0f), 1.5f, color);

	content->setGlobalLight(Color4F::WHITE);
	content->removeAllLights();
	content->addLight(move(light));
	content->addLight(move(ambient));
}

}
