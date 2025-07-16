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

#include "XLApplicationConfig.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreLoop.h"
#include "platform/XLEdid.h"
#include "SPCommandLineParser.h"

#if ANDROID
struct ANativeActivity;
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Context;
class AppThread;
class AppWindow;

#if ANDROID
using NativeContextHandle = ANativeActivity;
#else
using NativeContextHandle = void;
#endif

enum class NetworkFlags {
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
	Local = (1 << 14)
};

SP_DEFINE_ENUM_AS_MASK(NetworkFlags)

enum class WindowLayerFlags : uint32_t {
	None,
	CursorText,
	CursorPointer,
	CursorHelp,
	CursorProgress,
	CursorWait,
	CursorCopy,
	CursorAlias,
	CursorNoDrop,
	CursorNotAllowed,
	CursorAllScroll,
	CursorRowResize,
	CursorColResize,
	CursorMask = 0xF,

	ResizableTop = 1 << 27,
	ResizableRight = 1 << 28,
	ResizableBottom = 1 << 29,
	ResizableLeft = 1 << 30,
	ResizeMask = ResizableTop | ResizableRight | ResizableBottom | ResizableLeft,
};

SP_DEFINE_ENUM_AS_MASK(WindowLayerFlags)

struct WindowLayer {
	Rect rect;
	WindowLayerFlags flags;

	bool operator==(const WindowLayer &) const = default;
	bool operator!=(const WindowLayer &) const = default;
};

enum class WindowFlags {
	None = 0,
	FixedBorder = 1 << 0,
	SingleWindow = 1 << 1, // no subwindows can be created
};

SP_DEFINE_ENUM_AS_MASK(WindowFlags)

struct ModeInfo {
	static const ModeInfo Current;
	static const ModeInfo Preferred;

	uint16_t width = 0;
	uint16_t height = 0;
	uint16_t rate = 0;

	auto operator<=>(const ModeInfo &) const = default;
};

struct MonitorId {
	static const MonitorId Primary;
	static const MonitorId None;

	String name;
	platform::EdidInfo edid;
};

struct MonitorInfo : MonitorId {
	IRect rect;
	Extent2 mm;

	uint32_t preferredMode = 0;
	uint32_t currentMode = 0;

	Vector<ModeInfo> modes;
};

struct ScreenInfo : public Ref {
	Vector<MonitorInfo> monitors;
	uint32_t primaryMonitor = 0;
};

struct SP_PUBLIC WindowInfo final : public Ref {
	String id;
	String title;
	URect rect = URect(0, 0, 1'024, 768);
	Padding decoration;
	float density = 0.0f;
	WindowFlags flags = WindowFlags::None;

	// initial fullscreen mode
	ModeInfo mode = ModeInfo::Current;
	MonitorId monitor = MonitorId::None;

	// TODO: extra window attributes go here

	core::PresentMode preferredPresentMode = core::PresentMode::Mailbox;
	core::ImageFormat imageFormat = core::ImageFormat::Undefined;
	core::ColorSpace colorSpace = core::ColorSpace::SRGB_NONLINEAR_KHR;

	core::FrameConstraints exportConstraints() const {
		return core::FrameConstraints{
			.extent = Extent2(rect.width, rect.height),
			.contentPadding = decoration,
			.transform = core::SurfaceTransformFlags::Identity,
			.density = density,
		};
	}

	Value encode() const;
};

struct SP_PUBLIC UpdateTime final {
	uint64_t global = 0; // global OS timer in microseconds
	uint64_t app = 0; // microseconds since application was started
	uint64_t delta = 0; // microseconds since last update
	float dt = 0.0f; // seconds since last update
};

enum class ContextFlags {
	None = 0,
	Headless = 1 << 0, // No application window
};

SP_DEFINE_ENUM_AS_MASK(ContextFlags)

struct SP_PUBLIC ContextInfo final : public Ref {
	String bundleName = "org.stappler.xenolith.test"; // application reverce-domain name
	String appName = "Xenolith"; // application human-readable name
	String appVersion = "0.0.1"; // application human-readable version
	String userLanguage = "ru-ru"; // current locale name
	String userAgent = "XenolithTestApp"; // networking user agent
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

enum class CommonFlags {
	None = 0,
	Help = 1 << 0,
	Verbose = 1 << 1,
	Quiet = 1 << 2,
};

SP_DEFINE_ENUM_AS_MASK(CommonFlags)

struct ContextConfig final {
	static CommandLineParser<ContextConfig> CommandLine;

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

using OpacityValue = ValueWrapper<uint8_t, class OpacityTag>;
using ZOrder = ValueWrapper<int16_t, class ZOrderTag>;

constexpr const ModeInfo ModeInfo::Preferred{maxOf<uint16_t>(), maxOf<uint16_t>(), 0};
constexpr const ModeInfo ModeInfo::Current{maxOf<uint16_t>(), maxOf<uint16_t>(), maxOf<uint16_t>()};
constexpr const MonitorId MonitorId::Primary{"__primary__"};
constexpr const MonitorId MonitorId::None;

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLCONTEXTINFO_H_ */
