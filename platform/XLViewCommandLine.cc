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

#include "XLViewCommandLine.h"

namespace stappler::xenolith {

Value ViewCommandLineData::encode() const {
	Value ret;
	ret.setString(bundleName, "bundleName");
	ret.setString(applicationName, "applicationName");
	ret.setString(applicationVersion, "applicationVersion");
	ret.setString(userLanguage, "userLanguage");
	if (!launchUrl.empty()) {
		ret.setString(userLanguage, "userLanguage");
	}
	ret.setValue(Value({Value(screenSize.width), Value(screenSize.height)}), "screenSize");
	ret.setValue(Value({Value(viewDecoration.top), Value(viewDecoration.right),
		Value(viewDecoration.bottom), Value(viewDecoration.left)}), "viewDecoration");
	ret.setDouble(density, "density");
	if (isPhone) {
		ret.setBool(isPhone, "isPhone");
	}
	if (isFixed) {
		ret.setBool(isFixed, "isFixed");
	}
	if (renderdoc) {
		ret.setBool(renderdoc, "renderdoc");
	}
	if (!validation) {
		ret.setBool(validation, "validation");
	}
	if (verbose) {
		ret.setBool(verbose, "verbose");
	}
	if (help) {
		ret.setBool(help, "help");
	}
	return ret;
}

int parseViewCommandLineSwitch(ViewCommandLineData &ret, char c, const char *str) {
	if (c == 'h') {
		ret.help = true;
	} else if (c == 'v') {
		ret.verbose = true;
	}
	return 1;
}

int parseViewCommandLineString(ViewCommandLineData &ret, const StringView &str, int argc, const char * argv[]) {
	if (str == "help") {
		ret.help = true;
	} else if (str == "verbose") {
		ret.verbose = true;
	} else if (str.starts_with("w=")) {
		auto s = str.sub(2).readInteger().get(0);
		if (s > 0) {
			ret.screenSize.width = s;
		}
	} else if (str.starts_with("h=")) {
		auto s = str.sub(2).readInteger().get(0);
		if (s > 0) {
			ret.screenSize.height = s;
		}
	} else if (str.starts_with("d=")) {
		auto s = str.sub(2).readDouble().get(0.0);
		if (s > 0) {
			ret.density = s;
		}
	} else if (str.starts_with("l=")) {
		ret.userLanguage = str.sub(2).str<Interface>();
	} else if (str.starts_with("locale=")) {
		ret.userLanguage = str.sub("locale="_len).str<Interface>();
	} else if (str == "phone") {
		ret.isPhone = true;
	} else if (str.starts_with("bundle=")) {
		ret.bundleName = str.sub("bundle="_len).str<Interface>();
	} else if (str == "fixed") {
		ret.isFixed = true;
	} else if (str == "renderdoc") {
		ret.renderdoc = true;
	} else if (str == "novalidation") {
		ret.validation = false;
	} else if (str.starts_with("decor=")) {
		auto values = str.sub(6);
		float f[4] = { nan(), nan(), nan(), nan() };
		uint32_t i = 0;
		values.split<StringView::Chars<','>>([&] (StringView val) {
			if (i < 4) {
				f[i] = val.readFloat().get(nan());
			}
			++ i;
		});
		if (!isnan(f[0])) {
			if (isnan(f[1])) {
				f[1] = f[0];
			}
			if (isnan(f[2])) {
				f[2] = f[0];
			}
			if (isnan(f[3])) {
				f[3] = f[1];
			}
			ret.viewDecoration = Padding(f[0], f[1], f[2], f[3]);
		}
	}
	return 1;
}

}
