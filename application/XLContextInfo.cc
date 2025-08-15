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

#include "XLContextInfo.h"
#include "platform/XLContextController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// Опции для аргументов командной строки

CommandLineParser<ContextConfig> ContextConfig::getCommandLineParser() {
	return CommandLineParser<ContextConfig>({
		CommandLineOption<ContextConfig>{.patterns = {"-v", "--verbose"},
			.description = "Produce more verbose output",
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.context) {
			target.context = Rc<ContextInfo>::alloc();
		}
		target.flags |= CommonFlags::Verbose;
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"-h", "--help"},
			.description = StringView("Show help message and exit"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.context) {
			target.context = Rc<ContextInfo>::alloc();
		}
		target.flags |= CommonFlags::Help;
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"-q", "--quiet"},
			.description = StringView("Disable verbose output"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.context) {
			target.context = Rc<ContextInfo>::alloc();
		}
		target.flags |= CommonFlags::Quiet;
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"-W<#>", "--width <#>"},
			.description = StringView("Window width"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.window) {
			target.window = Rc<WindowInfo>::alloc();
		}
		target.window->rect.width = uint32_t(StringView(args[0]).readInteger(10).get(0));
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"-H<#>", "--height <#>"},
			.description = StringView("Window height"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.window) {
			target.window = Rc<WindowInfo>::alloc();
		}
		target.window->rect.height = uint32_t(StringView(args[0]).readInteger(10).get(0));
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"-D<#.#>", "--density <#.#>"},
			.description = StringView("Pixel density for a window"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.window) {
			target.window = Rc<WindowInfo>::alloc();
		}
		target.window->density = float(StringView(args[0]).readFloat().get(0));
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"--l <locale>", "--locale <locale>"},
			.description = StringView("User language locale"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.context) {
			target.context = Rc<ContextInfo>::alloc();
		}
		target.context->userLanguage = StringView(args[0]).str<Interface>();
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"--bundle <bundle-name>"},
			.description = StringView("Application bundle name"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.context) {
			target.context = Rc<ContextInfo>::alloc();
		}
		target.context->bundleName = StringView(args[0]).str<Interface>();
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"--fixed"},
			.description = StringView("Use fixed (so, not resizable) window layout"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.window) {
			target.window = Rc<WindowInfo>::alloc();
		}
		target.window->flags |= WindowFlags::FixedBorder;
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"--renderdoc"},
			.description = StringView("Open connection for renderdoc"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.instance) {
			target.instance = Rc<core::InstanceInfo>::alloc();
		}
		target.instance->flags |= core::InstanceFlags::RenderDoc;
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"--novalidation"},
			.description = StringView("Force-disable Vulkan validation layers"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.instance) {
			target.instance = Rc<core::InstanceInfo>::alloc();
		}
		target.instance->flags |= core::InstanceFlags::Validation;
		return true;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"--decor <decoration-description>"},
			.description = StringView("Define window decoration paddings"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		auto values = StringView(args[0]);
		float f[4] = {nan(), nan(), nan(), nan()};
		uint32_t i = 0;
		values.split<StringView::Chars<','>>([&](StringView val) {
			if (i < 4) {
				f[i] = val.readFloat().get(nan());
			}
			++i;
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
			if (!target.window) {
				target.window = Rc<WindowInfo>::alloc();
			}
			target.window->decoration = Padding(f[0], f[1], f[2], f[3]);
			return true;
		}
		return false;
	}},
		CommandLineOption<ContextConfig>{.patterns = {"--device <#>"},
			.description = StringView("Force-disable Vulkan validation layers"),
			.callback = [](ContextConfig &target, StringView pattern,
								SpanView<StringView> args) -> bool {
		if (!target.loop) {
			target.loop = Rc<core::LoopInfo>::alloc();
		}
		target.loop->deviceIdx =
				StringView(args.at(0)).readInteger(10).get(core::InstanceDefaultDevice);
		return true;
	}},
	});
}

bool ContextConfig::readFromCommandLine(ContextConfig &ret, int argc, const char *argv[],
		const Callback<void(StringView)> &cb) {
	return getCommandLineParser().parse(ret, argc, argv,
			cb ? Callback<void(ContextConfig &, StringView)>(
						 [&](ContextConfig &, StringView arg) { cb(arg); })
			   : nullptr);
}

ContextConfig::ContextConfig(int argc, const char *argv[]) : ContextConfig() {
	readFromCommandLine(*this, argc, argv);
	platform::ContextController::acquireDefaultConfig(*this, nullptr);
}

ContextConfig::ContextConfig(NativeContextHandle *handle, Value &&value) : ContextConfig() {
	context->extra = move(value);
	platform::ContextController::acquireDefaultConfig(*this, handle);
}

ContextConfig::ContextConfig() {
	context = Rc<ContextInfo>::alloc();
	window = Rc<WindowInfo>::alloc();
	instance = Rc<core::InstanceInfo>::alloc();
	loop = Rc<core::LoopInfo>::alloc();

	context->bundleName = getAppconfigBundleName();
	context->appName = getAppconfigAppName();
	context->appVersionCode = getAppconfigVersionIndex();
	context->appVersion = toString(getAppconfigVersionVariant(), ".", getAppconfigVersionApi(), ".",
			getAppconfigVersionRev(), ".", getAppconfigVersionBuild());

	window->title = context->appName;
}

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
				Value(decoration.top),
				Value(decoration.left),
				Value(decoration.bottom),
				Value(decoration.right),
			},
			"decoration");

	if (density) {
		ret.setDouble(density, "density");
	}

	ret.setString(core::getImageFormatName(imageFormat), "imageFormat");
	ret.setString(core::getColorSpaceName(colorSpace), "colorSpace");
	ret.setString(core::getPresentModeName(preferredPresentMode), "preferredPresentMode");

	Value f;
	if (hasFlag(flags, WindowFlags::FixedBorder)) {
		f.addString("FixedBorder");
	}

	if (!f.empty()) {
		ret.setValue(move(f), "flags");
	}
	return ret;
}

Value ContextInfo::encode() const {
	Value ret;
	ret.setString(bundleName, "bundleName");
	ret.setString(appName, "appName");
	ret.setString(appVersion, "appVersion");
	ret.setString(userLanguage, "userLanguage");
	ret.setString(userAgent, "userAgent");
	ret.setString(launchUrl, "launchUrl");
	ret.setInteger(appVersionCode, "appVersionCode");
	ret.setInteger(appUpdateInterval.toMicros(), "appUpdateInterval");
	ret.setValue(extra, "extra");

	ret.setInteger(appThreadsCount, "appThreadsCount");
	ret.setInteger(mainThreadsCount, "mainThreadsCount");

	Value f;
	if (hasFlag(flags, ContextFlags::Headless)) {
		f.addString("Headless");
	}
	if (!f.empty()) {
		ret.setValue(move(f), "flags");
	}
	return ret;
}

Value ContextConfig::encode() const {
	Value ret;

	if (auto act = context->encode()) {
		ret.setValue(act, "activity");
	}

	if (auto win = window->encode()) {
		ret.setValue(win, "window");
	}

	if (auto inst = instance->encode()) {
		ret.setValue(inst, "instance");
	}

	if (auto l = loop->encode()) {
		ret.setValue(l, "loop");
	}

	Value f;
	if (hasFlag(flags, CommonFlags::Help)) {
		f.addString("Help");
	}
	if (hasFlag(flags, CommonFlags::Verbose)) {
		f.addString("Verbose");
	}
	if (hasFlag(flags, CommonFlags::Quiet)) {
		f.addString("Quiet");
	}
	if (!f.empty()) {
		ret.setValue(move(f), "flags");
	}
	return ret;
}

Value ThemeInfo::encode() const {
	Value ret;
	ret.setValue(colorScheme, "colorScheme");
	ret.setValue(iconTheme, "iconTheme");
	ret.setValue(cursorTheme, "cursorTheme");
	ret.setValue(monospaceFontName, "monospaceFontName");
	ret.setValue(defaultFontName, "defaultFontName");
	ret.setValue(cursorSize, "cursorSize");
	ret.setValue(scalingFactor, "scalingFactor");
	ret.setValue(textScaling, "textScaling");
	return ret;
}

} // namespace stappler::xenolith
