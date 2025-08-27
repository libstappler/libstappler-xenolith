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

#ifndef XENOLITH_APPLICATION_XLCONTEXTINFO_H_
#define XENOLITH_APPLICATION_XLCONTEXTINFO_H_

#include "XLWindowInfo.h"
#include "XLCoreLoop.h" // IWYU pragma: keep
#include "XLCoreInstance.h"
#include "SPCommandLineParser.h"

#if ANDROID
struct ANativeActivity;
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Context;
class AppThread;

#if ANDROID
using NativeContextHandle = ANativeActivity;
#else
using NativeContextHandle = void;
#endif

enum class NetworkFlags : uint32_t {
	None,
	Internet = (1 << 0),
	Congested = (1 << 1),
	Metered = (1 << 2),
	Restricted = (1 << 3),
	Roaming = (1 << 4),
	Suspended = (1 << 5),
	Vpn = (1 << 6),
	PrioritizeBandwidth = (1 << 7),
	PrioritizeLatency = (1 << 8),
	TemporarilyNotMetered = (1 << 9),
	Trusted = (1 << 10),
	Validated = (1 << 11),
	WifiP2P = (1 << 12),
	CaptivePortal = (1 << 13),
	Local = (1 << 14),
	Wired = (1 << 15),
	Wireless = (1 << 16),
};

SP_DEFINE_ENUM_AS_MASK(NetworkFlags)

struct SP_PUBLIC UpdateTime final {
	uint64_t global = 0; // global OS timer in microseconds
	uint64_t app = 0; // microseconds since application was started
	uint64_t delta = 0; // microseconds since last update
	float dt = 0.0f; // seconds since last update
};

enum class ContextFlags : uint32_t {
	None = 0,

	Headless = 1 << 0,

	// Application shold be terminated when all it's windows were closed
	DestroyWhenAllWindowsClosed = 1 << 1,
};

SP_DEFINE_ENUM_AS_MASK(ContextFlags)

struct SP_PUBLIC ContextInfo final : public Ref {
	String bundleName = "org.stappler.xenolith.test"; // application reverce-domain name
	String appName = "Xenolith"; // application human-readable name
	String appVersion = "0.0.1"; // application human-readable version
	String userLanguage = ""; //"ru-ru"; // current locale name
	String userAgent = "XenolithApp"; // networking user agent
	String launchUrl; // initial launch URL (deep link)

	uint32_t appVersionCode = 0; // version code in Vulkan format (see SP_MAKE_API_VERSION)

	// Application event update interval (NOT screen update interval)
	TimeInterval appUpdateInterval = TimeInterval::seconds(1);

	uint16_t mainThreadsCount = config::getDefaultMainThreads(); // threads for general and gl tasks
	uint16_t appThreadsCount = config::getDefaultAppThreads(); // threads for app-level tasks

	ContextFlags flags = ContextFlags::None;

	Value extra;

	Value encode() const;
};

enum class CommonFlags : uint32_t {
	None = 0,
	Help = 1 << 0,
	Verbose = 1 << 1,
	Quiet = 1 << 2,
};

SP_DEFINE_ENUM_AS_MASK(CommonFlags)

struct ContextConfig final {
	static CommandLineParser<ContextConfig> getCommandLineParser();

	static bool readFromCommandLine(ContextConfig &, int argc, const char *argv[],
			const Callback<void(StringView)> &cb = nullptr);

	CommonFlags flags = CommonFlags::None;

	Rc<ContextInfo> context;
	Rc<WindowInfo> window;
	Rc<core::InstanceInfo> instance;
	Rc<core::LoopInfo> loop;

	ContextConfig(int argc, const char *argv[]);
	ContextConfig(NativeContextHandle *, Value && = Value());

	Value encode() const;

private:
	ContextConfig();
};

struct ThemeInfo {
	String colorScheme;
	String iconTheme;
	String cursorTheme;
	String documentFontName;
	String monospaceFontName;
	String defaultFontName;
	int32_t cursorSize = 0;
	uint32_t scalingFactor = 0;
	float textScaling = 1.0f;
	bool leftHandedMouse = false;
	uint32_t doubleClickInterval = 500'000; // in microseconds

	Value encode() const;

	bool operator==(const ThemeInfo &) const = default;
	bool operator!=(const ThemeInfo &) const = default;
};

using OpacityValue = ValueWrapper<uint8_t, class OpacityTag>;
using ZOrder = ValueWrapper<int16_t, class ZOrderTag>;

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLCONTEXTINFO_H_ */
