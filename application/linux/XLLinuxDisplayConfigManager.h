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

#ifndef XENOLITH_APPLICATION_LINUX_XLLINUXDISPLAYCONFIGMANAGER_H_
#define XENOLITH_APPLICATION_LINUX_XLLINUXDISPLAYCONFIGMANAGER_H_

#include "XLCommon.h"
#include "XlCoreMonitorInfo.h"

#if LINUX

#include "XLContextInfo.h"
#include "XLLinux.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct DisplayMode {
	uint32_t xid = 0; // mode id
	core::ModeInfo mode;
	String name;

	float preferredScale = 1.0f;
	Vector<float> scales;

	bool preferred = false;
	bool current = false;

	bool operator==(const DisplayMode &m) const noexcept = default;
	bool operator==(const core::ModeInfo &m) const noexcept { return mode == m; }
};

struct PhysicalDisplay {
	uint32_t xid = 0; // output id
	MonitorId id;
	Extent2 mm;
	Vector<DisplayMode> modes;

	const DisplayMode *getMode(const core::ModeInfo &m) const;
	const DisplayMode &getCurrent() const;

	bool operator==(const PhysicalDisplay &m) const noexcept = default;
};

struct LogicalDisplay {
	uint32_t xid = 0; // crtc id
	IRect rect;
	float scale = 1.0f;
	uint32_t transform = 0;
	bool primary = false;
	Vector<MonitorId> monitors;

	bool hasMonitor(const MonitorId &id) const;

	bool operator==(const LogicalDisplay &m) const noexcept = default;
};

struct DisplayConfig : public Ref {
	uint32_t serial = 0;
	Vector<PhysicalDisplay> monitors;
	Vector<LogicalDisplay> logical;

	const PhysicalDisplay *getMonitor(const MonitorId &id) const;

	bool isEqual(const DisplayConfig *) const;
};

class DisplayConfigManager : public Ref {
public:
	virtual ~DisplayConfigManager() = default;

	virtual bool init(Function<void(NotNull<DisplayConfigManager>)> &&);

	virtual void invalidate();

	void exportScreenInfo(NotNull<core::ScreenInfo>) const;

	// Set mode for the monitor, and reset modes for all other monitors to default
	// Only single monitor mode can be set with this function
	void setModeExclusive(xenolith::MonitorId, xenolith::ModeInfo, Function<void(Status)> &&,
			Ref *);

	void setMode(xenolith::MonitorId, xenolith::ModeInfo, Function<void(Status)> &&, Ref *);

	// Reset monitor modes to captured defaults (modes before first setMonitorMode call)
	void restoreMode(Function<void(Status)> &&, Ref *);

	bool hasSavedMode() const { return _savedConfig; }

	DisplayConfig *getCurrentConfig() const { return _currentConfig; }

protected:
	// extract only current modes
	Rc<DisplayConfig> extractCurrentConfig(NotNull<DisplayConfig>) const;
	void adjustDisplay(NotNull<DisplayConfig>) const;

	virtual void handleConfigChanged(NotNull<DisplayConfig>);

	virtual void prepareDisplayConfigUpdate(Function<void(DisplayConfig *)> &&);
	virtual void applyDisplayConfig(NotNull<DisplayConfig>, Function<void(Status)> &&);

	Function<void(NotNull<DisplayConfigManager>)> _onConfigChanged;
	Vector<Function<void()>> _waitForConfigNotification;
	Rc<DisplayConfig> _currentConfig;
	Rc<DisplayConfig> _savedConfig;
};

} // namespace stappler::xenolith::platform

#endif

#endif // XENOLITH_APPLICATION_LINUX_XLLINUXDISPLAYCONFIGMANAGER_H_
