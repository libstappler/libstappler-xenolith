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

#ifndef XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBCONNECTION_H_
#define XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBCONNECTION_H_

#include "SPBytesReader.h"
#include "XlCoreMonitorInfo.h"

#if LINUX

#include "linux/XLLinuxXkbLibrary.h"
#include "XLLinuxXcbLibrary.h"
#include "XLContextInfo.h"
#include "XLCoreInput.h"
#include "platform/XLContextController.h"

#include <xcb/xcb.h>
#include <xcb/xcb_errors.h>
#include <xcb/xproto.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class XcbWindow;
class XcbDisplayConfigManager;

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
	{XcbAtomIndex::_NET_WM_PING, "_NET_WM_PING", true, 0},
	{XcbAtomIndex::_NET_WM_PID, "_NET_WM_PID", true, 0},
	{XcbAtomIndex::_NET_WM_WINDOW_TYPE_DESKTOP, "_NET_WM_PID", true, 0},
	{XcbAtomIndex::_NET_WM_WINDOW_TYPE_DOCK, "_NET_WM_PID", true, 0},
	{XcbAtomIndex::_NET_WM_WINDOW_TYPE_TOOLBAR, "_NET_WM_WINDOW_TYPE_TOOLBAR", true, 0},
	{XcbAtomIndex::_NET_WM_WINDOW_TYPE_MENU, "_NET_WM_WINDOW_TYPE_MENU", true, 0},
	{XcbAtomIndex::_NET_WM_WINDOW_TYPE_UTILITY, "_NET_WM_WINDOW_TYPE_UTILITY", true, 0},
	{XcbAtomIndex::_NET_WM_WINDOW_TYPE_SPLASH, "_NET_WM_WINDOW_TYPE_SPLASH", true, 0},
	{XcbAtomIndex::_NET_WM_WINDOW_TYPE_DIALOG, "_NET_WM_WINDOW_TYPE_DIALOG", true, 0},
	{XcbAtomIndex::_NET_WM_WINDOW_TYPE_NORMAL, "_NET_WM_WINDOW_TYPE_NORMAL", true, 0},
	{XcbAtomIndex::_NET_WM_STATE, "_NET_WM_STATE", true, 0},
	{XcbAtomIndex::_NET_WM_STATE_MODAL, "_NET_WM_STATE_MODAL", true, 0},
	{XcbAtomIndex::_NET_WM_STATE_STICKY, "_NET_WM_STATE_STICKY", true, 0},
	{XcbAtomIndex::_NET_WM_STATE_MAXIMIZED_VERT, "_NET_WM_STATE_MAXIMIZED_VERT", true, 0},
	{XcbAtomIndex::_NET_WM_STATE_MAXIMIZED_HORZ, "_NET_WM_STATE_MAXIMIZED_HORZ", true, 0},
	{XcbAtomIndex::_NET_WM_STATE_SHADED, "_NET_WM_STATE_SHADED", true, 0},
	{XcbAtomIndex::_NET_WM_STATE_SKIP_TASKBAR, "_NET_WM_STATE_SKIP_TASKBAR", true, 0},
	{XcbAtomIndex::_NET_WM_STATE_SKIP_PAGER, "_NET_WM_STATE_SKIP_PAGER", true, 0},
	{XcbAtomIndex::_NET_WM_STATE_HIDDEN, "_NET_WM_STATE_HIDDEN", true, 0},
	{XcbAtomIndex::_NET_WM_STATE_FULLSCREEN, "_NET_WM_STATE_FULLSCREEN", true, 0},
	{XcbAtomIndex::_NET_WM_STATE_ABOVE, "_NET_WM_STATE_ABOVE", true, 0},
	{XcbAtomIndex::_NET_WM_STATE_BELOW, "_NET_WM_STATE_BELOW", true, 0},
	{XcbAtomIndex::_NET_WM_STATE_DEMANDS_ATTENTION, "_NET_WM_STATE_DEMANDS_ATTENTION", true, 0},
	{XcbAtomIndex::_NET_WM_STATE_FOCUSED, "_NET_WM_STATE_FOCUSED", true, 0},
	{XcbAtomIndex::_NET_WM_ACTION_MOVE, "_NET_WM_ACTION_MOVE", true, 0},
	{XcbAtomIndex::_NET_WM_ACTION_RESIZE, "_NET_WM_ACTION_RESIZE", true, 0},
	{XcbAtomIndex::_NET_WM_ACTION_MINIMIZE, "_NET_WM_ACTION_MINIMIZE", true, 0},
	{XcbAtomIndex::_NET_WM_ACTION_SHADE, "_NET_WM_ACTION_SHADE", true, 0},
	{XcbAtomIndex::_NET_WM_ACTION_STICK, "_NET_WM_ACTION_STICK", true, 0},
	{XcbAtomIndex::_NET_WM_ACTION_MAXIMIZE_HORZ, "_NET_WM_ACTION_MAXIMIZE_HORZ", true, 0},
	{XcbAtomIndex::_NET_WM_ACTION_MAXIMIZE_VERT, "_NET_WM_ACTION_MAXIMIZE_VERT", true, 0},
	{XcbAtomIndex::_NET_WM_ACTION_FULLSCREEN, "_NET_WM_ACTION_FULLSCREEN", true, 0},
	{XcbAtomIndex::_NET_WM_ACTION_CHANGE_DESKTOP, "_NET_WM_ACTION_CHANGE_DESKTOP", true, 0},
	{XcbAtomIndex::_NET_WM_ACTION_CLOSE, "_NET_WM_ACTION_CLOSE", true, 0},
	{XcbAtomIndex::_NET_WM_FULLSCREEN_MONITORS, "_NET_WM_FULLSCREEN_MONITORS", true, 0},
	{XcbAtomIndex::_NET_WM_BYPASS_COMPOSITOR, "_NET_WM_BYPASS_COMPOSITOR", true, 0},
	{XcbAtomIndex::_NET_SUPPORTED, "_NET_SUPPORTED", true, 0},
	{XcbAtomIndex::SAVE_TARGETS, "SAVE_TARGETS", false, 0},
	{XcbAtomIndex::CLIPBOARD, "CLIPBOARD", false, 0},
	{XcbAtomIndex::PRIMARY, "PRIMARY", false, 0},
	{XcbAtomIndex::TIMESTAMP, "TIMESTAMP", false, 0},
	{XcbAtomIndex::TARGETS, "TARGETS", false, 0},
	{XcbAtomIndex::MULTIPLE, "MULTIPLE", false, 0},
	{XcbAtomIndex::TEXT, "TEXT", false, 0},
	{XcbAtomIndex::UTF8_STRING, "UTF8_STRING", false, 0},
	{XcbAtomIndex::OCTET_STREAM, "application/octet-stream", false, 0},
	{XcbAtomIndex::ATOM_PAIR, "ATOM_PAIR", false, 0},
	{XcbAtomIndex::INCR, "INCR", false, 0},
	{XcbAtomIndex::XNULL, "NULL", false, 0},
	{XcbAtomIndex::XENOLITH_CLIPBOARD, "XENOLITH_CLIPBOARD", false, 0},
	{XcbAtomIndex::_XSETTINGS_SETTINGS, "_XSETTINGS_SETTINGS", true, 0},
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
	bool closed = false;

	xcb_sync_int64_t syncValue = {0, 0};
	uint64_t syncFrameOrder = 0;

	// output
	xcb_window_t window;
	xcb_sync_counter_t syncCounter;

	xcb_cursor_t cursorId = 0;
};

class XcbConnection final : public Ref {
public:
	static void ReportError(int error);

	static core::InputKeyCode getKeysymCode(xcb_keysym_t sym);

	virtual ~XcbConnection();

	XcbConnection(NotNull<XcbLibrary> xcb, NotNull<XkbLibrary> xkb,
			StringView display = StringView());

	Rc<DisplayConfigManager> makeDisplayConfigManager(
			Function<void(NotNull<DisplayConfigManager>)> &&);

	uint32_t poll();

	XcbLibrary *getXcb() const { return _xcb; }
	XkbLibrary *getXkb() const { return _xkb.lib; }

	XcbDisplayConfigManager *getDisplayConfigManager() const { return _displayConfig; }

	int getSocket() const { return _socket; }

	xcb_connection_t *getConnection() const { return _connection; }
	xcb_screen_t *getDefaultScreen() const { return _screen; }

	bool hasErrors() const;

	// get code from keysym mapping table
	core::InputKeyCode getKeyCode(xcb_keycode_t code) const;
	xcb_keysym_t getKeysym(xcb_keycode_t code, uint16_t state, bool resolveMods) const;

	xcb_atom_t getAtom(XcbAtomIndex) const;
	xcb_atom_t getAtom(StringView, bool onlyIfExists = false) const;
	StringView getAtomName(xcb_atom_t) const;

	bool hasCapability(XcbAtomIndex) const;
	bool hasCapability(xcb_atom_t) const;
	bool hasCapability(StringView) const;

	const char *getErrorMajorName(uint8_t) const;
	const char *getErrorMinorName(uint8_t, uint16_t) const;
	const char *getErrorName(uint8_t errorCode) const;

	void getAtomNames(SpanView<xcb_atom_t>, const Callback<void(SpanView<StringView>)> &);
	void getAtoms(SpanView<String>, const Callback<void(SpanView<xcb_atom_t>)> &);
	void getAtoms(SpanView<StringView>, const Callback<void(SpanView<xcb_atom_t>)> &);

	bool createWindow(const WindowInfo *, XcbWindowInfo &info) const;

	void attachWindow(xcb_window_t, XcbWindow *);
	void detachWindow(xcb_window_t);

	void fillTextInputData(core::InputEventData &, xcb_keycode_t, uint16_t state,
			bool textInputEnabled, bool compose);

	void notifyScreenChange();

	xcb_cursor_t loadCursor(SpanView<StringView>);

	bool setCursorId(xcb_window_t window, xcb_cursor_t);

	void readFromClipboard(Rc<ClipboardRequest> &&req);
	void writeToClipboard(Rc<ClipboardData> &&);

	Value getSettingsValue(StringView) const;
	uint32_t getUnscaledDpi() const { return (_xsettings.udpi == 0) ? 122'880 : _xsettings.udpi; }
	uint32_t getDpi() const { return (_xsettings.dpi == 0) ? 122'880 : _xsettings.dpi; }

	template <typename T>
	auto wrapXcbReply(T *t) const {
		return Ptr(t, [](T *ptr) { ::free(ptr); });
	}

	template <typename Fn, typename Cookie>
	auto perform(Fn *fn, Cookie cookie) const {
		xcb_generic_error_t *error = nullptr;
		auto reply = fn(_connection, cookie, &error);
		if (error) {
			printError("Fail to perform XCB function", error);
			::free(error);
			if (reply) {
				::free(reply);
			}
			return decltype(wrapXcbReply(reply))(nullptr);
		}
		if (reply) {
			return wrapXcbReply(reply);
		} else {
			printError("Fail to perform XCB function", error);
			return decltype(wrapXcbReply(reply))(nullptr);
		}
	}

	void printError(StringView, xcb_generic_error_t *) const;

protected:
	void updateKeysymMapping();

	bool checkCookie(xcb_void_cookie_t cookie, StringView errMessage);

	void continueClipboardProcessing();
	void finalizeClipboardWaiters(BytesView, xcb_atom_t);
	void handleSelectionNotify(xcb_selection_notify_event_t *);
	void handleSelectionClear(xcb_selection_clear_event_t *);
	void handlePropertyNotify(xcb_property_notify_event_t *);

	xcb_atom_t writeClipboardSelection(xcb_window_t requestor, xcb_atom_t target,
			xcb_atom_t targetProperty);
	void handleSelectionRequest(xcb_selection_request_event_t *);

	void handleSelectionUpdateNotify(xcb_xfixes_selection_notify_event_t *);

	void readXSettings();

	struct RandrInfo {
		bool enabled = true;
		bool initialized = false;
		uint8_t firstEvent = 0;

		uint32_t majorVersion = 0;
		uint32_t minorVersion = 0;
	};

	struct XfixesInfo {
		bool enabled = true;
		bool initialized = false;
		uint8_t firstEvent = 0;
		uint8_t firstError = 0;

		uint32_t majorVersion = 0;
		uint32_t minorVersion = 0;
	};

	struct KeyInfo {
		uint16_t numlock = 0;
		uint16_t shiftlock = 0;
		uint16_t capslock = 0;
		uint16_t modeswitch = 0;

		xcb_key_symbols_t *keysyms = nullptr;
	};

	struct ClipboardTransfer {
		xcb_window_t requestor;
		xcb_atom_t property;
		xcb_atom_t type;

		Rc<ClipboardData> data;
		uint32_t current = 0;
		std::deque<Bytes> chunks;
	};

	struct ClipboardInfo {
		xcb_window_t window = 0;
		xcb_window_t owner = 0;

		Vector<Rc<ClipboardRequest>> requests;
		std::multimap<xcb_atom_t, Rc<ClipboardRequest>> waiters;
		Vector<Bytes> incrBuffer;
		xcb_atom_t incrType = 0;
		bool incr = false;

		Rc<ClipboardData> data;
		Vector<xcb_atom_t> typeAtoms;
		xcb_timestamp_t selectionTimestamp = XCB_CURRENT_TIME;

		Map<uint64_t, ClipboardTransfer> transfers;

		ClipboardTransfer *addTransfer(xcb_window_t w, xcb_atom_t p, ClipboardTransfer &&t) {
			auto id = uint64_t(w) << 32 | uint64_t(p);
			auto it = transfers.find(id);
			if (it != transfers.end()) {
				return nullptr;
			}
			return &transfers.emplace(id, move(t)).first->second;
		}

		ClipboardTransfer *getTransfer(xcb_window_t w, xcb_atom_t p) {
			auto it = transfers.find(uint64_t(w) << 32 | uint64_t(p));
			if (it == transfers.end()) {
				return nullptr;
			}
			return &it->second;
		}

		void cancelTransfer(xcb_window_t w, xcb_atom_t p) {
			transfers.erase(uint64_t(w) << 32 | uint64_t(p));
		}
	};

	struct SettingsValue {
		Value value;
		uint32_t serial = 0;
	};

	struct XSettingsInfo {
		xcb_window_t window = 0;
		xcb_atom_t selection = 0;
		xcb_atom_t property = 0;
		uint32_t serial = 0;

		uint32_t udpi = 0;
		uint32_t dpi = 0;

		Map<String, SettingsValue> settings;
	};

	XcbLibrary *_xcb = nullptr;
	xcb_connection_t *_connection = nullptr;
	xcb_errors_context_t *_errors = nullptr;
	uint32_t _safeReqeustSize = 128_KiB;
	uint32_t _maxRequestSize = 0;
	int _screenNbr = -1;
	const xcb_setup_t *_setup = nullptr;
	xcb_screen_t *_screen = nullptr;
	xcb_cursor_context_t *_cursorContext = nullptr;
	int _socket = -1;

	XcbAtomInfo _atoms[sizeof(s_atomRequests) / sizeof(XcbAtomInfo)];
	mutable Map<String, xcb_atom_t> _namedAtoms;
	mutable Map<xcb_atom_t, String> _atomNames;

	bool _syncEnabled = true;

	Map<xcb_window_t, XcbWindow *> _windows;

	KeyInfo _keys;
	XkbInfo _xkb;
	RandrInfo _randr;
	XfixesInfo _xfixes;
	ClipboardInfo _clipboard;
	XSettingsInfo _xsettings;
	Rc<XcbDisplayConfigManager> _displayConfig;
	Vector<StringView> _capabilitiesByNames;
	Vector<xcb_atom_t> _capabilitiesByAtoms;
};

} // namespace stappler::xenolith::platform

#endif

#endif /* XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBCONNECTION_H_ */
