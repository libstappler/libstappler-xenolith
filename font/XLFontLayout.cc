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

#include "XLFontLayout.h"
#include "XLFontLibrary.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

String FontLayout::constructName(StringView family, const FontSpecializationVector &vec) {
	return FontParameters::getFontConfigName<Interface>(family, vec.fontSize, vec.fontStyle, vec.fontWeight, vec.fontStretch, vec.fontGrade, FontVariant::Normal, false);
}

bool FontLayout::init(String &&name, StringView family, FontSpecializationVector &&spec, Rc<FontFaceData> &&data, FontLibrary *c) {
	_name = move(name);
	_family = family.str<Interface>();
	_spec = move(spec);
	_sources.emplace_back(move(data));
	_faces.resize(_sources.size(), nullptr);
	_library = c;
	if (auto face = _library->openFontFace(_sources.front(), _spec)) {
		_faces[0] = face;
		_metrics = _faces.front()->getMetrics();
	}
	return true;
}

bool FontLayout::init(String &&name, StringView family, FontSpecializationVector &&spec, Vector<Rc<FontFaceData>> &&data, FontLibrary *c) {
	_name = move(name);
	_family = family.str<Interface>();
	_spec = move(spec);
	_sources = move(data);
	_faces.resize(_sources.size(), nullptr);
	_library = c;
	if (auto face = _library->openFontFace(_sources.front(), _spec)) {
		_faces[0] = face;
		_metrics = _faces.front()->getMetrics();
	}
	return true;
}

void FontLayout::touch(uint64_t clock, bool persistent) {
	_accessTime = clock;
	_persistent = persistent;
}

bool FontLayout::addString(const FontCharString &str, Vector<char16_t> &failed) {
	std::shared_lock sharedLock(_mutex);

	bool shouldOpenFonts = false;
	bool updated = false;
	size_t i = 0;

	for (auto &it : _faces) {
		if (i == 0) {
			if (it->addChars(str.chars, i == 0, &failed)) {
				updated = true;
			}
		} else {
			if (it == nullptr) {
				shouldOpenFonts = true;
				break;
			}

			auto tmp = move(failed);
			failed.clear();

			if (it->addChars(tmp, i == 0, &failed)) {
				updated = true;
			}
		}

		if (failed.empty()) {
			break;
		}

		++ i;
	}

	if (shouldOpenFonts) {
		sharedLock.unlock();
		std::unique_lock lock(_mutex);

		for (; i < _faces.size(); ++ i) {
			if (_faces[i] == nullptr) {
				_faces[i] = _library->openFontFace(_sources[i], _spec);
			}

			auto tmp = move(failed);
			failed.clear();

			if (_faces[i]->addChars(tmp, i == 0, &failed)) {
				updated = true;
			}

			if (failed.empty()) {
				break;
			}
		}
	}

	return updated;
}

uint16_t FontLayout::getFontHeight() const {
	return _metrics.height;
}

int16_t FontLayout::getKerningAmount(char16_t first, char16_t second, uint16_t face) const {
	std::shared_lock lock(_mutex);
	for (auto &it : _faces) {
		if (it) {
			if (it->getId() == face) {
				return it->getKerningAmount(first, second);
			}
		} else {
			return 0;
		}
	}
	return 0;
}

Metrics FontLayout::getMetrics() const {
	return _metrics;
}

CharLayout FontLayout::getChar(char16_t ch, uint16_t &face) const {
	std::shared_lock lock(_mutex);
	for (auto &it : _faces) {
		auto l = it->getChar(ch);
		if (l.charID != 0) {
			face = it->getId();
			return l;
		}
	}
	return CharLayout();
}

bool FontLayout::addTextureChars(SpanView<CharSpec> chars) const {
	std::shared_lock lock(_mutex);

	bool ret = false;
	for (auto &it : chars) {
		if (chars::isspace(it.charID) || it.charID == char16_t(0x0A) || it.charID == char16_t(0x00AD)) {
			continue;
		}

		for (auto &f : _faces) {
			if (f && f->getId() == it.face) {
				if (f->addRequiredChar(it.charID)) {
					ret = true;
					break;
				}
			}
		}
	}
	return ret;
}

const Vector<Rc<FontFaceObject>> &FontLayout::getFaces() const {
	return _faces;
}

size_t FontLayout::getFaceCount() const {
	return _sources.size();
}

Rc<FontFaceData> FontLayout::getSource(size_t idx) const {
	if (idx < _sources.size()) {
		return _sources[idx];
	}
	return nullptr;
}

}
