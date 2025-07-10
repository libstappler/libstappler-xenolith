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

#ifndef XENOLITH_APPLICATION_LINUX_XLLINUXXCBCONNECTION_H_
#define XENOLITH_APPLICATION_LINUX_XLLINUXXCBCONNECTION_H_

#include "XLLinuxXcbLibrary.h"
#include "XLLinuxXkbLibrary.h"
#include "XLContextInfo.h"
#include "XLCoreInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class XcbWindow;

struct XcbAtomInfo {
	XcbAtomIndex index;
	StringView name;
	bool onlyIfExists;
	xcb_atom_t value;
};

static XcbAtomInfo s_atomRequests[] = {
	{XcbAtomIndex::WM_PROTOCOLS, "WM_PROTOCOLS", true, 0},
	{XcbAtomIndex::WM_DELETE_WINDOW, "WM_DELETE_WINDOW", true, 0},
	{XcbAtomIndex::WM_NAME, "WM_NAME", false, 0},
	{XcbAtomIndex::WM_ICON_NAME, "WM_ICON_NAME", false, 0},
	{XcbAtomIndex::_NET_WM_SYNC_REQUEST, "_NET_WM_SYNC_REQUEST", true, 0},
	{XcbAtomIndex::_NET_WM_SYNC_REQUEST_COUNTER, "_NET_WM_SYNC_REQUEST_COUNTER", true, 0},
	{XcbAtomIndex::SAVE_TARGETS, "SAVE_TARGETS", false, 0},
	{XcbAtomIndex::CLIPBOARD, "CLIPBOARD", false, 0},
	{XcbAtomIndex::PRIMARY, "PRIMARY", false, 0},
	{XcbAtomIndex::TARGETS, "TARGETS", false, 0},
	{XcbAtomIndex::MULTIPLE, "MULTIPLE", false, 0},
	{XcbAtomIndex::UTF8_STRING, "UTF8_STRING", false, 0},
	{XcbAtomIndex::XNULL, "NULL", false, 0},
	{XcbAtomIndex::XENOLITH_CLIPBOARD, "XENOLITH_CLIPBOARD", false, 0},
};

struct XcbWindowInfo {
	uint8_t depth = XCB_COPY_FROM_PARENT;
	xcb_window_t parent;
	xcb_visualid_t visual;

	xcb_rectangle_t rect;

	uint32_t overrideRedirect = 0;
	uint32_t eventMask = 0;

	StringView title;
	StringView icon;
	StringView wmClass;

	bool overrideClose = true;
	bool enableSync = false;

	xcb_sync_int64_t syncValue = {0, 0};
	uint64_t syncFrameOrder = 0;

	// output
	xcb_window_t window;
	xcb_sync_counter_t syncCounter;

	xcb_cursor_t cursorId = 0;
};

struct ScreenInfo {
	uint16_t width;
	uint16_t height;
	uint16_t mwidth;
	uint16_t mheight;
	Vector<uint16_t> rates;

	void describe(const CallbackStream &);
};

struct ModeInfo {
	uint32_t id;
	uint16_t width;
	uint16_t height;
	uint16_t rate;
	String name;

	void describe(const CallbackStream &);
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

	void describe(const CallbackStream &, uint32_t indent = 0);
};

struct OutputInfo {
	xcb_randr_output_t output;
	xcb_randr_crtc_t crtc;
	Vector<xcb_randr_mode_t> modes;
	String name;

	void describe(const CallbackStream &);
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

	void describe(const CallbackStream &);
};

class XcbConnection : public Ref {
public:
	static void ReportError(int error);

	static core::InputKeyCode getKeysymCode(xcb_keysym_t sym);

	virtual ~XcbConnection();

	XcbConnection(NotNull<XcbLibrary> xcb, NotNull<XkbLibrary> xkb,
			StringView display = StringView());

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

	bool createWindow(const WindowInfo *, XcbWindowInfo &info) const;

	void attachWindow(xcb_window_t, XcbWindow *);
	void detachWindow(xcb_window_t);

	ScreenInfoData getScreenInfo(xcb_screen_t *screen) const;
	ScreenInfoData getScreenInfo(xcb_window_t root) const;

	void fillTextInputData(core::InputEventData &, xcb_keycode_t, uint16_t state,
			bool textInputEnabled, bool compose);

	xcb_cursor_t loadCursor(StringView);
	xcb_cursor_t loadCursor(std::initializer_list<StringView>);

	bool setCursorId(xcb_window_t window, xcb_cursor_t);

protected:
	void initXkb();

	void updateXkbMapping();
	void updateXkbKey(xcb_keycode_t code);

	void updateKeysymMapping();

	bool checkCookie(xcb_void_cookie_t cookie, StringView errMessage);

	XcbLibrary *_xcb = nullptr;
	Rc<XkbLibrary> _xkb;
	xcb_connection_t *_connection = nullptr;
	int _screenNbr = -1;
	const xcb_setup_t *_setup = nullptr;
	xcb_screen_t *_screen = nullptr;
	xcb_cursor_context_t *_cursorContext = nullptr;
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
	core::InputKeyCode _keycodes[256] = {core::InputKeyCode::Unknown};

	xcb_key_symbols_t *_keysyms = nullptr;
	uint16_t _numlock = 0;
	uint16_t _shiftlock = 0;
	uint16_t _capslock = 0;
	uint16_t _modeswitch = 0;

	bool _syncEnabled = true;

	Map<xcb_window_t, XcbWindow *> _windows;
};

} // namespace stappler::xenolith::platform

#endif /* XENOLITH_APPLICATION_LINUX_XLLINUXXCBCONNECTION_H_ */
