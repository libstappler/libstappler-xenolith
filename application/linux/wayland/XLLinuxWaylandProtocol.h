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

#ifndef XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDPROTOCOL_H_
#define XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDPROTOCOL_H_

#include "XLCommon.h"
#include "XLContextInfo.h"

#if LINUX

#include "linux/XLLinux.h"
#include "linux/XLLinuxXkbLibrary.h"
#include "XLCoreInput.h"

#include "linux/thirdparty/wayland-protocols/xdg-shell.h"
#include "linux/thirdparty/wayland-protocols/viewporter.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static const char *s_XenolithWaylandTag = "org.stappler.xenolith.wayland";

class WaylandLibrary;

enum class WaylandDecorationName {
	RightSide,
	TopRightCorner,
	TopSide,
	TopLeftCorner,
	BottomRightCorner,
	BottomSide,
	BottomLeftCorner,
	LeftSide,
	HeaderLeft,
	HeaderRight,
	HeaderCenter,
	HeaderBottom,
	IconClose,
	IconMaximize,
	IconMinimize,
	IconRestore
};

struct ViewporterInterface {
	const struct wl_interface *viewporter_types[6];

	const struct wl_message wp_viewporter_requests[2];
	const struct wl_message wp_viewport_requests[3];

	const struct wl_interface wp_viewporter_interface;
	const struct wl_interface wp_viewport_interface;

	ViewporterInterface(const struct wl_interface *wl_surface_interface);
};

struct XdgInterface {
	const struct wl_interface *xdg_shell_types[26];

	const struct wl_message xdg_wm_base_requests[4];
	const struct wl_message xdg_wm_base_events[1];
	const struct wl_message xdg_positioner_requests[10];
	const struct wl_message xdg_surface_requests[5];
	const struct wl_message xdg_surface_events[1];
	const struct wl_message xdg_toplevel_requests[14];
	const struct wl_message xdg_toplevel_events[4];
	const struct wl_message xdg_popup_requests[3];
	const struct wl_message xdg_popup_events[3];

	const struct wl_interface xdg_wm_base_interface;
	const struct wl_interface xdg_positioner_interface;
	const struct wl_interface xdg_surface_interface;
	const struct wl_interface xdg_toplevel_interface;
	const struct wl_interface xdg_popup_interface;

	XdgInterface(const struct wl_interface *wl_output_interface,
			const struct wl_interface *wl_seat_interface,
			const struct wl_interface *wl_surface_interface);
};

struct SP_PUBLIC WaylandBuffer : Ref {
	virtual ~WaylandBuffer();

	bool init(WaylandLibrary *lib, wl_shm_pool *wl_shm_pool, int32_t offset, int32_t width,
			int32_t height, int32_t stride, uint32_t format);

	Rc<WaylandLibrary> wayland;
	wl_buffer *buffer = nullptr;
	uint32_t width = 0;
	uint32_t height = 0;
};

struct ShadowBuffers {
	Rc<WaylandBuffer> top;
	Rc<WaylandBuffer> left;
	Rc<WaylandBuffer> bottom;
	Rc<WaylandBuffer> right;
	Rc<WaylandBuffer> topLeft;
	Rc<WaylandBuffer> topRight;
	Rc<WaylandBuffer> bottomLeft;
	Rc<WaylandBuffer> bottomRight;

	Rc<WaylandBuffer> topActive;
	Rc<WaylandBuffer> leftActive;
	Rc<WaylandBuffer> bottomActive;
	Rc<WaylandBuffer> rightActive;
	Rc<WaylandBuffer> topLeftActive;
	Rc<WaylandBuffer> topRightActive;
	Rc<WaylandBuffer> bottomLeftActive;
	Rc<WaylandBuffer> bottomRightActive;

	Rc<WaylandBuffer> headerLeft;
	Rc<WaylandBuffer> headerLeftActive;
	Rc<WaylandBuffer> headerRight;
	Rc<WaylandBuffer> headerRightActive;
	Rc<WaylandBuffer> headerCenter;
	Rc<WaylandBuffer> headerCenterActive;

	Rc<WaylandBuffer> iconClose;
	Rc<WaylandBuffer> iconMaximize;
	Rc<WaylandBuffer> iconMinimize;
	Rc<WaylandBuffer> iconRestore;

	Rc<WaylandBuffer> iconCloseActive;
	Rc<WaylandBuffer> iconMaximizeActive;
	Rc<WaylandBuffer> iconMinimizeActive;
	Rc<WaylandBuffer> iconRestoreActive;
};

struct KeyState {
	xkb_mod_index_t controlIndex = 0;
	xkb_mod_index_t altIndex = 0;
	xkb_mod_index_t shiftIndex = 0;
	xkb_mod_index_t superIndex = 0;
	xkb_mod_index_t capsLockIndex = 0;
	xkb_mod_index_t numLockIndex = 0;

	int32_t keyRepeatRate = 0;
	int32_t keyRepeatDelay = 0;
	int32_t keyRepeatInterval = 0;
	uint32_t modsDepressed = 0;
	uint32_t modsLatched = 0;
	uint32_t modsLocked = 0;

	core::InputKeyCode keycodes[256] = {core::InputKeyCode::Unknown};

	KeyState();
};

SP_PUBLIC bool allocateDecorations(WaylandLibrary *wayland, wl_shm *shm, ShadowBuffers *ret,
		uint32_t width, uint32_t inset, const Color3B &header, const Color3B &headerActive);

SP_PUBLIC std::ostream &operator<<(std::ostream &, WaylandDecorationName);

} // namespace stappler::xenolith::platform

#endif

#endif // XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDPROTOCOL_H_
