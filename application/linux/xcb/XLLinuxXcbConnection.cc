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

#include "XLLinuxXcbConnection.h"
#include "SPSpanView.h"
#include "XLLinuxXcbDisplayConfigManager.h"
#include "SPCore.h"
#include "SPMemory.h"
#include "XLLinuxXcbWindow.h"
#include "linux/xcb/XLLinuxXcbLibrary.h"
#include <X11/keysym.h>
#include <algorithm>
#include <xcb/randr.h>

#define XL_X11_DEBUG 0

#if XL_X11_DEBUG
#define XL_X11_LOG(...) log::format(log::Debug, "XCB", __VA_ARGS__)
#else
#define XL_X11_LOG(...)
#endif

// use GLFW mappings as a fallback for XKB
uint32_t _glfwKeySym2Unicode(unsigned int keysym);

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

void XcbConnection::ReportError(int error) {
	switch (error) {
	case XCB_CONN_ERROR:
		stappler::log::error("XcbView",
				"XCB_CONN_ERROR: socket error, pipe error or other stream error");
		break;
	case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
		stappler::log::error("XcbView",
				"XCB_CONN_CLOSED_EXT_NOTSUPPORTED: extension is not supported");
		break;
	case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
		stappler::log::error("XcbView", "XCB_CONN_CLOSED_MEM_INSUFFICIENT: out of memory");
		break;
	case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
		stappler::log::error("XcbView", "XCB_CONN_CLOSED_REQ_LEN_EXCEED: too large request");
		break;
	case XCB_CONN_CLOSED_PARSE_ERR:
		stappler::log::error("XcbView",
				"XCB_CONN_CLOSED_PARSE_ERR: error during parsing display string");
		break;
	case XCB_CONN_CLOSED_INVALID_SCREEN:
		stappler::log::
				error("XcbView",
						"XCB_CONN_CLOSED_INVALID_SCREEN: server does not have a screen matching "
						"the " "display");
		break;
	case XCB_CONN_CLOSED_FDPASSING_FAILED:
		stappler::log::error("XcbView", "XCB_CONN_CLOSED_FDPASSING_FAILED: fail to pass some FD");
		break;
	}
}

core::InputKeyCode XcbConnection::getKeysymCode(xcb_keysym_t sym) {
	// from GLFW: x11_init.c
	switch (sym) {
	case XK_KP_0: return core::InputKeyCode::KP_0;
	case XK_KP_1: return core::InputKeyCode::KP_1;
	case XK_KP_2: return core::InputKeyCode::KP_2;
	case XK_KP_3: return core::InputKeyCode::KP_3;
	case XK_KP_4: return core::InputKeyCode::KP_4;
	case XK_KP_5: return core::InputKeyCode::KP_5;
	case XK_KP_6: return core::InputKeyCode::KP_6;
	case XK_KP_7: return core::InputKeyCode::KP_7;
	case XK_KP_8: return core::InputKeyCode::KP_8;
	case XK_KP_9: return core::InputKeyCode::KP_9;
	case XK_KP_Separator:
	case XK_KP_Decimal: return core::InputKeyCode::KP_DECIMAL;
	case XK_Escape: return core::InputKeyCode::ESCAPE;
	case XK_Tab: return core::InputKeyCode::TAB;
	case XK_Shift_L: return core::InputKeyCode::LEFT_SHIFT;
	case XK_Shift_R: return core::InputKeyCode::RIGHT_SHIFT;
	case XK_Control_L: return core::InputKeyCode::LEFT_CONTROL;
	case XK_Control_R: return core::InputKeyCode::RIGHT_CONTROL;
	case XK_Meta_L:
	case XK_Alt_L: return core::InputKeyCode::LEFT_ALT;
	case XK_Mode_switch: // Mapped to Alt_R on many keyboards
	case XK_ISO_Level3_Shift: // AltGr on at least some machines
	case XK_Meta_R:
	case XK_Alt_R: return core::InputKeyCode::RIGHT_ALT;
	case XK_Super_L: return core::InputKeyCode::LEFT_SUPER;
	case XK_Super_R: return core::InputKeyCode::RIGHT_SUPER;
	case XK_Menu: return core::InputKeyCode::MENU;
	case XK_Num_Lock: return core::InputKeyCode::NUM_LOCK;
	case XK_Caps_Lock: return core::InputKeyCode::CAPS_LOCK;
	case XK_Print: return core::InputKeyCode::PRINT_SCREEN;
	case XK_Scroll_Lock: return core::InputKeyCode::SCROLL_LOCK;
	case XK_Pause: return core::InputKeyCode::PAUSE;
	case XK_Delete: return core::InputKeyCode::DELETE;
	case XK_BackSpace: return core::InputKeyCode::BACKSPACE;
	case XK_Return: return core::InputKeyCode::ENTER;
	case XK_Home: return core::InputKeyCode::HOME;
	case XK_End: return core::InputKeyCode::END;
	case XK_Page_Up: return core::InputKeyCode::PAGE_UP;
	case XK_Page_Down: return core::InputKeyCode::PAGE_DOWN;
	case XK_Insert: return core::InputKeyCode::INSERT;
	case XK_Left: return core::InputKeyCode::LEFT;
	case XK_Right: return core::InputKeyCode::RIGHT;
	case XK_Down: return core::InputKeyCode::DOWN;
	case XK_Up: return core::InputKeyCode::UP;
	case XK_F1: return core::InputKeyCode::F1;
	case XK_F2: return core::InputKeyCode::F2;
	case XK_F3: return core::InputKeyCode::F3;
	case XK_F4: return core::InputKeyCode::F4;
	case XK_F5: return core::InputKeyCode::F5;
	case XK_F6: return core::InputKeyCode::F6;
	case XK_F7: return core::InputKeyCode::F7;
	case XK_F8: return core::InputKeyCode::F8;
	case XK_F9: return core::InputKeyCode::F9;
	case XK_F10: return core::InputKeyCode::F10;
	case XK_F11: return core::InputKeyCode::F11;
	case XK_F12: return core::InputKeyCode::F12;
	case XK_F13: return core::InputKeyCode::F13;
	case XK_F14: return core::InputKeyCode::F14;
	case XK_F15: return core::InputKeyCode::F15;
	case XK_F16: return core::InputKeyCode::F16;
	case XK_F17: return core::InputKeyCode::F17;
	case XK_F18: return core::InputKeyCode::F18;
	case XK_F19: return core::InputKeyCode::F19;
	case XK_F20: return core::InputKeyCode::F20;
	case XK_F21: return core::InputKeyCode::F21;
	case XK_F22: return core::InputKeyCode::F22;
	case XK_F23: return core::InputKeyCode::F23;
	case XK_F24: return core::InputKeyCode::F24;
	case XK_F25:
		return core::InputKeyCode::F25;

		// Numeric keypad
	case XK_KP_Divide: return core::InputKeyCode::KP_DIVIDE;
	case XK_KP_Multiply: return core::InputKeyCode::KP_MULTIPLY;
	case XK_KP_Subtract: return core::InputKeyCode::KP_SUBTRACT;
	case XK_KP_Add:
		return core::InputKeyCode::KP_ADD;

		// These should have been detected in secondary keysym test above!
	case XK_KP_Insert: return core::InputKeyCode::KP_0;
	case XK_KP_End: return core::InputKeyCode::KP_1;
	case XK_KP_Down: return core::InputKeyCode::KP_2;
	case XK_KP_Page_Down: return core::InputKeyCode::KP_3;
	case XK_KP_Left: return core::InputKeyCode::KP_4;
	case XK_KP_Right: return core::InputKeyCode::KP_6;
	case XK_KP_Home: return core::InputKeyCode::KP_7;
	case XK_KP_Up: return core::InputKeyCode::KP_8;
	case XK_KP_Page_Up: return core::InputKeyCode::KP_9;
	case XK_KP_Delete: return core::InputKeyCode::KP_DECIMAL;
	case XK_KP_Equal: return core::InputKeyCode::KP_EQUAL;
	case XK_KP_Enter:
		return core::InputKeyCode::KP_ENTER;

		// Last resort: Check for printable keys (should not happen if the XKB
		// extension is available). This will give a layout dependent mapping
		// (which is wrong, and we may miss some keys, especially on non-US
		// keyboards), but it's better than nothing...
	case XK_a: return core::InputKeyCode::A;
	case XK_b: return core::InputKeyCode::B;
	case XK_c: return core::InputKeyCode::C;
	case XK_d: return core::InputKeyCode::D;
	case XK_e: return core::InputKeyCode::E;
	case XK_f: return core::InputKeyCode::F;
	case XK_g: return core::InputKeyCode::G;
	case XK_h: return core::InputKeyCode::H;
	case XK_i: return core::InputKeyCode::I;
	case XK_j: return core::InputKeyCode::J;
	case XK_k: return core::InputKeyCode::K;
	case XK_l: return core::InputKeyCode::L;
	case XK_m: return core::InputKeyCode::M;
	case XK_n: return core::InputKeyCode::N;
	case XK_o: return core::InputKeyCode::O;
	case XK_p: return core::InputKeyCode::P;
	case XK_q: return core::InputKeyCode::Q;
	case XK_r: return core::InputKeyCode::R;
	case XK_s: return core::InputKeyCode::S;
	case XK_t: return core::InputKeyCode::T;
	case XK_u: return core::InputKeyCode::U;
	case XK_v: return core::InputKeyCode::V;
	case XK_w: return core::InputKeyCode::W;
	case XK_x: return core::InputKeyCode::X;
	case XK_y: return core::InputKeyCode::Y;
	case XK_z: return core::InputKeyCode::Z;
	case XK_1: return core::InputKeyCode::_1;
	case XK_2: return core::InputKeyCode::_2;
	case XK_3: return core::InputKeyCode::_3;
	case XK_4: return core::InputKeyCode::_4;
	case XK_5: return core::InputKeyCode::_5;
	case XK_6: return core::InputKeyCode::_6;
	case XK_7: return core::InputKeyCode::_7;
	case XK_8: return core::InputKeyCode::_8;
	case XK_9: return core::InputKeyCode::_9;
	case XK_0: return core::InputKeyCode::_0;
	case XK_space: return core::InputKeyCode::SPACE;
	case XK_minus: return core::InputKeyCode::MINUS;
	case XK_equal: return core::InputKeyCode::EQUAL;
	case XK_bracketleft: return core::InputKeyCode::LEFT_BRACKET;
	case XK_bracketright: return core::InputKeyCode::RIGHT_BRACKET;
	case XK_backslash: return core::InputKeyCode::BACKSLASH;
	case XK_semicolon: return core::InputKeyCode::SEMICOLON;
	case XK_apostrophe: return core::InputKeyCode::APOSTROPHE;
	case XK_grave: return core::InputKeyCode::GRAVE_ACCENT;
	case XK_comma: return core::InputKeyCode::COMMA;
	case XK_period: return core::InputKeyCode::PERIOD;
	case XK_slash: return core::InputKeyCode::SLASH;
	case XK_less: return core::InputKeyCode::WORLD_1; // At least in some layouts...
	default: break;
	}
	return core::InputKeyCode::Unknown;
}

XcbConnection::~XcbConnection() {
	if (_errors) {
		_xcb->xcb_errors_context_free(_errors);
		_errors = nullptr;
	}

	if (_clipboard.window) {
		_xcb->xcb_destroy_window(_connection, _clipboard.window);
	}

	if (_cursorContext) {
		_xcb->xcb_cursor_context_free(_cursorContext);
		_cursorContext = nullptr;
	}

	if (_keys.keysyms) {
		_xcb->xcb_key_symbols_free(_keys.keysyms);
		_keys.keysyms = nullptr;
	}
	if (_connection) {
		_xcb->xcb_disconnect(_connection);
		_connection = nullptr;
	}
}

XcbConnection::XcbConnection(NotNull<XcbLibrary> xcb, NotNull<XkbLibrary> xkb, StringView d)
: _xkb(xkb) {
	_xcb = xcb;
	_xkb.lib = xkb;

	_connection = _xcb->xcb_connect(
			d.empty() ? nullptr : (d.terminated() ? d.data() : d.str<Interface>().data()),
			&_screenNbr); // always not null

	_maxRequestSize = _xcb->xcb_get_maximum_request_length(_connection);
	_safeReqeustSize = std::min(_maxRequestSize, _safeReqeustSize);

	_setup = _xcb->xcb_get_setup(_connection);

	_socket = _xcb->xcb_get_file_descriptor(_connection); // assume it's non-blocking

	int screenNbr = _screenNbr;
	auto iter = _xcb->xcb_setup_roots_iterator(_setup);
	for (; iter.rem; --screenNbr, _xcb->xcb_screen_next(&iter)) {
		if (screenNbr == 0) {
			_screen = iter.data;
			break;
		}
	}

	xcb_randr_query_version_cookie_t randrVersionCookie;
	xcb_xfixes_query_version_cookie_t xfixesVersionCookie;

	if (_xcb->hasRandr()) {
		auto ext = _xcb->xcb_get_extension_data(_connection, _xcb->xcb_randr_id);
		if (ext) {
			_randr.enabled = true;
			_randr.firstEvent = ext->first_event;
			randrVersionCookie = _xcb->xcb_randr_query_version(_connection,
					XcbLibrary::RANDR_MAJOR_VERSION, XcbLibrary::RANDR_MINOR_VERSION);
		}
	}

	if (_xcb->hasSync()) {
		auto ext = _xcb->xcb_get_extension_data(_connection, _xcb->xcb_sync_id);
		if (ext) {
			_syncEnabled = true;
		}
	}

	if (_xkb.lib && _xkb.lib->hasX11() && _xcb->hasXkb()) {
		_xkb.initXcb(_xcb, _connection);
	}

	if (_xcb->hasXfixes()) {
		auto ext = _xcb->xcb_get_extension_data(_connection, _xcb->xcb_xfixes_id);
		if (ext) {
			_xfixes.enabled = true;
			_xfixes.firstEvent = ext->first_event;
			_xfixes.firstError = ext->first_error;

			xfixesVersionCookie = _xcb->xcb_xfixes_query_version(_connection,
					XcbLibrary::XFIXES_MAJOR_VERSION, XcbLibrary::XFIXES_MINOR_VERSION);
		}
	}

	// read atoms from connection
	xcb_intern_atom_cookie_t atomCookies[sizeof(s_atomRequests) / sizeof(XcbAtomInfo)];

	size_t i = 0;
	for (auto &it : s_atomRequests) {
		atomCookies[i] = _xcb->xcb_intern_atom(_connection, it.onlyIfExists ? 1 : 0, it.name.size(),
				it.name.data());
		++i;
	}

	// make fake window for clipboard
	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t values[2];
	values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
			| XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;

	/* Ask for our window's Id */
	_clipboard.window = _xcb->xcb_generate_id(_connection);

	_xcb->xcb_create_window(_connection,
			XCB_COPY_FROM_PARENT, // depth (same as root)
			_clipboard.window, // window Id
			_screen->root, // parent window
			0, 0, 100, 100,
			0, // border_width
			XCB_WINDOW_CLASS_INPUT_OUTPUT, // class
			_screen->root_visual, // visual
			mask, values);

	_xcb->xcb_flush(_connection);

	if (_xcb->xcb_cursor_context_new(_connection, _screen, &_cursorContext) < 0) {
		log::warn("XcbConnection", "Fail to load cursor context");
		_cursorContext = nullptr;
	}

	memcpy(_atoms, s_atomRequests, sizeof(s_atomRequests));

	i = 0;
	for (auto &it : atomCookies) {
		auto reply = perform(_xcb->xcb_intern_atom_reply, it);
		if (reply) {
			_atoms[i].value = reply->atom;
			_namedAtoms.emplace(_atoms[i].name.str<Interface>(), reply->atom);
			_atomNames.emplace(reply->atom, _atoms[i].name.str<Interface>());
		} else {
			_atoms[i].value = 0;
		}
		++i;
	}

	xcb_get_property_cookie_t netSupportedCookie;
	if (auto a = getAtom(XcbAtomIndex::_NET_SUPPORTED)) {
		netSupportedCookie = _xcb->xcb_get_property(_connection, 0, _screen->root, a,
				XCB_GET_PROPERTY_TYPE_ANY, 0, maxOf<uint32_t>() / 4);
	}

	if (_randr.enabled) {
		if (auto versionReply = perform(_xcb->xcb_randr_query_version_reply, randrVersionCookie)) {
			_randr.majorVersion = versionReply->major_version;
			_randr.minorVersion = versionReply->minor_version;
			_randr.initialized = true;

			_xcb->xcb_randr_select_input(_connection, _clipboard.window,
					XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE | XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE
							| XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE);
		}
	}

	if (_xfixes.enabled) {
		if (auto versionReply =
						perform(_xcb->xcb_xfixes_query_version_reply, xfixesVersionCookie)) {
			_xfixes.majorVersion = versionReply->major_version;
			_xfixes.minorVersion = versionReply->minor_version;
			_xfixes.initialized = true;

			_xcb->xcb_xfixes_select_selection_input(_connection, _clipboard.window,
					getAtom(XcbAtomIndex::CLIPBOARD),
					XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER
							| XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY
							| XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE);
		}
	}

	_xcb->xcb_errors_context_new(_connection, &_errors);

	// try XSETTINGS
	_xsettings.selection = getAtom(toString("_XSETTINGS_S", d.readInteger(10).get(0)), true);
	_xsettings.property = getAtom(XcbAtomIndex::_XSETTINGS_SETTINGS);
	if (_xsettings.selection && _xsettings.property) {
		auto cookie = _xcb->xcb_get_selection_owner(_connection, _xsettings.selection);
		auto reply = perform(_xcb->xcb_get_selection_owner_reply, cookie);
		if (reply && reply->owner) {
			_xsettings.window = reply->owner;
			// subscribe on target window's events
			const uint32_t values[] = {
				XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE};
			_xcb->xcb_change_window_attributes(_connection, reply->owner, XCB_CW_EVENT_MASK,
					values);
			readXSettings();
		}
	}

	if (_randr.initialized && _randr.majorVersion == 1 && _randr.minorVersion >= 5) {
		_displayConfig = Rc<XcbDisplayConfigManager>::create(this, nullptr);
	}

	if (netSupportedCookie.sequence) {
		auto reply = perform(_xcb->xcb_get_property_reply, netSupportedCookie);
		if (reply) {
			auto atoms = (xcb_atom_t *)_xcb->xcb_get_property_value(reply);
			auto len = _xcb->xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);

			_capabilitiesByAtoms = makeSpanView(atoms, len).vec<Interface>();
			std::sort(_capabilitiesByAtoms.begin(), _capabilitiesByAtoms.end());

			getAtomNames(_capabilitiesByAtoms, [&](SpanView<StringView> strs) {
				_capabilitiesByNames = strs.vec<Interface>();
				std::sort(_capabilitiesByNames.begin(), _capabilitiesByNames.end());
			});
		}
	}
}

Rc<DisplayConfigManager> XcbConnection::makeDisplayConfigManager(
		Function<void(NotNull<DisplayConfigManager>)> &&cb) {
	if (_displayConfig) {
		_displayConfig->setCallback(sp::move(cb));
	}
	return _displayConfig;
}

template <typename Event>
static bool XcbConnection_forwardToWindow(StringView eventName,
		const Map<xcb_window_t, XcbWindow *> &windows, xcb_window_t window, Event *event,
		void (XcbWindow::*ptr)(Event *), Set<XcbWindow *> *eventWindows = nullptr) {
	auto wIt = windows.find(window);
	if (wIt != windows.end()) {
		(*wIt->second.*ptr)(event);
		if (eventWindows) {
			eventWindows->emplace(wIt->second);
		}
		return true;
	}
	log::warn("XcbConnection", "No window ", window, " attached for event ", eventName);
	return false;
}

template <typename Event>
static bool XcbConnection_forwardToWindow(StringView eventName,
		const Map<xcb_window_t, XcbWindow *> &windows, xcb_window_t window, Event *event,
		const Callback<void(Event *, XcbWindow *)> &cb) {
	auto wIt = windows.find(window);
	if (wIt != windows.end()) {
		cb(event, wIt->second);
		return true;
	}
	log::warn("XcbConnection", "No window ", window, " attached for event ", eventName);
	return false;
}

uint32_t XcbConnection::poll() {
	uint32_t ret = 0;

	xcb_generic_event_t *e;

	Set<XcbWindow *> eventWindows;

	while ((e = _xcb->xcb_poll_for_event(_connection))) {
		auto et = e->response_type & 0x7f;
		switch (et) {
		case 0: {
			auto err = reinterpret_cast<xcb_generic_error_t *>(e);
			printError("Connection error", err);
			log::error("XcbConnection", "X11 error: ", int(err->error_code));
			break;
		}
		case XCB_EXPOSE: XL_X11_LOG("XCB_EXPOSE"); break;
		case XCB_PROPERTY_NOTIFY:
			handlePropertyNotify(reinterpret_cast<xcb_property_notify_event_t *>(e));
			break;
		case XCB_VISIBILITY_NOTIFY: XL_X11_LOG("XCB_VISIBILITY_NOTIFY"); break;
		case XCB_MAP_NOTIFY: XL_X11_LOG("XCB_MAP_NOTIFY"); break;
		case XCB_REPARENT_NOTIFY: XL_X11_LOG("XCB_REPARENT_NOTIFY"); break;
		case XCB_COLORMAP_NOTIFY: XL_X11_LOG("XCB_COLORMAP_NOTIFY"); break;
		case XCB_CONFIGURE_REQUEST: XL_X11_LOG("XCB_CONFIGURE_REQUEST"); break;
		case XCB_RESIZE_REQUEST: XL_X11_LOG("XCB_RESIZE_REQUEST"); break;

		case XCB_SELECTION_NOTIFY:
			if (reinterpret_cast<xcb_selection_notify_event_t *>(e)->requestor
					== _clipboard.window) {
				handleSelectionNotify(reinterpret_cast<xcb_selection_notify_event_t *>(e));
			}
			break;
		case XCB_SELECTION_CLEAR:
			if (reinterpret_cast<xcb_selection_clear_event_t *>(e)->owner == _clipboard.window) {
				handleSelectionClear(reinterpret_cast<xcb_selection_clear_event_t *>(e));
			}
			break;
		case XCB_SELECTION_REQUEST:
			handleSelectionRequest((xcb_selection_request_event_t *)e);
			break;
		case XCB_BUTTON_PRESS:
			XcbConnection_forwardToWindow("XCB_BUTTON_PRESS", _windows,
					((xcb_button_press_event_t *)e)->event, (xcb_button_press_event_t *)e,
					&XcbWindow::handleButtonPress, &eventWindows);
			break;
		case XCB_BUTTON_RELEASE:
			XcbConnection_forwardToWindow("XCB_BUTTON_RELEASE", _windows,
					((xcb_button_release_event_t *)e)->event, (xcb_button_release_event_t *)e,
					&XcbWindow::handleButtonRelease, &eventWindows);
			break;
		case XCB_MOTION_NOTIFY:
			XcbConnection_forwardToWindow("XCB_MOTION_NOTIFY", _windows,
					((xcb_motion_notify_event_t *)e)->event, (xcb_motion_notify_event_t *)e,
					&XcbWindow::handleMotionNotify, &eventWindows);
			break;

		case XCB_ENTER_NOTIFY:
			XcbConnection_forwardToWindow("XCB_ENTER_NOTIFY", _windows,
					((xcb_enter_notify_event_t *)e)->event, (xcb_enter_notify_event_t *)e,
					&XcbWindow::handleEnterNotify, &eventWindows);
			break;
		case XCB_LEAVE_NOTIFY:
			XcbConnection_forwardToWindow("XCB_LEAVE_NOTIFY", _windows,
					((xcb_leave_notify_event_t *)e)->event, (xcb_leave_notify_event_t *)e,
					&XcbWindow::handleLeaveNotify, &eventWindows);
			break;
		case XCB_FOCUS_IN:
			XcbConnection_forwardToWindow("XCB_FOCUS_IN", _windows,
					((xcb_focus_in_event_t *)e)->event, (xcb_focus_in_event_t *)e,
					&XcbWindow::handleFocusIn, &eventWindows);
			// Update key mappings in case layout was changed
			updateKeysymMapping();
			break;
		case XCB_FOCUS_OUT:
			XcbConnection_forwardToWindow("XCB_FOCUS_OUT", _windows,
					((xcb_focus_out_event_t *)e)->event, (xcb_focus_out_event_t *)e,
					&XcbWindow::handleFocusOut, &eventWindows);
			break;
		case XCB_KEY_PRESS:
			XcbConnection_forwardToWindow("XCB_KEY_PRESS", _windows,
					((xcb_key_press_event_t *)e)->event, (xcb_key_press_event_t *)e,
					&XcbWindow::handleKeyPress, &eventWindows);
			break;
		case XCB_KEY_RELEASE:
			XcbConnection_forwardToWindow("XCB_KEY_RELEASE", _windows,
					((xcb_key_release_event_t *)e)->event, (xcb_key_release_event_t *)e,
					&XcbWindow::handleKeyRelease, &eventWindows);
			break;
		case XCB_CONFIGURE_NOTIFY:
			XcbConnection_forwardToWindow("XCB_CONFIGURE_NOTIFY", _windows,
					((xcb_configure_notify_event_t *)e)->event, (xcb_configure_notify_event_t *)e,
					&XcbWindow::handleConfigureNotify, &eventWindows);
			break;
		case XCB_CLIENT_MESSAGE:
			XcbConnection_forwardToWindow("XCB_CLIENT_MESSAGE", _windows,
					((xcb_client_message_event_t *)e)->window, (xcb_client_message_event_t *)e,
					Callback<void(xcb_client_message_event_t *, XcbWindow *)>(
							[&](xcb_client_message_event_t *event, XcbWindow *w) {
				if (event->type == _atoms[toInt(XcbAtomIndex::WM_PROTOCOLS)].value) {
					if (event->data.data32[0]
							== _atoms[toInt(XcbAtomIndex::WM_DELETE_WINDOW)].value) {
						w->handleCloseRequest();
					} else if (event->data.data32[0]
							== _atoms[toInt(XcbAtomIndex::_NET_WM_SYNC_REQUEST)].value) {
						xcb_sync_int64_t value;
						value.lo = event->data.data32[2];
						value.hi = static_cast<int32_t>(event->data.data32[3]);
						w->handleSyncRequest(event->data.data32[1], value);
					} else if (event->data.data32[0]
							== _atoms[toInt(XcbAtomIndex::_NET_WM_PING)].value) {
						if (event->window == _screen->root) {
							return;
						}

						xcb_client_message_event_t reply = *event;
						reply.response_type = XCB_CLIENT_MESSAGE;
						reply.window = _screen->root;
						_xcb->xcb_send_event(_connection, 0, _screen->root,
								XCB_EVENT_MASK_STRUCTURE_NOTIFY
										| XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
								(const char *)&reply);
						_xcb->xcb_flush(_connection);
					} else {
						log::error("XcbView", "Unknown protocol message: ", event->window,
								" of type ", event->type, ": ", event->data.data32[0]);
					}
				} else {
					log::error("XcbView", "Unknown client message: ", event->window, " of type ",
							event->type, ": ", event->data.data32[0]);
				}
			}));
			break;
		case XCB_MAPPING_NOTIFY: {
			xcb_mapping_notify_event_t *ev = (xcb_mapping_notify_event_t *)e;
			if (_keys.keysyms) {
				_xcb->xcb_refresh_keyboard_mapping(_keys.keysyms, ev);
			}
			XL_X11_LOG("XCB_MAPPING_NOTIFY: %d %d %d", (int)ev->request, (int)ev->first_keycode,
					(int)ev->count);
			break;
		}
		default:
			if (et == _xkb.firstEvent) {
				switch (e->pad0) {
				case XCB_XKB_NEW_KEYBOARD_NOTIFY: _xkb.initXcb(_xcb, _connection); break;
				case XCB_XKB_MAP_NOTIFY: _xkb.updateXkbMapping(_connection); break;
				case XCB_XKB_STATE_NOTIFY: {
					xcb_xkb_state_notify_event_t *ev = (xcb_xkb_state_notify_event_t *)e;
					_xkb.lib->xkb_state_update_mask(_xkb.state, ev->baseMods, ev->latchedMods,
							ev->lockedMods, ev->baseGroup, ev->latchedGroup, ev->lockedGroup);
					break;
				}
				}
			} else if (et == _randr.firstEvent) {
				switch (e->pad0) {
				case XCB_RANDR_SCREEN_CHANGE_NOTIFY:
					log::debug("XcbConnection", "XCB_RANDR_SCREEN_CHANGE_NOTIFY");
					break;
				case XCB_RANDR_NOTIFY:
					if (_displayConfig) {
						_displayConfig->update();
					}
					break;
				default: break;
				}
			} else if (et == _xfixes.firstEvent) {
				switch (e->pad0) {
				case XCB_XFIXES_SELECTION_NOTIFY:
					handleSelectionUpdateNotify(
							reinterpret_cast<xcb_xfixes_selection_notify_event_t *>(e));
					break;
				default: break;
				}
			} else {
				/* Unknown event type, ignore it */
				XL_X11_LOG("Unknown event: %d", et);
			}
			break;
		}

		/* Free the Generic Event */
		::free(e);

		++ret;
	}

	for (auto &it : eventWindows) { it->dispatchPendingEvents(); }

	return ret;
}

bool XcbConnection::hasErrors() const {
	auto err = _xcb->xcb_connection_has_error(_connection);
	if (err != 0) {
		ReportError(err);
		return true;
	}
	return false;
}

core::InputKeyCode XcbConnection::getKeyCode(xcb_keycode_t code) const {
	return _xkb.keycodes[code];
}

xcb_atom_t XcbConnection::getAtom(XcbAtomIndex index) const { return _atoms[toInt(index)].value; }

xcb_atom_t XcbConnection::getAtom(StringView name, bool onlyIfExists) const {
	auto it = _namedAtoms.find(name);
	if (it != _namedAtoms.end()) {
		return it->second;
	}

	auto cookie = _xcb->xcb_intern_atom(_connection, onlyIfExists, name.size(), name.data());

	xcb_generic_error_t *error = nullptr;
	auto reply = _xcb->xcb_intern_atom_reply(_connection, cookie, &error);
	if (error || !reply) {
		printError(toString("Fail to xcb_intern_atom_reply for '", name, "'"), error);
		if (error) {
			::free(error);
		}
	}

	if (reply) {
		auto atom = _namedAtoms.emplace(name.str<Interface>(), reply->atom).first->second;
		_atomNames.emplace(reply->atom, name.str<Interface>());
		free(reply);
		return atom;
	}

	return 0;
}

StringView XcbConnection::getAtomName(xcb_atom_t atom) const {
	auto it = _atomNames.find(atom);
	if (it != _atomNames.end()) {
		return it->second;
	}

	auto cookie = _xcb->xcb_get_atom_name_unchecked(_connection, atom);
	auto reply = _xcb->xcb_get_atom_name_reply(_connection, cookie, nullptr);
	if (reply) {
		auto data = _xcb->xcb_get_atom_name_name(reply);
		auto ret = String(data, _xcb->xcb_get_atom_name_name_length(reply));
		auto it = _atomNames.emplace(atom, ret).first;
		::free(reply);
		return it->second;
	}
	return StringView();
}

bool XcbConnection::hasCapability(XcbAtomIndex index) const {
	if (auto a = getAtom(index)) {
		return hasCapability(a);
	}
	return false;
}

bool XcbConnection::hasCapability(xcb_atom_t atom) const {
	auto lb = std::lower_bound(_capabilitiesByAtoms.begin(), _capabilitiesByAtoms.end(), atom);
	if (lb != _capabilitiesByAtoms.end() && *lb == atom) {
		return true;
	}
	return false;
}

bool XcbConnection::hasCapability(StringView str) const {
	auto lb = std::lower_bound(_capabilitiesByNames.begin(), _capabilitiesByNames.end(), str);
	if (lb != _capabilitiesByNames.end() && *lb == str) {
		return true;
	}
	return false;
}

void XcbConnection::getAtomNames(SpanView<xcb_atom_t> ids,
		const Callback<void(SpanView<StringView>)> &cb) {
	Vector<StringView> names;
	names.resize(ids.size());

	Vector<std::tuple<xcb_get_atom_name_cookie_t, xcb_atom_t, StringView *>> cookies;

	uint32_t idx = 0;
	for (auto &id : ids) {
		auto iit = _atomNames.find(id);
		if (iit != _atomNames.end()) {
			names[idx] = iit->second;
		} else {
			cookies.emplace_back(_xcb->xcb_get_atom_name_unchecked(_connection, id), id,
					&names[idx]);
		}
		++idx;
	}

	for (auto &it : cookies) {
		auto reply = _xcb->xcb_get_atom_name_reply(_connection, std::get<0>(it), nullptr);
		if (reply) {
			auto data = _xcb->xcb_get_atom_name_name(reply);
			auto ret = String(data, _xcb->xcb_get_atom_name_name_length(reply));
			auto aIt = _atomNames.emplace(std::get<1>(it), ret).first;
			_namedAtoms.emplace(ret, std::get<1>(it));
			*std::get<2>(it) = aIt->second;
			::free(reply);
		}
	}

	cb(names);
}

void XcbConnection::getAtoms(SpanView<String> names,
		const Callback<void(SpanView<xcb_atom_t>)> &cb) {
	Vector<StringView> views;
	views.reserve(names.size());
	for (auto &it : names) { views.emplace_back(it); }
	getAtoms(views, cb);
}

void XcbConnection::getAtoms(SpanView<StringView> names,
		const Callback<void(SpanView<xcb_atom_t>)> &cb) {
	Vector<xcb_atom_t> atoms;
	atoms.resize(names.size());

	Vector<std::tuple<xcb_intern_atom_cookie_t, StringView, xcb_atom_t *>> cookies;

	uint32_t idx = 0;
	for (auto &name : names) {
		auto iit = _namedAtoms.find(name);
		if (iit != _namedAtoms.end()) {
			atoms[idx] = iit->second;
		} else {
			cookies.emplace_back(_xcb->xcb_intern_atom(_connection, 0, name.size(), name.data()),
					name, &atoms[idx]);
		}
		++idx;
	}

	for (auto &it : cookies) {
		auto reply = _xcb->xcb_intern_atom_reply(_connection, std::get<0>(it), nullptr);
		if (reply) {
			auto atom = reply->atom;
			auto aIt = _namedAtoms.emplace(std::get<1>(it).str<Interface>(), atom).first;
			_atomNames.emplace(atom, std::get<1>(it).str<Interface>());
			*std::get<2>(it) = aIt->second;
			::free(reply);
		}
	}

	cb(atoms);
}

const char *XcbConnection::getErrorMajorName(uint8_t major) const {
	return _xcb->xcb_errors_get_name_for_major_code(_errors, major);
}

const char *XcbConnection::getErrorMinorName(uint8_t major, uint16_t minor) const {
	return _xcb->xcb_errors_get_name_for_minor_code(_errors, major, minor);
}

const char *XcbConnection::getErrorName(uint8_t errorCode) const {
	return _xcb->xcb_errors_get_name_for_error(_errors, errorCode, nullptr);
}

bool XcbConnection::createWindow(const WindowInfo *winfo, XcbWindowInfo &xinfo) const {
	uint32_t mask = /*XCB_CW_BACK_PIXEL | */ XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
	uint32_t values[3];
	//values[0] = _defaultScreen->white_pixel;
	values[0] = xinfo.overrideRedirect;
	values[1] = xinfo.eventMask;

	/* Ask for our window's Id */
	xinfo.window = _xcb->xcb_generate_id(_connection);

	_xcb->xcb_create_window(_connection,
			xinfo.depth, // depth (same as root)
			xinfo.window, // window Id
			xinfo.parent, // parent window
			xinfo.rect.x, xinfo.rect.y, xinfo.rect.width, xinfo.rect.height,
			0, // border_width
			XCB_WINDOW_CLASS_INPUT_OUTPUT, // class
			xinfo.visual, // visual
			mask, values);

	if (!xinfo.title.empty()) {
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, xinfo.window,
				XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, xinfo.title.size(), xinfo.title.data());
	}
	if (!xinfo.icon.empty()) {
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, xinfo.window,
				XCB_ATOM_WM_ICON_NAME, XCB_ATOM_STRING, 8, xinfo.icon.size(), xinfo.icon.data());
	}
	if (!xinfo.wmClass.empty()) {
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, xinfo.window,
				XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, xinfo.wmClass.size(), xinfo.wmClass.data());
	}

	char buf[512] = {0};
	if (::gethostname(buf, 512) == 0) {
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, xinfo.window,
				XCB_ATOM_WM_CLIENT_MACHINE, XCB_ATOM_STRING, 8, strlen(buf), buf);
	}

	auto pid = ::getpid();
	if (auto a = getAtom(XcbAtomIndex::_NET_WM_PID)) {
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, xinfo.window, a,
				XCB_ATOM_CARDINAL, 32, 1, &pid);
	}

	uint32_t nProtocols = 0;
	xcb_atom_t protocolAtoms[2];

	if (xinfo.overrideClose && _atoms[toInt(XcbAtomIndex::WM_DELETE_WINDOW)].value) {
		protocolAtoms[nProtocols++] = _atoms[toInt(XcbAtomIndex::WM_DELETE_WINDOW)].value;
	}

	if (_atoms[toInt(XcbAtomIndex::_NET_WM_PING)].value) {
		protocolAtoms[nProtocols++] = _atoms[toInt(XcbAtomIndex::_NET_WM_PING)].value;
	}

	if (_syncEnabled && xinfo.enableSync
			&& _atoms[toInt(XcbAtomIndex::_NET_WM_SYNC_REQUEST)].value) {
		xinfo.syncValue.hi = 0;
		xinfo.syncValue.lo = 0;

		xinfo.syncCounter = _xcb->xcb_generate_id(_connection);
		_xcb->xcb_sync_create_counter(_connection, xinfo.syncCounter, xinfo.syncValue);
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, xinfo.window,
				_atoms[toInt(XcbAtomIndex::_NET_WM_SYNC_REQUEST_COUNTER)].value, XCB_ATOM_CARDINAL,
				32, 1, &xinfo.syncCounter);
	}

	if (nProtocols && _atoms[toInt(XcbAtomIndex::WM_PROTOCOLS)].value) {
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, xinfo.window,
				_atoms[toInt(XcbAtomIndex::WM_PROTOCOLS)].value, XCB_ATOM_ATOM, 32, nProtocols,
				protocolAtoms);
	}

	_xcb->xcb_flush(_connection);

	if (!hasErrors()) {
		return true;
	}

	xinfo.window = 0;
	xinfo.syncCounter = 0;

	return false;
}

void XcbConnection::attachWindow(xcb_window_t window, XcbWindow *iface) {
	_windows.emplace(window, iface);
}

void XcbConnection::detachWindow(xcb_window_t window) { _windows.erase(window); }

void XcbConnection::fillTextInputData(core::InputEventData &event, xcb_keycode_t detail,
		uint16_t state, bool textInputEnabled, bool compose) {
	if (_xkb.initialized) {
		event.key.keycode = getKeyCode(detail);
		event.key.compose = core::InputKeyComposeState::Nothing;
		event.key.keysym = getKeysym(detail, state, false);
		if (textInputEnabled) {
			if (compose) {
				const auto keysym = _xkb.composeSymbol(
						_xkb.lib->xkb_state_key_get_one_sym(_xkb.state, detail), event.key.compose);
				const uint32_t cp = _xkb.lib->xkb_keysym_to_utf32(keysym);
				if (cp != 0 && keysym != XKB_KEY_NoSymbol) {
					event.key.keychar = cp;
				} else {
					event.key.keychar = 0;
				}
			} else {
				event.key.keychar = _xkb.lib->xkb_state_key_get_utf32(_xkb.state, detail);
			}
		} else {
			event.key.keychar = 0;
		}
	} else {
		auto sym = getKeysym(detail, state, false); // state-inpependent keysym
		event.key.keycode = getKeysymCode(sym);
		event.key.compose = core::InputKeyComposeState::Nothing;
		event.key.keysym = sym;
		if (textInputEnabled) {
			event.key.keychar = _glfwKeySym2Unicode(getKeysym(detail, state, true));
		} else {
			event.key.keychar = 0;
		}
	}
}

void XcbConnection::notifyScreenChange() {
	for (auto &it : _windows) { it.second->notifyScreenChange(); }
}

xcb_keysym_t XcbConnection::getKeysym(xcb_keycode_t code, uint16_t state, bool resolveMods) const {
	xcb_keysym_t k0, k1;

	if (!resolveMods) {
		k0 = _xcb->xcb_key_symbols_get_keysym(_keys.keysyms, code, 0);
		// resolve only numlock
		if ((state & _keys.numlock)) {
			k1 = _xcb->xcb_key_symbols_get_keysym(_keys.keysyms, code, 1);
			if (_xcb->xcb_is_keypad_key(k1)) {
				if ((state & XCB_MOD_MASK_SHIFT)
						|| ((state & XCB_MOD_MASK_LOCK) && (state & _keys.shiftlock))) {
					return k0;
				} else {
					return k1;
				}
			}
		}
		return k0;
	}

	if (state & _keys.modeswitch) {
		k0 = _xcb->xcb_key_symbols_get_keysym(_keys.keysyms, code, 2);
		k1 = _xcb->xcb_key_symbols_get_keysym(_keys.keysyms, code, 3);
	} else {
		k0 = _xcb->xcb_key_symbols_get_keysym(_keys.keysyms, code, 0);
		k1 = _xcb->xcb_key_symbols_get_keysym(_keys.keysyms, code, 1);
	}

	if (k1 == XCB_NO_SYMBOL) {
		k1 = k0;
	}

	if ((state & _keys.numlock) && _xcb->xcb_is_keypad_key(k1)) {
		if ((state & XCB_MOD_MASK_SHIFT)
				|| ((state & XCB_MOD_MASK_LOCK) && (state & _keys.shiftlock))) {
			return k0;
		} else {
			return k1;
		}
	} else if (!(state & XCB_MOD_MASK_SHIFT) && !(state & XCB_MOD_MASK_LOCK)) {
		return k0;
	} else if (!(state & XCB_MOD_MASK_SHIFT)
			&& ((state & XCB_MOD_MASK_LOCK) && (state & _keys.capslock))) {
		if (k0 >= XK_0 && k0 <= XK_9) {
			return k0;
		}
		return k1;
	} else if ((state & XCB_MOD_MASK_SHIFT)
			&& ((state & XCB_MOD_MASK_LOCK) && (state & _keys.capslock))) {
		return k1;
	} else if ((state & XCB_MOD_MASK_SHIFT)
			|| ((state & XCB_MOD_MASK_LOCK) && (state & _keys.shiftlock))) {
		return k1;
	}

	return XCB_NO_SYMBOL;
}

xcb_cursor_t XcbConnection::loadCursor(SpanView<StringView> list) {
	xcb_cursor_t cursor = XCB_CURSOR_NONE;
	for (auto &it : list) {
		cursor = _xcb->xcb_cursor_load_cursor(_cursorContext,
				it.terminated() ? it.data() : it.str<Interface>().data());
		if (cursor != XCB_CURSOR_NONE) {
			return cursor;
		}
	}
	return cursor;
}

bool XcbConnection::setCursorId(xcb_window_t window, uint32_t cursorId) {
	_xcb->xcb_change_window_attributes(_connection, window, XCB_CW_CURSOR, (uint32_t[]){cursorId});
	_xcb->xcb_flush(_connection);

	/*xcb_font_t font = _xcb->xcb_generate_id(_connection);
	xcb_void_cookie_t fontCookie =
			_xcb->xcb_open_font_checked(_connection, font, strlen("cursor"), "cursor");
	if (!checkCookie(fontCookie, "can't open font")) {
		return false;
	}

	xcb_cursor_t cursor = _xcb->xcb_generate_id(_connection);
	_xcb->xcb_create_glyph_cursor(_connection, cursor, font, font, cursorId, cursorId + 1, 0, 0, 0,
			0, 0, 0);

	xcb_gcontext_t gc = _xcb->xcb_generate_id(_connection);

	uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
	uint32_t values_list[3];
	values_list[0] = _screen->black_pixel;
	values_list[1] = _screen->white_pixel;
	values_list[2] = font;

	xcb_void_cookie_t gcCookie =
			_xcb->xcb_create_gc_checked(_connection, gc, window, mask, values_list);
	if (!checkCookie(gcCookie, "can't create gc")) {
		return false;
	}

	mask = XCB_CW_CURSOR;
	uint32_t value_list = cursor;
	_xcb->xcb_change_window_attributes(_connection, window, mask, &value_list);
	_xcb->xcb_free_cursor(_connection, cursor);

	fontCookie = _xcb->xcb_close_font_checked(_connection, font);
	if (!checkCookie(fontCookie, "can't close font")) {
		return false;
	}*/
	return true;
}

Status XcbConnection::readFromClipboard(Rc<ClipboardRequest> &&req) {
	auto owner = _clipboard.owner;
	if (!owner) {
		auto reply = perform(_xcb->xcb_get_selection_owner_reply,
				_xcb->xcb_get_selection_owner(getConnection(), getAtom(XcbAtomIndex::CLIPBOARD)));
		if (reply) {
			owner = reply->owner;
		}
	}

	if (!owner) {
		// there is no clipboard
		req->dataCallback(BytesView(), StringView());
		return Status::Declined;
	}

	if (owner == _clipboard.window) {
		Vector<StringView> views;
		views.reserve(_clipboard.data->types.size());
		for (auto &it : _clipboard.data->types) { views.emplace_back(it); }
		auto type = req->typeCallback(views);
		if (type.empty() || std::find(views.begin(), views.end(), type) == views.end()) {
			req->dataCallback(BytesView(), StringView());
		} else {
			auto data = _clipboard.data->encodeCallback(type);
			req->dataCallback(data, type);
		}
	} else {
		if (_clipboard.requests.empty() && _clipboard.waiters.empty()) {
			// acquire list of formats
			_xcb->xcb_convert_selection(getConnection(), _clipboard.window,
					getAtom(XcbAtomIndex::CLIPBOARD), getAtom(XcbAtomIndex::TARGETS),
					getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD), XCB_CURRENT_TIME);
			_xcb->xcb_flush(getConnection());
		}

		_clipboard.requests.emplace_back(sp::move(req));
	}
	return Status::Ok;
}

Status XcbConnection::writeToClipboard(Rc<ClipboardData> &&data) {
	Vector<xcb_atom_t> atoms{
		getAtom(XcbAtomIndex::TARGETS),
		getAtom(XcbAtomIndex::TIMESTAMP),
		getAtom(XcbAtomIndex::MULTIPLE),
		getAtom(XcbAtomIndex::SAVE_TARGETS),
	};

	getAtoms(data->types, [&](SpanView<xcb_atom_t> a) {
		for (auto &it : a) { atoms.emplace_back(it); }
	});

	if (std::find(data->types.begin(), data->types.end(), "text/plain") != data->types.end()) {
		atoms.emplace_back(getAtom(XcbAtomIndex::UTF8_STRING));
		atoms.emplace_back(getAtom(XcbAtomIndex::TEXT));
		atoms.emplace_back(XCB_ATOM_STRING);
	}

	_clipboard.data = move(data);
	_clipboard.typeAtoms = sp::move(atoms);

	_xcb->xcb_set_selection_owner(getConnection(), _clipboard.window,
			getAtom(XcbAtomIndex::CLIPBOARD), XCB_CURRENT_TIME);

	auto cookie = _xcb->xcb_get_selection_owner(getConnection(), getAtom(XcbAtomIndex::CLIPBOARD));
	auto reply = _xcb->xcb_get_selection_owner_reply(getConnection(), cookie, nullptr);
	if (reply->owner != _clipboard.window) {
		_clipboard.data = nullptr;
		_clipboard.typeAtoms.clear();
	}
	::free(reply);

	return Status::Ok;
}

Value XcbConnection::getSettingsValue(StringView key) const {
	auto it = _xsettings.settings.find(key);
	if (it != _xsettings.settings.end()) {
		return it->second.value;
	}
	return Value();
}

void XcbConnection::printError(StringView message, xcb_generic_error_t *error) const {
	if (error) {
		log::error("XcbConnection", message, "; code=", error->error_code,
				"; major=", getErrorMajorName(error->major_code),
				"; minor=", getErrorMinorName(error->major_code, error->minor_code),
				"; name=", getErrorName(error->error_code));
	} else {
		log::error("XcbConnection", message, "; no error reported");
	}
}

void XcbConnection::updateKeysymMapping() {
	static auto look_for = [](uint16_t &mask, xcb_keycode_t *codes, xcb_keycode_t kc, int i) {
		if (mask == 0 && codes) {
			for (xcb_keycode_t *ktest = codes; *ktest; ktest++) {
				if (*ktest == kc) {
					mask = (uint16_t)(1 << i);
					break;
				}
			}
		}
	};

	if (_keys.keysyms) {
		_xcb->xcb_key_symbols_free(_keys.keysyms);
	}

	if (_xcb->hasKeysyms()) {
		_keys.keysyms = _xcb->xcb_key_symbols_alloc(_connection);
	}

	if (!_keys.keysyms) {
		return;
	}

	auto modifierCookie = _xcb->xcb_get_modifier_mapping_unchecked(_connection);

	xcb_get_keyboard_mapping_cookie_t mappingCookie;
	const xcb_setup_t *setup = _xcb->xcb_get_setup(_connection);

	if (!_xkb.lib) {
		mappingCookie = _xcb->xcb_get_keyboard_mapping(_connection, setup->min_keycode,
				setup->max_keycode - setup->min_keycode + 1);
	}

	auto numlockcodes = Ptr(_xcb->xcb_key_symbols_get_keycode(_keys.keysyms, XK_Num_Lock));
	auto shiftlockcodes = Ptr(_xcb->xcb_key_symbols_get_keycode(_keys.keysyms, XK_Shift_Lock));
	auto capslockcodes = Ptr(_xcb->xcb_key_symbols_get_keycode(_keys.keysyms, XK_Caps_Lock));
	auto modeswitchcodes = Ptr(_xcb->xcb_key_symbols_get_keycode(_keys.keysyms, XK_Mode_switch));

	auto modmap_r = perform(_xcb->xcb_get_modifier_mapping_reply, modifierCookie);
	if (!modmap_r) {
		return;
	}

	xcb_keycode_t *modmap = _xcb->xcb_get_modifier_mapping_keycodes(modmap_r);

	_keys.numlock = 0;
	_keys.shiftlock = 0;
	_keys.capslock = 0;
	_keys.modeswitch = 0;

	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < modmap_r->keycodes_per_modifier; j++) {
			xcb_keycode_t kc = modmap[i * modmap_r->keycodes_per_modifier + j];
			look_for(_keys.numlock, numlockcodes, kc, i);
			look_for(_keys.shiftlock, shiftlockcodes, kc, i);
			look_for(_keys.capslock, capslockcodes, kc, i);
			look_for(_keys.modeswitch, modeswitchcodes, kc, i);
		}
	}

	// only if no xkb available
	if (!_xkb.lib) {
		memset(_xkb.keycodes, 0, sizeof(core::InputKeyCode) * 256);
		// from https://stackoverflow.com/questions/18689863/obtain-keyboard-layout-and-keysyms-with-xcb
		auto keyboard_mapping = perform(_xcb->xcb_get_keyboard_mapping_reply, mappingCookie);

		int nkeycodes = keyboard_mapping->length / keyboard_mapping->keysyms_per_keycode;

		xcb_keysym_t *keysyms = (xcb_keysym_t *)(keyboard_mapping + 1);

		for (int keycode_idx = 0; keycode_idx < nkeycodes; ++keycode_idx) {
			_xkb.keycodes[setup->min_keycode + keycode_idx] =
					getKeysymCode(keysyms[keycode_idx * keyboard_mapping->keysyms_per_keycode]);
		}
	}
}

bool XcbConnection::checkCookie(xcb_void_cookie_t cookie, StringView errMessage) {
	auto error = Ptr(_xcb->xcb_request_check(_connection, cookie));
	if (error) {
		printError(errMessage, error);
		return false;
	}
	return true;
}

void XcbConnection::continueClipboardProcessing() {
	if (!_clipboard.waiters.empty()) {
		auto firstType = _clipboard.waiters.begin()->first;
		_xcb->xcb_convert_selection(getConnection(), _clipboard.window,
				getAtom(XcbAtomIndex::CLIPBOARD), firstType,
				getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD), XCB_CURRENT_TIME);
		_xcb->xcb_flush(getConnection());
	} else if (!_clipboard.requests.empty()) {
		_xcb->xcb_convert_selection(getConnection(), _clipboard.window,
				getAtom(XcbAtomIndex::CLIPBOARD), getAtom(XcbAtomIndex::TARGETS),
				getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD), XCB_CURRENT_TIME);
		_xcb->xcb_flush(getConnection());
	}
}

void XcbConnection::finalizeClipboardWaiters(BytesView data, xcb_atom_t type) {
	auto wIt = _clipboard.waiters.equal_range(type);
	auto typeName = getAtomName(type);
	if (typeName == "STRING" || typeName == "UTF8_STRING" || typeName == "TEXT") {
		typeName = StringView("text/plain");
	}
	for (auto it = wIt.first; it != wIt.second; ++it) { it->second->dataCallback(data, typeName); }

	_clipboard.waiters.erase(wIt.first, wIt.second);
}

void XcbConnection::handleSelectionNotify(xcb_selection_notify_event_t *event) {
	if (event->property == getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD)) {
		if (event->target == getAtom(XcbAtomIndex::TARGETS)) {
			auto cookie = _xcb->xcb_get_property_unchecked(getConnection(), 1, _clipboard.window,
					getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD), XCB_ATOM_ATOM, 0,
					maxOf<uint32_t>() / 4);
			auto reply = perform(_xcb->xcb_get_property_reply, cookie);
			if (reply) {
				auto targets = (xcb_atom_t *)_xcb->xcb_get_property_value(reply);
				auto len = _xcb->xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);

				// resolve required types
				getAtomNames(SpanView(targets, len), [&](SpanView<StringView> types) {
					// Hide system types from user
					Vector<StringView> safeTypes;
					for (auto &it : types) {
						if (it == "UTF8_STRING" || it == "STRING") {
							safeTypes.emplace_back("text/plain");
						} else if (it != "TARGETS" && it != "MULTIPLE" && it != "SAVE_TARGETS"
								&& it != "TIMESTAMP" && it != "COMPOUND_TEXT") {
							safeTypes.emplace_back(it);
						}
					}
					for (auto &it : _clipboard.requests) {
						auto type = it->typeCallback(safeTypes);
						// check if type is in list of available types
						if (!type.empty()
								&& std::find(types.begin(), types.end(), type) != types.end()) {
							if (type == "text/plain") {
								auto tIt = std::find(types.begin(), types.end(), "UTF8_STRING");
								if (tIt != types.end()) {
									_clipboard.waiters.emplace(getAtom(XcbAtomIndex::UTF8_STRING),
											sp::move(it));
								} else {
									tIt = std::find(types.begin(), types.end(), "STRING");
									if (tIt != types.end()) {
										_clipboard.waiters.emplace(XCB_ATOM_STRING, sp::move(it));
									} else {
										_clipboard.waiters.emplace(getAtom(type), sp::move(it));
									}
								}
							} else {
								_clipboard.waiters.emplace(getAtom(type), sp::move(it));
							}
						} else {
							// notify that we have no matched type
							it->dataCallback(BytesView(), StringView());
						}
					}
				});

				_clipboard.requests.clear();
			}
		} else {
			auto wIt = _clipboard.waiters.equal_range(event->target);

			if (wIt.first != wIt.second) {
				auto cookie = _xcb->xcb_get_property_unchecked(getConnection(), 1,
						_clipboard.window, getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD),
						XCB_GET_PROPERTY_TYPE_ANY, 0, maxOf<uint32_t>() / 4);
				auto reply = perform(_xcb->xcb_get_property_reply, cookie);
				if (reply) {
					if (reply->type == getAtom(XcbAtomIndex::INCR)) {
						// wait for an incremental content
						_clipboard.incr = true;
						_clipboard.incrType = event->target;
						_clipboard.incrBuffer.clear();
						return;
					} else {
						finalizeClipboardWaiters(
								BytesView((const uint8_t *)_xcb->xcb_get_property_value(reply),
										_xcb->xcb_get_property_value_length(reply)),
								_clipboard.incrType);
					}
				} else {
					_clipboard.waiters.erase(wIt.first, wIt.second);
				}
			} else {
				log::error("XcbConnection", "No requests waits for a ", getAtomName(event->target),
						" clipboard target");

				// remove property for a type
				auto cookie = _xcb->xcb_get_property_unchecked(getConnection(), 1,
						_clipboard.window, getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD),
						XCB_GET_PROPERTY_TYPE_ANY, 0, 0);
				auto reply = perform(_xcb->xcb_get_property_reply, cookie);
				if (reply) {
					reply.clear();
				}
			}
		}
	}
	continueClipboardProcessing();
}

void XcbConnection::handleSelectionClear(xcb_selection_clear_event_t *ev) {
	if (ev->owner == _clipboard.window && ev->selection == getAtom(XcbAtomIndex::CLIPBOARD)) {
		_clipboard.data = nullptr;
		_clipboard.typeAtoms.clear();
	}
}

void XcbConnection::handlePropertyNotify(xcb_property_notify_event_t *ev) {
	if (ev->window == _clipboard.window && ev->atom == getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD)
			&& ev->state == XCB_PROPERTY_NEW_VALUE && _clipboard.incr) {
		auto cookie = _xcb->xcb_get_property_unchecked(getConnection(), 1, _clipboard.window,
				getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD), XCB_GET_PROPERTY_TYPE_ANY, 0,
				maxOf<uint32_t>());
		auto reply = perform(_xcb->xcb_get_property_reply, cookie);
		if (reply) {
			auto len = _xcb->xcb_get_property_value_length(reply);

			if (len > 0) {
				auto data = (const uint8_t *)_xcb->xcb_get_property_value(reply);
				_clipboard.incrBuffer.emplace_back(BytesView(data, len).bytes<Interface>());
			} else {
				Bytes data;
				size_t size = 0;
				for (auto &it : _clipboard.incrBuffer) { size += it.size(); }

				data.resize(size);

				size = 0;
				for (auto &it : _clipboard.incrBuffer) {
					memcpy(data.data() + size, it.data(), it.size());
					size += it.size();
				}

				finalizeClipboardWaiters(data, _clipboard.incrType);
				_clipboard.incrBuffer.clear();
				_clipboard.incr = false;
				// finalize transfer
			}
		} else {
			finalizeClipboardWaiters(BytesView(), _clipboard.incrType);
			_clipboard.incrBuffer.clear();
			_clipboard.incr = false;
		}
	} else if (auto t = _clipboard.getTransfer(ev->window, ev->atom)) {
		if (ev->state == XCB_PROPERTY_DELETE) {
			if (t->chunks.empty()) {
				// write zero-length prop to end transfer
				_xcb->xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, t->requestor,
						t->property, t->type, 8, 0, nullptr);

				const uint32_t values[] = {XCB_EVENT_MASK_NO_EVENT};
				_xcb->xcb_change_window_attributes(_connection, t->requestor, XCB_CW_EVENT_MASK,
						values);

				_xcb->xcb_flush(_connection);
				_clipboard.cancelTransfer(ev->window, ev->atom);
			} else {
				auto &chunk = t->chunks.front();
				_xcb->xcb_change_property(getConnection(), XCB_PROP_MODE_APPEND, t->requestor,
						t->property, t->type, 8, chunk.size(), chunk.data());
				_xcb->xcb_flush(_connection);
				t->chunks.pop_front();
				++t->current;
			}
		}
	} else if (ev->window == _xsettings.window && ev->atom == _xsettings.property) {
		readXSettings();
	} else {
		auto it = _windows.find(ev->window);
		if (it != _windows.end()) {
			it->second->handlePropertyNotify(ev);
		}
	}
}

xcb_atom_t XcbConnection::writeClipboardSelection(xcb_window_t requestor, xcb_atom_t target,
		xcb_atom_t targetProperty) {
	if (!_clipboard.data) {
		return XCB_ATOM_NONE;
	}

	StringView type;
	if (target == XCB_ATOM_STRING || target == getAtom(XcbAtomIndex::UTF8_STRING)) {
		type = StringView("text/plain");
	} else {
		type = getAtomName(target);
	}

	auto it = std::find(_clipboard.data->types.begin(), _clipboard.data->types.end(), type);
	if (it == _clipboard.data->types.end()) {
		return XCB_ATOM_NONE;
	}

	auto data = _clipboard.data->encodeCallback(type);
	if (data.empty()) {
		return XCB_ATOM_NONE;
	}

	if (data.size() > _safeReqeustSize) {
		// start incr transfer

		auto t = _clipboard.addTransfer(requestor, targetProperty,
				ClipboardTransfer{
					requestor,
					targetProperty,
					target,
					_clipboard.data,
					0,
				});

		if (!t) {
			return XCB_ATOM_NONE;
		}

		uint32_t dataSize = uint32_t(data.size());
		BytesView dataView(data);

		while (!dataView.empty()) {
			auto block = dataView.readBytes(_safeReqeustSize);
			t->chunks.emplace_back(block.bytes<Interface>());
		}

		// subscribe on target widnow's events
		const uint32_t values[] = {
			XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE};
		_xcb->xcb_change_window_attributes(_connection, requestor, XCB_CW_EVENT_MASK, values);

		auto incr = getAtom(XcbAtomIndex::INCR);
		_xcb->xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, requestor, targetProperty,
				incr, 32, 1, &dataSize);
		return target;
	} else {
		_xcb->xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, requestor, targetProperty,
				target, 8, data.size(), data.data());
		return target;
	}
}

void XcbConnection::handleSelectionRequest(xcb_selection_request_event_t *event) {
	xcb_selection_notify_event_t notify;
	notify.response_type = XCB_SELECTION_NOTIFY;
	notify.pad0 = 0;
	notify.sequence = 0;
	notify.time = event->time;
	notify.requestor = event->requestor;
	notify.selection = event->selection;
	notify.target = event->target;
	notify.property = XCB_ATOM_NONE;

	if (event->target == getAtom(XcbAtomIndex::TARGETS)) {
		// The list of supported targets was requested
		_xcb->xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, event->requestor,
				event->property, XCB_ATOM_ATOM, 32, _clipboard.typeAtoms.size(),
				(unsigned char *)_clipboard.typeAtoms.data());
		notify.property = event->property;
	} else if (event->target == getAtom(XcbAtomIndex::TIMESTAMP)) {
		// The list of supported targets was requested
		_xcb->xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, event->requestor,
				event->property, XCB_ATOM_INTEGER, 32, 1,
				(unsigned char *)&_clipboard.selectionTimestamp);
		notify.property = event->property;
	} else if (event->target == getAtom(XcbAtomIndex::MULTIPLE)) {
		auto cookie = _xcb->xcb_get_property(getConnection(), 0, event->requestor, event->property,
				getAtom(XcbAtomIndex::ATOM_PAIR), 0, maxOf<uint32_t>() / 4);
		auto reply = perform(_xcb->xcb_get_property_reply, cookie);
		if (reply) {
			xcb_atom_t *requests = (xcb_atom_t *)_xcb->xcb_get_property_value(reply);
			auto count = uint32_t(_xcb->xcb_get_property_value_length(reply)) / sizeof(xcb_atom_t);

			for (uint32_t i = 0; i < count; i += 2) {
				auto it = std::find(_clipboard.typeAtoms.begin(), _clipboard.typeAtoms.end(),
						requests[i]);
				if (it == _clipboard.typeAtoms.end()) {
					requests[i + 1] = XCB_ATOM_NONE;
				} else {
					requests[i + 1] =
							writeClipboardSelection(event->requestor, requests[i], requests[i + 1]);
				}
			}

			_xcb->xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, event->requestor,
					event->property, getAtom(XcbAtomIndex::ATOM_PAIR), 32, count, requests);

			notify.property = event->property;
		}
	} else if (event->target == getAtom(XcbAtomIndex::SAVE_TARGETS)) {
		_xcb->xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, event->requestor,
				event->property, getAtom(XcbAtomIndex::XNULL), 32, 0, nullptr);
		notify.property = event->property;
	} else {
		auto it =
				std::find(_clipboard.typeAtoms.begin(), _clipboard.typeAtoms.end(), event->target);

		if (it != _clipboard.typeAtoms.end()) {
			notify.target =
					writeClipboardSelection(event->requestor, event->target, event->property);
			if (notify.target != XCB_ATOM_NONE) {
				notify.property = event->property;
			}
		}
	}

	_xcb->xcb_send_event(getConnection(), false, event->requestor,
			XCB_EVENT_MASK_NO_EVENT, // SelectionNotify events go without mask
			(const char *)&notify);
	_xcb->xcb_flush(getConnection());
}

void XcbConnection::handleSelectionUpdateNotify(xcb_xfixes_selection_notify_event_t *ev) {
	if (ev->selection != getAtom(XcbAtomIndex::CLIPBOARD)) {
		return;
	}

	_clipboard.owner = ev->owner;

	if (ev->owner == _clipboard.window) {
		_clipboard.selectionTimestamp = ev->selection_timestamp;
	} else if (ev->owner == XCB_WINDOW_NONE) {
		_clipboard.data = nullptr;
		_clipboard.typeAtoms.clear();
	}
}

static int _getPadding(int length, int increment) {
	return (increment - (length % increment)) % increment;
}

void XcbConnection::readXSettings() {
	auto cookie = _xcb->xcb_get_property(getConnection(), 0, _xsettings.window, _xsettings.property,
			0, 0, maxOf<uint32_t>() / 4);
	auto reply = perform(_xcb->xcb_get_property_reply, cookie);
	if (reply) {
		auto data = _xcb->xcb_get_property_value(reply);
		auto len = _xcb->xcb_get_property_value_length(reply);

		Map<String, SettingsValue> settings;

		uint32_t udpi = 0;
		uint32_t dpi = 0;

		auto d = BytesView(reinterpret_cast<uint8_t *>(data), len);
		/*auto byteOrder =*/d.readUnsigned32();
		auto serial = d.readUnsigned32();
		auto nsettings = d.readUnsigned32();

		while (nsettings > 0 && !d.empty()) {
			auto type = d.readUnsigned(); //                  1  SETTING_TYPE  type
			d.readUnsigned(); //                                       1                unused
			auto len = d.readUnsigned16(); //                2  n             name-len
			auto name = d.readString(len); // n  STRING8       name
			d.readBytes(_getPadding(len, 4)); //   P               unused, p=pad(n)
			auto serial = d.readUnsigned32(); //             4   CARD32      last-change-serial

			if (type == 0) {
				auto value = d.readUnsigned32();
				settings.emplace(name.str<Interface>(),
						SettingsValue{Value(bit_cast<int32_t>(value)), serial});
				if (name == "Gdk/UnscaledDPI") {
					udpi = value;
				} else if (name == "Xft/DPI") {
					dpi = value;
				}
			} else if (type == 1) {
				auto len = d.readUnsigned32();
				auto value = d.readString(len);
				d.readBytes(_getPadding(len, 4));
				settings.emplace(name.str<Interface>(), SettingsValue{Value(value), serial});
			} else if (type == 2) {
				auto r = d.readUnsigned16();
				auto g = d.readUnsigned16();
				auto b = d.readUnsigned16();
				auto a = d.readUnsigned16();

				settings.emplace(name.str<Interface>(),
						SettingsValue{Value{Value(r), Value(g), Value(b), Value(a)}, serial});
			} else {
				break;
			}
			--nsettings;
		}

		_xsettings.serial = serial;
		_xsettings.settings = sp::move(settings);
		_xsettings.udpi = udpi;
		_xsettings.dpi = dpi;

		for (auto &it : _windows) { it.second->handleSettingsUpdated(); }
	}
}

} // namespace stappler::xenolith::platform
