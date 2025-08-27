/**
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

#include "XLWindowInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

Value WindowInfo::encode() const {
	Value ret;
	ret.setString(title, "title");
	ret.setValue(
			Value{
				Value(rect.x),
				Value(rect.y),
				Value(rect.width),
				Value(rect.height),
			},
			"rect");

	ret.setValue(
			Value{
				Value(decorationInsets.top),
				Value(decorationInsets.left),
				Value(decorationInsets.bottom),
				Value(decorationInsets.right),
			},
			"decoration");

	if (density) {
		ret.setDouble(density, "density");
	}

	ret.setString(core::getImageFormatName(imageFormat), "imageFormat");
	ret.setString(core::getColorSpaceName(colorSpace), "colorSpace");
	ret.setString(core::getPresentModeName(preferredPresentMode), "preferredPresentMode");

	Value f;
	if (hasFlag(flags, WindowCreationFlags::DirectOutput)) {
		f.addString("DirectOutput");
	}

	if (hasFlag(flags, WindowCreationFlags::PreferNativeDecoration)) {
		f.addString("PreferNativeDecoration");
	}

	if (hasFlag(flags, WindowCreationFlags::PreferServerSideDecoration)) {
		f.addString("PreferServerSideDecoration");
	}

	if (hasFlag(flags, WindowCreationFlags::PreferServerSideCursors)) {
		f.addString("PreferServerSideCursors");
	}

	if (!f.empty()) {
		ret.setValue(move(f), "flags");
	}
	return ret;
}

} // namespace stappler::xenolith
