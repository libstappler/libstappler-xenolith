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

#ifndef XENOLITH_PLATFORM_XLVIEWCOMMANDLINE_H_
#define XENOLITH_PLATFORM_XLVIEWCOMMANDLINE_H_

#include "XLCommon.h"

namespace stappler::xenolith {

struct ViewCommandLineData {
	String bundleName = "org.stappler.xenolith.test";
	String applicationName = "Xenolith";
	String applicationVersion = "0.0.1";
	String userLanguage = "ru-ru";

	String launchUrl;

	Extent2 screenSize = Extent2(1024, 768);
	Padding viewDecoration;
	float density = 1.0f;
	bool isPhone = false;
	bool isFixed = false;
	bool renderdoc = false;
	bool validation = true;

	bool verbose = false;
	bool help = false;

	Value encode() const;
};

int parseViewCommandLineSwitch(ViewCommandLineData &ret, char c, const char *str);
int parseViewCommandLineString(ViewCommandLineData &ret, const StringView &str, int argc, const char * argv[]);

}

#endif /* XENOLITH_PLATFORM_XLVIEWCOMMANDLINE_H_ */
