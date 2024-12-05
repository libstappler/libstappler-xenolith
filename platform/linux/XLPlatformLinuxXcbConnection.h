/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCBCONNECTION_H_
#define XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCBCONNECTION_H_

#include "XLPlatformLinuxXcb.h"
#include "XLPlatformLinuxXkb.h"
#include "XLCoreInput.h"

#if LINUX

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct XcbAtomInfo {
	XcbAtomIndex index;
	StringView name;
	bool onlyIfExists;
	xcb_atom_t value;
};

static XcbAtomInfo s_atomRequests[] = {
	{ XcbAtomIndex::WM_PROTOCOLS, "WM_PROTOCOLS", true, 0 },
	{ XcbAtomIndex::WM_DELETE_WINDOW, "WM_DELETE_WINDOW", true, 0 },
	{ XcbAtomIndex::WM_NAME, "WM_NAME", false, 0 },
	{ XcbAtomIndex::WM_ICON_NAME, "WM_ICON_NAME", false, 0 },
	{ XcbAtomIndex::_NET_WM_SYNC_REQUEST, "_NET_WM_SYNC_REQUEST", true, 0 },
	{ XcbAtomIndex::_NET_WM_SYNC_REQUEST_COUNTER, "_NET_WM_SYNC_REQUEST_COUNTER", true, 0 },
	{ XcbAtomIndex::SAVE_TARGETS, "SAVE_TARGETS", false, 0 },
	{ XcbAtomIndex::CLIPBOARD, "CLIPBOARD", false, 0 },
	{ XcbAtomIndex::PRIMARY, "PRIMARY", false, 0 },
	{ XcbAtomIndex::TARGETS, "TARGETS", false, 0 },
	{ XcbAtomIndex::MULTIPLE, "MULTIPLE", false, 0 },
	{ XcbAtomIndex::UTF8_STRING, "UTF8_STRING", false, 0 },
	{ XcbAtomIndex::XNULL, "NULL", false, 0 },
	{ XcbAtomIndex::XENOLITH_CLIPBOARD, "XENOLITH_CLIPBOARD", false, 0 },
};

struct XcbWindowInfo {
	uint8_t depth = XCB_COPY_FROM_PARENT;
	xcb_window_t parent;
	xcb_rectangle_t rect;
	xcb_visualid_t visual;

	uint32_t overrideRedirect = 0;
	uint32_t eventMask = 0;

	StringView title;
	StringView icon;
	StringView wmClass;

	bool overrideClose = true;
	bool enableSync = false;

	xcb_sync_int64_t syncValue = { 0, 0 };

	// output
	xcb_window_t window;
	xcb_sync_counter_t syncCounter;
};

struct ScreenInfo {
	uint16_t width;
	uint16_t height;
	uint16_t mwidth;
	uint16_t mheight;
	Vector<uint16_t> rates;
};

struct ModeInfo {
	uint32_t id;
    uint16_t width;
    uint16_t height;
    uint16_t rate;
    String name;
};

struct CrtcInfo {
	xcb_randr_crtc_t crtc;
	int16_t x;
	int16_t y;
	uint16_t width;
	uint16_t height;
	xcb_randr_mode_t mode;
	uint16_t rotation;
	uint16_t rotations;
	Vector<xcb_randr_output_t> outputs;
	Vector<xcb_randr_output_t> possible;
};

struct OutputInfo {
	xcb_randr_output_t output;
    xcb_randr_crtc_t crtc;
	Vector<xcb_randr_mode_t> modes;
	String name;
};

struct ScreenInfoData {
	Vector<xcb_randr_crtc_t> currentCrtcs;
	Vector<xcb_randr_output_t> currentOutputs;
	Vector<ModeInfo> currentModeInfo;
	Vector<ModeInfo> modeInfo;
	Vector<ScreenInfo> screenInfo;
	Vector<CrtcInfo> crtcInfo;

	OutputInfo primaryOutput;
	CrtcInfo primaryCrtc;
	ModeInfo primaryMode;
	xcb_timestamp_t config;
};

class XcbWindowInterface {
public:
	virtual void handleConfigureNotify(xcb_configure_notify_event_t *) { }

	virtual void handleButtonPress(xcb_button_press_event_t *) { }
	virtual void handleButtonRelease(xcb_button_release_event_t *) { }
	virtual void handleMotionNotify(xcb_motion_notify_event_t *) { }
	virtual void handleEnterNotify(xcb_enter_notify_event_t *) { }
	virtual void handleLeaveNotify(xcb_leave_notify_event_t *) { }
	virtual void handleFocusIn(xcb_focus_in_event_t *) { }
	virtual void handleFocusOut(xcb_focus_out_event_t *) { }
	virtual void handleKeyPress(xcb_key_press_event_t *) { }
	virtual void handleKeyRelease(xcb_key_release_event_t *) { }

	virtual void handleSelectionNotify(xcb_selection_notify_event_t *) { }
	virtual void handleSelectionRequest(xcb_selection_request_event_t *) { }

	virtual void handleSyncRequest(xcb_timestamp_t, xcb_sync_int64_t) { }
	virtual void handleCloseRequest() { }

	virtual void handleScreenChangeNotify(xcb_randr_screen_change_notify_event_t *) { }

	virtual void dispatchPendingEvents() { }
};

class XcbConnection : public Ref {
public:
	static void ReportError(int error);

	static core::InputKeyCode getKeysymCode(xcb_keysym_t sym);

	virtual ~XcbConnection();

	XcbConnection(XcbLibrary *);

	void poll();

	XcbLibrary *getXcb() const { return _xcb; }
	XkbLibrary *getXkb() const { return _xkb; }

	int getSocket() const { return _socket; }

	xcb_connection_t *getConnection() const { return _connection; }
	xcb_screen_t *getDefaultScreen() const { return _screen; }

	bool hasErrors() const;

	// get code from keysym mapping table
	core::InputKeyCode getKeyCode(xcb_keycode_t code) const;
	xcb_keysym_t getKeysym(xcb_keycode_t code, uint16_t state, bool resolveMods) const;
	xkb_keysym_t composeSymbol(xkb_keysym_t sym, core::InputKeyComposeState &compose) const;

	xcb_atom_t getAtom(XcbAtomIndex) const;

	bool createWindow(XcbWindowInfo &info) const;

	void attachWindow(xcb_window_t, XcbWindowInterface *);
	void detachWindow(xcb_window_t);

	ScreenInfoData getScreenInfo(xcb_screen_t *screen) const;
	ScreenInfoData getScreenInfo(xcb_window_t root) const;

protected:
	void initXkb();

	void updateXkbMapping();
	void updateXkbKey(xcb_keycode_t code);

	void updateKeysymMapping();

	XcbLibrary *_xcb;
	Rc<XkbLibrary> _xkb;
	xcb_connection_t *_connection = nullptr;
	int _screenNbr = -1;
	const xcb_setup_t *_setup = nullptr;
	xcb_screen_t *_screen = nullptr;
	int _socket = -1;

	XcbAtomInfo _atoms[sizeof(s_atomRequests) / sizeof(XcbAtomInfo)];

	bool _randrEnabled = true;
	uint8_t _randrFirstEvent = 0;

	bool _xkbSetup = false;
	int32_t _xkbDeviceId = 0;
	uint8_t _xkbFirstEvent = 0;
	uint8_t _xkbFirstError = 0;
	xkb_keymap *_xkbKeymap = nullptr;
	xkb_state *_xkbState = nullptr;
	xkb_compose_state *_xkbCompose = nullptr;
	core::InputKeyCode _keycodes[256] = { core::InputKeyCode::Unknown };

	xcb_key_symbols_t *_keysyms = nullptr;
	uint16_t _numlock = 0;
	uint16_t _shiftlock = 0;
	uint16_t _capslock = 0;
	uint16_t _modeswitch = 0;

	bool _syncEnabled = true;

	Map<xcb_window_t, XcbWindowInterface *> _windows;
};

}

#endif // LINUX

#endif /* XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCBCONNECTION_H_ */
