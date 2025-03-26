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

#ifndef XENOLITH_FONT_XLFONTLOCALE_H_
#define XENOLITH_FONT_XLFONTLOCALE_H_

#include "XLEventHeader.h"
#include "XLFontConfig.h"
#include "SPMetastring.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-string-literal-operator-template"
#endif

// full localized string
template <typename CharType, CharType ... Chars> auto operator ""_locale() {
	return metastring::merge("@Locale:"_meta, metastring::metastring<Chars ...>());
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

// localized token
inline String operator""_token ( const char* str, std::size_t len) {
	String ret;
	ret.reserve(len + 2);
	ret.append("%");
	ret.append(str, len);
	ret.append("%");
	return ret;
}

inline String localeIndex(size_t idx) {
	String ret; ret.reserve(20);
	ret.append("%=");
	ret.append(toString(idx));
	ret.push_back('%');
	return ret;
}

template <size_t Index>
inline constexpr auto localeIndex() {
	return metastring::merge("%="_meta, metastring::numeric<Index>(), "%"_meta);
}

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::locale {

using LocaleInitList = std::initializer_list<Pair<StringView, StringView>>;
using LocaleIndexList = std::initializer_list<Pair<uint32_t, StringView>>;
using NumRule = Function<uint8_t(uint32_t)>;

enum class TimeTokens {
	Today = 0,
	Yesterday,
	Jan,
	Feb,
	Mar,
	Apr,
	Nay,
	Jun,
	jul,
	Aug,
	Sep,
	Oct,
	Nov,
	Dec,
	Max
};

extern EventHeader onLocale;

SP_PUBLIC void define(const StringView &locale, LocaleInitList &&);
SP_PUBLIC void define(const StringView &locale, LocaleIndexList &&);
SP_PUBLIC void define(const StringView &locale, const std::array<StringView, toInt(TimeTokens::Max)> &);

SP_PUBLIC void setDefault(StringView);
SP_PUBLIC StringView getDefault();

SP_PUBLIC void setLocale(StringView);
SP_PUBLIC StringView getLocale();

SP_PUBLIC void setNumRule(const String &, NumRule &&);

SP_PUBLIC WideStringView string(const WideStringView &);
SP_PUBLIC WideStringView string(size_t);

template <char ... Chars>
SP_PUBLIC WideStringView string(const metastring::metastring<Chars...> &str) {
	return string(WideStringView(str.to_std_ustring()));
}

SP_PUBLIC WideStringView numeric(const WideStringView &, uint32_t);

template <char ... Chars>
SP_PUBLIC WideStringView numeric(const metastring::metastring<Chars...> &str, uint32_t n) {
	return numeric(WideStringView(str.to_std_ustring()), n);
}

SP_PUBLIC bool hasLocaleTagsFast(const WideStringView &);
SP_PUBLIC bool hasLocaleTags(const WideStringView &);
SP_PUBLIC WideString resolveLocaleTags(const WideStringView &);

SP_PUBLIC String language(const StringView &locale);

// convert locale name to common form ('en-us', 'ru-ru', 'fr-fr')
SP_PUBLIC String common(const StringView &locale);

SP_PUBLIC StringView timeToken(TimeTokens);

SP_PUBLIC const std::array<memory::string, toInt(TimeTokens::Max)> &timeTokenTable();

SP_PUBLIC String localDate(Time);
SP_PUBLIC String localDate(const std::array<StringView, toInt(TimeTokens::Max)> &, Time);

}

#endif /* XENOLITH_FONT_XLFONTLOCALE_H_ */
