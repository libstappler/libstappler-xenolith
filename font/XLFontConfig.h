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

#ifndef XENOLITH_FONT_XLFONTCONFIG_H_
#define XENOLITH_FONT_XLFONTCONFIG_H_

#include "XLCommon.h"
#include "SPFontStyle.h"

namespace stappler::xenolith::config {

// preload char layouts for whole group when char from this group is met
static constexpr bool FontPreloadGroups = false;

// max chars count, used by locale::hasLocaleTagsFast
static constexpr size_t MaxFastLocaleChars = size_t(127);

}

namespace stappler::xenolith::font {

using FontLayoutParameters = geom::FontLayoutParameters;
using FontVariations = geom::FontVariations;
using FontSpecializationVector = geom::FontSpecializationVector;
using FontParameters = geom::FontParameters;
using FontVariableAxis = geom::FontVariableAxis;
using FontWeight = geom::FontWeight;
using FontStretch = geom::FontStretch;
using FontGrade = geom::FontGrade;
using FontStyle = geom::FontStyle;
using FontVariant = geom::FontVariant;
using FontSize = geom::FontSize;
using CharTexture = geom::CharTexture;
using CharLayout = geom::CharLayout;
using CharSpec = geom::CharSpec;
using Metrics = geom::FontMetrics;
using TextDecoration = geom::TextDecoration;
using TextAlign = geom::TextAlign;
using TextParameters = geom::TextParameters;
using TextTransform = geom::TextTransform;
using VerticalAlign = geom::VerticalAlign;
using WhiteSpace = geom::WhiteSpace;
using Hyphens = geom::Hyphens;

}
#endif /* XENOLITH_FONT_XLFONTCONFIG_H_ */
