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

#include "MaterialLabel.h"
#include "MaterialSurface.h"
#include "XLFrameInfo.h"

namespace stappler::xenolith::material2d {

bool TypescaleLabel::init(TypescaleRole role) {
	if (!Label::init()) {
		return false;
	}

	setFontFamily("sans");
	setRole(role);
	return true;
}

bool TypescaleLabel::init(TypescaleRole role, StringView str) {
	if (!Label::init(str)) {
		return false;
	}

	setFontFamily("sans");
	setRole(role);
	return true;
}

bool TypescaleLabel::init(TypescaleRole role, StringView str, float w, TextAlign a) {
	if (!Label::init(str, w, a)) {
		return false;
	}

	setFontFamily("sans");
	setRole(role);
	return true;
}

void TypescaleLabel::setRole(TypescaleRole role) {
	_role = role;
	switch (_role) {
	case TypescaleRole::DisplayLarge: setFontSize(57); setFontWeight(font::FontWeight(400)); break; // 57 400
	case TypescaleRole::DisplayMedium: setFontSize(45); setFontWeight(font::FontWeight(400)); break; // 45 400
	case TypescaleRole::DisplaySmall: setFontSize(36); setFontWeight(font::FontWeight(400)); break; // 36 400
	case TypescaleRole::HeadlineLarge: setFontSize(32); setFontWeight(font::FontWeight(400)); break; // 32 400
	case TypescaleRole::HeadlineMedium: setFontSize(28); setFontWeight(font::FontWeight(400)); break; // 28 400
	case TypescaleRole::HeadlineSmall: setFontSize(24); setFontWeight(font::FontWeight(400)); break; // 24 400
	case TypescaleRole::TitleLarge: setFontSize(22); setFontWeight(font::FontWeight(400)); break; // 22 400
	case TypescaleRole::TitleMedium: setFontSize(16); setFontWeight(font::FontWeight(500)); break; // 16 500
	case TypescaleRole::TitleSmall: setFontSize(14); setFontWeight(font::FontWeight(500)); break; // 14 500
	case TypescaleRole::LabelLarge: setFontSize(14); setFontWeight(font::FontWeight(500)); break; // 14 500
	case TypescaleRole::LabelMedium: setFontSize(12); setFontWeight(font::FontWeight(500)); break; // 12 500
	case TypescaleRole::LabelSmall: setFontSize(11); setFontWeight(font::FontWeight(500)); break; // 11 500
	case TypescaleRole::BodyLarge: setFontSize(16); setFontWeight(font::FontWeight(400)); break; // 16 400 0.5
	case TypescaleRole::BodyMedium: setFontSize(14); setFontWeight(font::FontWeight(400)); break; // 14 400 0.25
	case TypescaleRole::BodySmall: setFontSize(12); setFontWeight(font::FontWeight(400)); break; // 12 400 0.4
	case TypescaleRole::Unknown: break;
	}
}

void TypescaleLabel::setBlendColor(ColorRole rule, float value) {
	if (_blendColorRule != rule || _blendValue != value) {
		_blendColorRule = rule;
		_blendValue = value;
	}
}

void TypescaleLabel::setPreserveOpacity(bool value) {
	_preserveOpacity = value;
}

bool TypescaleLabel::isPreserveOpacity() const {
	return _preserveOpacity;
}

bool TypescaleLabel::visitDraw(FrameInfo &frame, NodeFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	auto style = frame.getComponent<SurfaceInterior>(SurfaceInterior::ComponentFrameTag);
	if (style) {
		auto &s = style->getStyle();

		if (_blendValue > 0.0f) {
			auto styleContainer = frame.getComponent<StyleContainer>(StyleContainer::ComponentFrameTag);
			if (styleContainer) {
				if (auto scheme = styleContainer->getScheme(s.schemeTag)) {
					auto c = scheme->get(_blendColorRule);
					if (c != _blendColor) {
						_blendColor = c;
					}
				}
			}
		}

		auto color = s.colorOn.asColor4F();
		if (_blendValue > 0.0f) {
			color = color * (1.0f - _blendValue) + _blendColor * _blendValue;
		}

		if (color != getColor()) {
			setColor(color, !_preserveOpacity);
		}

		if (getRenderingLevel() != RenderingLevel::Default) {
			if (s.colorElevation.a < 1.0f && style->getStyle().colorElevation.a > 0.0f) {
				setRenderingLevel(RenderingLevel::Transparent);
			} else {
				setRenderingLevel(RenderingLevel::Surface);
			}
		}

		if (_themeType != style->getStyle().themeType) {
			_themeType = style->getStyle().themeType;
			_labelDirty = true;
		}
	}

	return Label::visitDraw(frame, parentFlags);
}

void TypescaleLabel::specializeStyle(DescriptionStyle &style, float density) const {
	struct PersistentStyle {
		font::FontSize size;
		font::FontWeight weight;

		constexpr PersistentStyle(TypescaleRole role) {
			switch (role) {
			case TypescaleRole::DisplayLarge: size = font::FontSize(57); weight = font::FontWeight(400); break; // 57 400
			case TypescaleRole::DisplayMedium: size = font::FontSize(45); weight = font::FontWeight(400); break; // 45 400
			case TypescaleRole::DisplaySmall: size = font::FontSize(36); weight = font::FontWeight(400); break; // 36 400
			case TypescaleRole::HeadlineLarge: size = font::FontSize(32); weight = font::FontWeight(400); break; // 32 400
			case TypescaleRole::HeadlineMedium: size = font::FontSize(28); weight = font::FontWeight(400); break; // 28 400
			case TypescaleRole::HeadlineSmall: size = font::FontSize(24); weight = font::FontWeight(400); break; // 24 400
			case TypescaleRole::TitleLarge: size = font::FontSize(22); weight = font::FontWeight(400); break; // 22 400
			case TypescaleRole::TitleMedium: size = font::FontSize(16); weight = font::FontWeight(500); break; // 16 500
			case TypescaleRole::TitleSmall: size = font::FontSize(14); weight = font::FontWeight(500); break; // 14 500
			case TypescaleRole::LabelLarge: size = font::FontSize(14); weight = font::FontWeight(500); break; // 14 500
			case TypescaleRole::LabelMedium: size = font::FontSize(12); weight = font::FontWeight(500); break; // 12 500
			case TypescaleRole::LabelSmall: size = font::FontSize(11); weight = font::FontWeight(500); break; // 11 500
			case TypescaleRole::BodyLarge: size = font::FontSize(16); weight = font::FontWeight(400); break; // 16 400 0.5
			case TypescaleRole::BodyMedium: size = font::FontSize(14); weight = font::FontWeight(400); break; // 14 400 0.25
			case TypescaleRole::BodySmall: size = font::FontSize(12); weight = font::FontWeight(400); break; // 12 400 0.4
			case TypescaleRole::Unknown: break;
			}
		}

		bool operator==(const font::FontParameters &f) const { return f.fontSize == size && f.fontWeight == weight; }
	};

	if (_themeType == ThemeType::DarkTheme) {
		style.font.fontGrade = font::FontGrade(style.font.fontGrade.get() - 50);
	}

	Label::specializeStyle(style, density);
	if (!_persistentLayout) {
		if ((style.font.fontGrade == font::FontGrade::Normal || style.font.fontGrade == font::FontGrade::Normal)
				&& style.font.fontStretch.get() % 100 == 0
				&& (style.font.fontStyle == font::FontStyle::Italic || style.font.fontStyle == font::FontStyle::Oblique)) {

			static constexpr PersistentStyle variants[] = {
				PersistentStyle(TypescaleRole::DisplayLarge), // 57 400
				PersistentStyle(TypescaleRole::DisplayMedium), // 45 400
				PersistentStyle(TypescaleRole::DisplaySmall), // 36 400
				PersistentStyle(TypescaleRole::HeadlineLarge), // 32 400
				PersistentStyle(TypescaleRole::HeadlineMedium), // 28 400
				PersistentStyle(TypescaleRole::HeadlineSmall), // 24 400
				PersistentStyle(TypescaleRole::TitleLarge), // 22 400
				PersistentStyle(TypescaleRole::TitleMedium), // 16 500
				PersistentStyle(TypescaleRole::TitleSmall), // 14 500
				PersistentStyle(TypescaleRole::LabelLarge), // 14 500
				PersistentStyle(TypescaleRole::LabelMedium), // 12 500
				PersistentStyle(TypescaleRole::LabelSmall), // 11 500
				PersistentStyle(TypescaleRole::BodyLarge), // 16 400 0.5
				PersistentStyle(TypescaleRole::BodyMedium), // 14 400 0.25
				PersistentStyle(TypescaleRole::BodySmall), // 12 400 0.4
			};

			for (auto &it : variants) {
				if (it == style.font) {
					style.font.persistent = true;
				}
			}
		}
	}
}

}