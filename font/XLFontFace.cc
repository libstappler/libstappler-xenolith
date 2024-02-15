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

#include "XLFontFace.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_MULTIPLE_MASTERS_H
#include FT_SFNT_NAMES_H
#include FT_ADVANCES_H

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

static constexpr uint32_t getAxisTag(char c1, char c2, char c3, char c4) {
	return uint32_t(c1 & 0xFF) << 24 | uint32_t(c2 & 0xFF) << 16 | uint32_t(c3 & 0xFF) << 8 | uint32_t(c4 & 0xFF);
}

static constexpr uint32_t getAxisTag(const char c[4]) {
	return getAxisTag(c[0], c[1], c[2], c[3]);
}

static CharGroupId getCharGroupForChar(char16_t c) {
	using namespace chars;
	if (CharGroup<char16_t, CharGroupId::Numbers>::match(c)) {
		return CharGroupId::Numbers;
	} else if (CharGroup<char16_t, CharGroupId::Latin>::match(c)) {
		return CharGroupId::Latin;
	} else if (CharGroup<char16_t, CharGroupId::Cyrillic>::match(c)) {
		return CharGroupId::Cyrillic;
	} else if (CharGroup<char16_t, CharGroupId::Currency>::match(c)) {
		return CharGroupId::Currency;
	} else if (CharGroup<char16_t, CharGroupId::GreekBasic>::match(c)) {
		return CharGroupId::GreekBasic;
	} else if (CharGroup<char16_t, CharGroupId::Math>::match(c)) {
		return CharGroupId::Math;
	} else if (CharGroup<char16_t, CharGroupId::TextPunctuation>::match(c)) {
		return CharGroupId::TextPunctuation;
	}
	return CharGroupId::None;
}

void FontCharString::addChar(char16_t c) {
	auto it = std::lower_bound(chars.begin(), chars.end(), c);
	if (it == chars.end() || *it != c) {
		chars.insert(it, c);
	}
}

void FontCharString::addString(const String &str) {
	addString(string::toUtf16<Interface>(str));
}

void FontCharString::addString(const WideString &str) {
	addString(str.data(), str.size());
}

void FontCharString::addString(const char16_t *str, size_t len) {
	for (size_t i = 0; i < len; ++ i) {
		const char16_t &c = str[i];
		auto it = std::lower_bound(chars.begin(), chars.end(), c);
		if (it == chars.end() || *it != c) {
			chars.insert(it, c);
		}
	}
}

void FontCharString::addString(const FontCharString &str) {
	addString(str.chars.data(), str.chars.size());
}

bool FontFaceData::init(StringView name, BytesView data, bool persistent) {
	if (persistent) {
		_view = data;
		_persistent = true;
		_name = name.str<Interface>();
		return true;
	} else {
		return init(name, data.bytes<Interface>());
	}
}

bool FontFaceData::init(StringView name, Bytes &&data) {
	_persistent = false;
	_data = move(data);
	_view = _data;
	_name = name.str<Interface>();
	return true;
}

bool FontFaceData::init(StringView name, Function<Bytes()> &&cb) {
	_persistent = true;
	_data = cb();
	_view = _data;
	_name = name.str<Interface>();
	return true;
}

void FontFaceData::inspectVariableFont(FontLayoutParameters params, FT_Face face) {
	FT_MM_Var *masters = nullptr;
	FT_Get_MM_Var(face, &masters);

	_variations.weight = params.fontWeight;
	_variations.stretch = params.fontStretch;
	_variations.opticalSize = uint32_t(0);
	_variations.italic = uint32_t(params.fontStyle == FontStyle::Italic ? 1 : 0);
	_variations.slant = params.fontStyle;
	_variations.grade = params.fontGrade;

	if (masters) {
		for (FT_UInt i = 0; i < masters->num_axis; ++ i) {
			auto tag = masters->axis[i].tag;
			if (tag == getAxisTag("wght")) {
				_variations.axisMask |= FontVariableAxis::Weight;
				_variations.weight.min = FontWeight(masters->axis[i].minimum >> 16);
				_variations.weight.max = FontWeight(masters->axis[i].maximum >> 16);
			} else if (tag == getAxisTag("wdth")) {
				_variations.axisMask |= FontVariableAxis::Width;
				_variations.stretch.min = FontStretch(masters->axis[i].minimum >> 15);
				_variations.stretch.max = FontStretch(masters->axis[i].maximum >> 15);
			} else if (tag == getAxisTag("ital")) {
				_variations.axisMask |= FontVariableAxis::Italic;
				_variations.italic.min = masters->axis[i].minimum;
				_variations.italic.max = masters->axis[i].maximum;
			} else if (tag == getAxisTag("slnt")) {
				_variations.axisMask |= FontVariableAxis::Slant;
				_variations.slant.min = FontStyle(masters->axis[i].minimum >> 10);
				_variations.slant.max = FontStyle(masters->axis[i].maximum >> 10);
			} else if (tag == getAxisTag("opsz")) {
				_variations.axisMask |= FontVariableAxis::OpticalSize;
				_variations.opticalSize.min = masters->axis[i].minimum;
				_variations.opticalSize.max = masters->axis[i].maximum;
			} else if (tag == getAxisTag("GRAD")) {
				_variations.axisMask |= FontVariableAxis::Grade;
				_variations.grade.min = FontGrade(masters->axis[i].minimum >> 16);
				_variations.grade.max = FontGrade(masters->axis[i].maximum >> 16);
			}
			/* std::cout << "Variable axis: [" << masters->axis[i].tag << "] "
					<< (masters->axis[i].minimum >> 16) << " - " << (masters->axis[i].maximum >> 16)
					<< " def: "<< (masters->axis[i].def >> 16) << "\n"; */
		}
	}

	_params = params;
}

BytesView FontFaceData::getView() const {
	return _view;
}

FontSpecializationVector FontFaceData::getSpecialization(const FontSpecializationVector &vec) const {
	return _variations.getSpecialization(vec);
}

FontFaceObject::~FontFaceObject() { }

bool FontFaceObject::init(StringView name, const Rc<FontFaceData> &data, FT_Face face, const FontSpecializationVector &spec, uint16_t id) {
	auto err = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
	if (err != FT_Err_Ok) {
		return false;
	}

	auto &var = data->getVariations();
	if (var.axisMask != FontVariableAxis::None) {
		Vector<FT_Fixed> vector;

		FT_MM_Var *masters;
		FT_Get_MM_Var(face, &masters);

		if (masters) {
			for (FT_UInt i = 0; i < masters->num_axis; ++ i) {
				auto tag = masters->axis[i].tag;
				if (tag == getAxisTag("wght")) {
					vector.emplace_back(var.weight.clamp(spec.fontWeight).get() << 16);
				} else if (tag == getAxisTag("wdth")) {
					vector.emplace_back(var.stretch.clamp(spec.fontStretch).get() << 15);
				} else if (tag == getAxisTag("ital")) {
					if (spec.fontStyle.get() == FontStyle::Normal.get()) {
						vector.emplace_back(var.italic.min);
					} else if (spec.fontStyle.get() == FontStyle::Italic.get()) {
						vector.emplace_back(var.italic.max);
					} else {
						if ((var.axisMask & FontVariableAxis::Slant) != FontVariableAxis::None) {
							vector.emplace_back(var.italic.min); // has true oblique
						} else {
							vector.emplace_back(var.italic.max);
						}
					}
				} else if (tag == getAxisTag("slnt")) {
					if (spec.fontStyle.get() == FontStyle::Normal.get()) {
						vector.emplace_back(0);
					} else if (spec.fontStyle.get() == FontStyle::Italic.get()) {
						if ((var.axisMask & FontVariableAxis::Italic) != FontVariableAxis::None) {
							vector.emplace_back(masters->axis[i].def);
						} else {
							vector.emplace_back(var.slant.clamp(FontStyle::Oblique).get() << 10);
						}
					} else {
						vector.emplace_back(var.slant.clamp(spec.fontStyle).get() << 10);
					}
				} else if (tag == getAxisTag("opsz")) {
					auto opticalSize = uint32_t(floorf(spec.fontSize.get() / spec.density)) << 16;
					vector.emplace_back(var.opticalSize.clamp(opticalSize));
				} else if (tag == getAxisTag("GRAD")) {
					vector.emplace_back(var.grade.clamp(spec.fontGrade).get() << 16);
				} else {
					vector.emplace_back(masters->axis[i].def);
				}
			}

			FT_Set_Var_Design_Coordinates(face, vector.size(), vector.data());
		}
	}

	// set the requested font size
	err = FT_Set_Pixel_Sizes(face, spec.fontSize.get(), spec.fontSize.get());
	if (err != FT_Err_Ok) {
		return false;
	}

	_spec = spec;
	_metrics.size = spec.fontSize.get();
	_metrics.height = face->size->metrics.height >> 6;
	_metrics.ascender = face->size->metrics.ascender >> 6;
	_metrics.descender = face->size->metrics.descender >> 6;
	_metrics.underlinePosition = face->underline_position >> 6;
	_metrics.underlineThickness = face->underline_thickness >> 6;

	_name = name.str<Interface>();
	_id = id;
	_data = data;
	_face = face;

	return true;
}

bool FontFaceObject::acquireTexture(char16_t theChar, const Callback<void(const CharTexture &)> &cb) {
	std::unique_lock<Mutex> lock(_faceMutex);

	return acquireTextureUnsafe(theChar, cb);
}

bool FontFaceObject::acquireTextureUnsafe(char16_t theChar, const Callback<void(const CharTexture &)> &cb) {
	int glyph_index = FT_Get_Char_Index(_face, theChar);
	if (!glyph_index) {
		return false;
	}

	auto err = FT_Load_Glyph(_face, glyph_index, FT_LOAD_DEFAULT | FT_LOAD_RENDER);
	if (err != FT_Err_Ok) {
		return false;
	}

	//log::format("Texture", "%s: %d '%s'", data.layout->getName().c_str(), theChar.charID, string::toUtf8(theChar.charID).c_str());

	if (_face->glyph->bitmap.buffer != nullptr) {
		if (_face->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
			cb(CharTexture{_id, theChar,
				static_cast<int16_t>(_face->glyph->metrics.horiBearingX >> 6),
				static_cast<int16_t>(- (_face->glyph->metrics.horiBearingY >> 6)),
				static_cast<uint16_t>(_face->glyph->metrics.width >> 6),
				static_cast<uint16_t>(_face->glyph->metrics.height >> 6),
				_face->glyph->bitmap.width,
				_face->glyph->bitmap.rows,
				_face->glyph->bitmap.pitch ? _face->glyph->bitmap.pitch : int(_face->glyph->bitmap.width),
				_face->glyph->bitmap.buffer
			});
			return true;
		}
	} else {
		if (!chars::isspace(theChar) && theChar != char16_t(0x0A)) {
			log::format(log::Warn, "Font", "error: no bitmap for (%d) '%s'", theChar, string::toUtf8<Interface>(theChar).data());
		}
	}
	return false;
}

bool FontFaceObject::addChars(const Vector<char16_t> &chars, bool expand, Vector<char16_t> *failed) {
	bool updated = false;
	uint32_t mask = 0;

	if constexpr (!config::FontPreloadGroups) {
		expand = false;
	}

	// for some chars, we add full group, not only requested char
	for (auto &c : chars) {
		if (expand) {
			auto g = getCharGroupForChar(c);
			if (g != CharGroupId::None) {
				if ((mask & toInt(g)) == 0) {
					mask |= toInt(g);
					if (addCharGroup(g, failed)) {
						updated = true;
					}
					continue;
				}
			}
		}

		if (!addChar(c, updated) && failed) {
			mem_std::emplace_ordered(*failed, c);
		}
	}
	return updated;
}

bool FontFaceObject::addCharGroup(CharGroupId g, Vector<char16_t> *failed) {
	bool updated = false;
	using namespace chars;
	auto f = [&, this] (char16_t c) {
		if (!addChar(c, updated) && failed) {
			mem_std::emplace_ordered(*failed, c);
		}
	};

	switch (g) {
	case CharGroupId::Numbers: CharGroup<char16_t, CharGroupId::Numbers>::foreach(f); break;
	case CharGroupId::Latin: CharGroup<char16_t, CharGroupId::Latin>::foreach(f); break;
	case CharGroupId::Cyrillic: CharGroup<char16_t, CharGroupId::Cyrillic>::foreach(f); break;
	case CharGroupId::Currency: CharGroup<char16_t, CharGroupId::Currency>::foreach(f); break;
	case CharGroupId::GreekBasic: CharGroup<char16_t, CharGroupId::GreekBasic>::foreach(f); break;
	case CharGroupId::Math: CharGroup<char16_t, CharGroupId::Math>::foreach(f); break;
	case CharGroupId::TextPunctuation: CharGroup<char16_t, CharGroupId::TextPunctuation>::foreach(f); break;
	default: break;
	}
	return updated;
}

bool FontFaceObject::addRequiredChar(char16_t ch) {
	std::unique_lock<Mutex> lock(_requiredMutex);
	return mem_std::emplace_ordered(_required, ch);
}

Vector<char16_t> FontFaceObject::getRequiredChars() const {
	std::unique_lock<Mutex> lock(_requiredMutex);
	return _required;
}

CharLayout FontFaceObject::getChar(char16_t c) const {
	std::shared_lock lock(_charsMutex);
	auto l = _chars.get(c);
	if (l && l->charID == c) {
		return *l;
	}
	return CharLayout{0};
}

int16_t FontFaceObject::getKerningAmount(char16_t first, char16_t second) const {
	std::shared_lock lock(_charsMutex);
	uint32_t key = (first << 16) | (second & 0xffff);
	auto it = _kerning.find(key);
	if (it != _kerning.end()) {
		return it->second;
	}
	return 0;
}

bool FontFaceObject::addChar(char16_t theChar, bool &updated) {
	do {
		// try to get char with shared lock
		std::shared_lock charsLock(_charsMutex);
		auto value = _chars.get(theChar);
		if (value) {
			if (value->charID == theChar) {
				return true;
			} else if (value->charID == char16_t(0xFFFF)) {
				return false;
			}
		}
	} while (0);

	std::unique_lock charsUniqueLock(_charsMutex);
	auto value = _chars.get(theChar);
	if (value) {
		if (value->charID == theChar) {
			return true;
		} else if (value->charID == char16_t(0xFFFF)) {
			return false;
		}
	}

	std::unique_lock<Mutex> faceLock(_faceMutex);
	FT_UInt cIdx = FT_Get_Char_Index(_face, theChar);
	if (!cIdx) {
		_chars.emplace(theChar, CharLayout{char16_t(0xFFFF)});
		return false;
	}

	FT_Fixed advance;
	auto err = FT_Get_Advance(_face, cIdx, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP, &advance);
	if (err != FT_Err_Ok) {
		_chars.emplace(theChar, CharLayout{char16_t(0xFFFF)});
		return false;
	}

	/*auto err = FT_Load_Glyph(_face, cIdx, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP);
	if (err != FT_Err_Ok) {
		_chars.emplace(theChar, CharLayout{char16_t(0xFFFF)});
		return false;
	}*/

	// store result in the passed rectangle
	_chars.emplace(theChar, CharLayout{theChar,
		//static_cast<int16_t>(_face->glyph->metrics.horiBearingX >> 6),
		//static_cast<int16_t>(- (_face->glyph->metrics.horiBearingY >> 6)),
		static_cast<uint16_t>(advance >> 16),
		//static_cast<uint16_t>(_face->glyph->metrics.width >> 6),
		//static_cast<uint16_t>(_face->glyph->metrics.height >> 6)
		});

	if (!chars::isspace(theChar)) {
		updated = true;
	}

	if (FT_HAS_KERNING(_face)) {
		_chars.foreach([&, this] (const CharLayout & it) {
			if (it.charID == 0 || it.charID == char16_t(0xFFFF)) {
				return;
			}

			if (it.charID != theChar) {
				FT_Vector kerning;
				auto err = FT_Get_Kerning(_face, cIdx, cIdx, FT_KERNING_DEFAULT, &kerning);
				if (err == FT_Err_Ok) {
					auto value = (int16_t)(kerning.x >> 6);
					if (value != 0) {
						_kerning.emplace(theChar << 16 | (it.charID & 0xffff), value);
					}
				}
			} else {
				auto kIdx = FT_Get_Char_Index(_face, it.charID);

				FT_Vector kerning;
				auto err = FT_Get_Kerning(_face, cIdx, kIdx, FT_KERNING_DEFAULT, &kerning);
				if (err == FT_Err_Ok) {
					auto value = (int16_t)(kerning.x >> 6);
					if (value != 0) {
						_kerning.emplace(theChar << 16 | (it.charID & 0xffff), value);
					}
				}

				err = FT_Get_Kerning(_face, kIdx, cIdx, FT_KERNING_DEFAULT, &kerning);
				if (err == FT_Err_Ok) {
					auto value = (int16_t)(kerning.x >> 6);
					if (value != 0) {
						_kerning.emplace(it.charID << 16 | (theChar & 0xffff), value);
					}
				}
			}
		});
	}
	return true;
}

}
