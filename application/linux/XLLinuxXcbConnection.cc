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
#include "XLLinuxXcbWindow.h"
#include <X11/keysym.h>

#ifndef XL_X11_DEBUG
#if XL_X11_DEBUG
#define XL_X11_LOG(...) log::debug("XCB", __VA_ARGS__)
#else
#define XL_X11_LOG(...)
#endif
#endif

// use GLFW mappings as a fallback for XKB
uint32_t _glfwKeySym2Unicode(unsigned int keysym);

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

void ScreenInfo::describe(const CallbackStream &out) {
	out << "ScreenInfo(" << width << "x" << height << "; " << mwidth << "x" << mheight
		<< "; rates:";
	for (auto &it : rates) { out << " " << it; }
	out << ");";
}

void ModeInfo::describe(const CallbackStream &out) {
	out << "ModeInfo(" << id << ":'" << name << "': " << width << "x" << height << "@" << rate
		<< ");";
}

void CrtcInfo::describe(const CallbackStream &out, uint32_t indent) {
	for (uint32_t i = 0; i < indent; ++i) { out << '\t'; }
	out << "CrtcInfo(" << crtc << "; x:" << x << "; y:" << y << "; w:" << width << "; h:" << height
		<< "; m:" << mode << "; r:" << rotation << "; rs:" << rotations << ");\n";

	for (uint32_t i = 0; i < indent + 1; ++i) { out << '\t'; }
	out << "outputs:";
	for (auto &it : outputs) { out << " " << it << ";"; }
	out << "\n";

	for (uint32_t i = 0; i < indent + 1; ++i) { out << '\t'; }
	out << "possible:";
	for (auto &it : possible) { out << " " << it << ";"; }
	out << "\n";
}

void OutputInfo::describe(const CallbackStream &out) {
	out << "OutputInfo(" << output << ":'" << name << "': crtc:" << crtc << "; modes:";

	for (auto &it : modes) { out << " " << it << ";"; }
	out << ");";
}

void ScreenInfoData::describe(const CallbackStream &out) {
	out << "ScreenInfoData: " << config << "\n";
	out << "\tcrtcs:";
	for (auto &it : currentCrtcs) { out << " " << it << ";"; }
	out << "\n";

	out << "\toutput:";
	for (auto &it : currentOutputs) { out << " " << it << ";"; }
	out << "\n";

	out << "\tcurrentModeInfo:\n";
	for (auto &it : currentModeInfo) {
		out << "\t\t";
		it.describe(out);
		out << "\n";
	}

	out << "\tmodeInfo:\n";
	for (auto &it : modeInfo) {
		out << "\t\t";
		it.describe(out);
		out << "\n";
	}

	out << "\tscreenInfo:\n";
	for (auto &it : screenInfo) {
		out << "\t\t";
		it.describe(out);
		out << "\n";
	}

	out << "\tcrtcInfo:\n";
	for (auto &it : crtcInfo) { it.describe(out, 2); }

	out << "\tprimaryOutput:\n";
	out << "\t\t";
	primaryOutput.describe(out);
	out << "\n";

	out << "\tprimaryCrtc:\n";
	primaryCrtc.describe(out, 2);

	out << "\tprimaryMode:\n";
	out << "\t\t";
	primaryMode.describe(out);
	out << "\n";
}

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
	if (_cursorContext) {
		_xcb->xcb_cursor_context_free(_cursorContext);
		_cursorContext = nullptr;
	}
	if (_xkbKeymap) {
		_xkb->xkb_keymap_unref(_xkbKeymap);
		_xkbKeymap = nullptr;
	}
	if (_xkbState) {
		_xkb->xkb_state_unref(_xkbState);
		_xkbState = nullptr;
	}
	if (_xkbCompose) {
		_xkb->xkb_compose_state_unref(_xkbCompose);
		_xkbCompose = nullptr;
	}
	if (_keysyms) {
		_xcb->xcb_key_symbols_free(_keysyms);
		_keysyms = nullptr;
	}
	if (_connection) {
		_xcb->xcb_disconnect(_connection);
		_connection = nullptr;
	}
}

XcbConnection::XcbConnection(NotNull<XcbLibrary> xcb, NotNull<XkbLibrary> xkb, StringView display) {
	_xcb = xcb;
	_xkb = xkb;

	_connection = _xcb->xcb_connect(display.empty()
					? NULL
					: (display.terminated() ? display.data() : display.str<Interface>().data()),
			&_screenNbr); // always not null
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

	if (_xcb->hasRandr()) {
		auto ext = _xcb->xcb_get_extension_data(_connection, _xcb->xcb_randr_id);
		if (ext) {
			_randrEnabled = true;
			_randrFirstEvent = ext->first_event;
		}
	}

	if (_xcb->hasSync()) {
		auto ext = _xcb->xcb_get_extension_data(_connection, _xcb->xcb_sync_id);
		if (ext) {
			_syncEnabled = true;
		}
	}

	if (_xkb && _xkb->hasX11() && _xcb->hasXkb()) {
		initXkb();
	}

	// read atoms from connection
	xcb_intern_atom_cookie_t atomCookies[sizeof(s_atomRequests) / sizeof(XcbAtomInfo)];

	size_t i = 0;
	for (auto &it : s_atomRequests) {
		atomCookies[i] = _xcb->xcb_intern_atom(_connection, it.onlyIfExists ? 1 : 0, it.name.size(),
				it.name.data());
		++i;
	}

	_xcb->xcb_flush(_connection);

	if (_xcb->xcb_cursor_context_new(_connection, _screen, &_cursorContext) < 0) {
		log::warn("XcbConnection", "Fail to load cursor context");
		_cursorContext = nullptr;
	}

	memcpy(_atoms, s_atomRequests, sizeof(s_atomRequests));

	i = 0;
	for (auto &it : atomCookies) {
		auto reply = _xcb->xcb_intern_atom_reply(_connection, it, nullptr);
		if (reply) {
			_atoms[i].value = reply->atom;
			free(reply);
		} else {
			_atoms[i].value = 0;
		}
		++i;
	}
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

void XcbConnection::poll() {
	xcb_generic_event_t *e;

	Set<XcbWindow *> eventWindows;

	while ((e = _xcb->xcb_poll_for_event(_connection))) {
		auto et = e->response_type & 0x7f;
		switch (et) {
		case XCB_EXPOSE: XL_X11_LOG("XCB_EXPOSE"); break;
		case XCB_PROPERTY_NOTIFY: XL_X11_LOG("XCB_PROPERTY_NOTIFY"); break;
		case XCB_VISIBILITY_NOTIFY: XL_X11_LOG("XCB_VISIBILITY_NOTIFY"); break;
		case XCB_MAP_NOTIFY: XL_X11_LOG("XCB_MAP_NOTIFY"); break;
		case XCB_REPARENT_NOTIFY: XL_X11_LOG("XCB_REPARENT_NOTIFY"); break;
		case XCB_COLORMAP_NOTIFY: XL_X11_LOG("XCB_COLORMAP_NOTIFY"); break;
		case XCB_CONFIGURE_REQUEST: XL_X11_LOG("XCB_CONFIGURE_REQUEST"); break;
		case XCB_RESIZE_REQUEST: XL_X11_LOG("XCB_RESIZE_REQUEST"); break;

		case XCB_SELECTION_NOTIFY:
			XcbConnection_forwardToWindow("XCB_SELECTION_NOTIFY", _windows,
					((xcb_selection_notify_event_t *)e)->requestor,
					(xcb_selection_notify_event_t *)e, &XcbWindow::handleSelectionNotify);
			break;
		case XCB_SELECTION_REQUEST:
			XcbConnection_forwardToWindow("XCB_SELECTION_REQUEST", _windows,
					((xcb_selection_request_event_t *)e)->owner, (xcb_selection_request_event_t *)e,
					&XcbWindow::handleSelectionRequest);
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
			if (_keysyms) {
				_xcb->xcb_refresh_keyboard_mapping(_keysyms, ev);
			}
			XL_X11_LOG("XCB_MAPPING_NOTIFY: ", (int)ev->request, " ", (int)ev->first_keycode, " ",
					(int)ev->count);
			break;
		}
		default:
			if (et == _xkbFirstEvent) {
				switch (e->pad0) {
				case XCB_XKB_NEW_KEYBOARD_NOTIFY: initXkb(); break;
				case XCB_XKB_MAP_NOTIFY: updateXkbMapping(); break;
				case XCB_XKB_STATE_NOTIFY: {
					xcb_xkb_state_notify_event_t *ev = (xcb_xkb_state_notify_event_t *)e;
					_xkb->xkb_state_update_mask(_xkbState, ev->baseMods, ev->latchedMods,
							ev->lockedMods, ev->baseGroup, ev->latchedGroup, ev->lockedGroup);
					break;
				}
				}
			} else if (et == _randrFirstEvent) {
				switch (e->pad0) {
				case XCB_RANDR_SCREEN_CHANGE_NOTIFY:
					XcbConnection_forwardToWindow("XCB_RANDR_SCREEN_CHANGE_NOTIFY", _windows,
							((xcb_randr_screen_change_notify_event_t *)e)->request_window,
							(xcb_randr_screen_change_notify_event_t *)e,
							&XcbWindow::handleScreenChangeNotify, &eventWindows);
					break;
				default: break;
				}
			} else {
				/* Unknown event type, ignore it */
				XL_X11_LOG("Unknown event: ", et);
			}
			break;
		}

		/* Free the Generic Event */
		free(e);
	}

	for (auto &it : eventWindows) { it->dispatchPendingEvents(); }
}

bool XcbConnection::hasErrors() const {
	auto err = _xcb->xcb_connection_has_error(_connection);
	if (err != 0) {
		ReportError(err);
		return true;
	}
	return false;
}

core::InputKeyCode XcbConnection::getKeyCode(xcb_keycode_t code) const { return _keycodes[code]; }

xcb_atom_t XcbConnection::getAtom(XcbAtomIndex index) const { return _atoms[toInt(index)].value; }

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
			winfo->rect.x, winfo->rect.y, winfo->rect.width, winfo->rect.height,
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

	uint32_t nProtocols = 0;
	xcb_atom_t protocolAtoms[2];

	if (xinfo.overrideClose && _atoms[toInt(XcbAtomIndex::WM_DELETE_WINDOW)].value) {
		protocolAtoms[nProtocols++] = _atoms[toInt(XcbAtomIndex::WM_DELETE_WINDOW)].value;
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

ScreenInfoData XcbConnection::getScreenInfo(xcb_screen_t *screen) const {
	return getScreenInfo(screen->root);
}

ScreenInfoData XcbConnection::getScreenInfo(xcb_window_t root) const {
	if (!_xcb->hasRandr()) {
		return ScreenInfoData();
	}

	// submit our version to X11
	auto versionCookie = _xcb->xcb_randr_query_version(_connection, XcbLibrary::RANDR_MAJOR_VERSION,
			XcbLibrary::RANDR_MINOR_VERSION);
	if (auto versionReply =
					_xcb->xcb_randr_query_version_reply(_connection, versionCookie, nullptr)) {
		if (versionReply->major_version != XcbLibrary::RANDR_MAJOR_VERSION) {
			::free(versionReply);
			return ScreenInfoData();
		}

		::free(versionReply);
	} else {
		return ScreenInfoData();
	}

	ScreenInfoData ret;

	// spawn requests
	auto screenResCurrentCookie =
			_xcb->xcb_randr_get_screen_resources_current_unchecked(_connection, root);
	auto outputPrimaryCookie = _xcb->xcb_randr_get_output_primary_unchecked(_connection, root);
	auto screenResCookie = _xcb->xcb_randr_get_screen_resources_unchecked(_connection, root);
	auto screenInfoCookie = _xcb->xcb_randr_get_screen_info_unchecked(_connection, root);
	xcb_randr_get_output_info_cookie_t outputInfoCookie;

	Vector<Pair<xcb_randr_crtc_t, xcb_randr_get_crtc_info_cookie_t>> crtcCookies;

	do {
		// process current modes
		auto curReply = _xcb->xcb_randr_get_screen_resources_current_reply(_connection,
				screenResCurrentCookie, nullptr);
		auto curModes = _xcb->xcb_randr_get_screen_resources_current_modes(curReply);
		auto curNmodes = _xcb->xcb_randr_get_screen_resources_current_modes_length(curReply);
		uint8_t *names = _xcb->xcb_randr_get_screen_resources_current_names(curReply);

		while (curNmodes > 0) {
			double vTotal = curModes->vtotal;

			if (curModes->mode_flags & XCB_RANDR_MODE_FLAG_DOUBLE_SCAN) {
				/* doublescan doubles the number of lines */
				vTotal *= 2;
			}

			if (curModes->mode_flags & XCB_RANDR_MODE_FLAG_INTERLACE) {
				/* interlace splits the frame into two fields */
				/* the field rate is what is typically reported by monitors */
				vTotal /= 2;
			}

			if (curModes->htotal && vTotal) {
				auto rate = uint16_t(floor(
						double(curModes->dot_clock) / (double(curModes->htotal) * double(vTotal))));
				ret.currentModeInfo.emplace_back(ModeInfo{curModes->id, curModes->width,
					curModes->height, rate, String((const char *)names, curModes->name_len)});
			}

			names += curModes->name_len;
			++curModes;
			--curNmodes;
		}

		auto outputs = _xcb->xcb_randr_get_screen_resources_current_outputs(curReply);
		auto noutputs = _xcb->xcb_randr_get_screen_resources_current_outputs_length(curReply);

		while (noutputs > 0) {
			ret.currentOutputs.emplace_back(*outputs);
			++outputs;
			--noutputs;
		}

		ret.config = curReply->config_timestamp;

		auto crtcs = _xcb->xcb_randr_get_screen_resources_current_crtcs(curReply);
		auto ncrtcs = _xcb->xcb_randr_get_screen_resources_current_crtcs_length(curReply);

		crtcCookies.reserve(ncrtcs);

		while (ncrtcs > 0) {
			ret.currentCrtcs.emplace_back(*crtcs);

			crtcCookies.emplace_back(*crtcs,
					_xcb->xcb_randr_get_crtc_info_unchecked(_connection, *crtcs, ret.config));

			++crtcs;
			--ncrtcs;
		}

		::free(curReply);
	} while (0);

	do {
		auto reply =
				_xcb->xcb_randr_get_output_primary_reply(_connection, outputPrimaryCookie, nullptr);
		ret.primaryOutput.output = reply->output;
		::free(reply);

		outputInfoCookie = _xcb->xcb_randr_get_output_info_unchecked(_connection,
				ret.primaryOutput.output, ret.config);
	} while (0);

	// process screen info
	do {
		auto reply = _xcb->xcb_randr_get_screen_info_reply(_connection, screenInfoCookie, nullptr);
		auto sizes = size_t(_xcb->xcb_randr_get_screen_info_sizes_length(reply));

		Vector<Vector<uint16_t>> ratesVec;
		Vector<uint16_t> tmp;

		auto ratesIt = _xcb->xcb_randr_get_screen_info_rates_iterator(reply);
		while (ratesIt.rem > 0) {
			auto nRates = _xcb->xcb_randr_refresh_rates_rates_length(ratesIt.data);
			auto rates = _xcb->xcb_randr_refresh_rates_rates(ratesIt.data);
			auto tmpNRates = nRates;

			while (tmpNRates) {
				tmp.emplace_back(*rates);
				++rates;
				--tmpNRates;
			}

			_xcb->xcb_randr_refresh_rates_next(&ratesIt);
			ratesIt.rem += 1 - nRates; // bypass rem bug

			ratesVec.emplace_back(sp::move(tmp));
			tmp.clear();
		}

		auto sizesData = _xcb->xcb_randr_get_screen_info_sizes(reply);
		for (size_t i = 0; i < sizes; ++i) {
			auto &it = sizesData[i];
			ScreenInfo info{it.width, it.height, it.mwidth, it.mheight};

			if (ratesVec.size() > i) {
				info.rates = ratesVec[i];
			} else if (ratesVec.size() == 1) {
				info.rates = ratesVec[0];
			} else {
				info.rates = Vector<uint16_t>{60};
			}

			ret.screenInfo.emplace_back(move(info));
		}

		::free(reply);
	} while (0);

	do {
		auto modesReply =
				_xcb->xcb_randr_get_screen_resources_reply(_connection, screenResCookie, nullptr);
		auto modes = _xcb->xcb_randr_get_screen_resources_modes(modesReply);
		auto nmodes = _xcb->xcb_randr_get_screen_resources_modes_length(modesReply);

		while (nmodes > 0) {
			double vTotal = modes->vtotal;

			if (modes->mode_flags & XCB_RANDR_MODE_FLAG_DOUBLE_SCAN) {
				/* doublescan doubles the number of lines */
				vTotal *= 2;
			}

			if (modes->mode_flags & XCB_RANDR_MODE_FLAG_INTERLACE) {
				/* interlace splits the frame into two fields */
				/* the field rate is what is typically reported by monitors */
				vTotal /= 2;
			}

			if (modes->htotal && vTotal) {
				auto rate = uint16_t(
						floor(double(modes->dot_clock) / (double(modes->htotal) * double(vTotal))));
				ret.modeInfo.emplace_back(ModeInfo{modes->id, modes->width, modes->height, rate});
			}

			++modes;
			--nmodes;
		}

		::free(modesReply);
	} while (0);

	do {
		auto reply = _xcb->xcb_randr_get_output_info_reply(_connection, outputInfoCookie, nullptr);
		auto modes = _xcb->xcb_randr_get_output_info_modes(reply);
		auto nmodes = _xcb->xcb_randr_get_output_info_modes_length(reply);

		while (nmodes > 0) {
			ret.primaryOutput.modes.emplace_back(*modes);

			++modes;
			--nmodes;
		}

		auto name = _xcb->xcb_randr_get_output_info_name(reply);
		auto nameLen = _xcb->xcb_randr_get_output_info_name_length(reply);

		ret.primaryOutput.crtc = reply->crtc;
		ret.primaryOutput.name = String((const char *)name, nameLen);

		::free(reply);
	} while (0);

	for (auto &crtcCookie : crtcCookies) {
		auto reply = _xcb->xcb_randr_get_crtc_info_reply(_connection, crtcCookie.second, nullptr);

		Vector<xcb_randr_output_t> outputs;
		Vector<xcb_randr_output_t> possible;

		auto outputsPtr = _xcb->xcb_randr_get_crtc_info_outputs(reply);
		auto noutputs = _xcb->xcb_randr_get_crtc_info_outputs_length(reply);

		outputs.reserve(noutputs);

		while (noutputs) {
			outputs.emplace_back(*outputsPtr);
			++outputsPtr;
			--noutputs;
		}

		auto possiblePtr = _xcb->xcb_randr_get_crtc_info_possible(reply);
		auto npossible = _xcb->xcb_randr_get_crtc_info_possible_length(reply);

		possible.reserve(npossible);

		while (npossible) {
			possible.emplace_back(*possiblePtr);
			++possiblePtr;
			--npossible;
		}

		ret.crtcInfo.emplace_back(CrtcInfo{crtcCookie.first, reply->x, reply->y, reply->width,
			reply->height, reply->mode, reply->rotation, reply->rotations, sp::move(outputs),
			sp::move(possible)});

		::free(reply);
	}

	for (auto &it : ret.crtcInfo) {
		if (it.crtc == ret.primaryOutput.crtc) {
			ret.primaryCrtc = it;

			for (auto &iit : ret.currentModeInfo) {
				if (iit.id == ret.primaryCrtc.mode) {
					ret.primaryMode = iit;
					break;
				}
			}

			break;
		}
	}

	return ret;
}

void XcbConnection::fillTextInputData(core::InputEventData &event, xcb_keycode_t detail,
		uint16_t state, bool textInputEnabled, bool compose) {
	if (_xkb) {
		event.key.keycode = getKeyCode(detail);
		event.key.compose = core::InputKeyComposeState::Nothing;
		event.key.keysym = getKeysym(detail, state, false);
		if (textInputEnabled) {
			if (compose) {
				const auto keysym = composeSymbol(
						_xkb->xkb_state_key_get_one_sym(_xkbState, detail), event.key.compose);
				const uint32_t cp = _xkb->xkb_keysym_to_utf32(keysym);
				if (cp != 0 && keysym != XKB_KEY_NoSymbol) {
					event.key.keychar = cp;
				} else {
					event.key.keychar = 0;
				}
			} else {
				event.key.keychar = _xkb->xkb_state_key_get_utf32(_xkbState, detail);
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

xcb_keysym_t XcbConnection::getKeysym(xcb_keycode_t code, uint16_t state, bool resolveMods) const {
	xcb_keysym_t k0, k1;

	if (!resolveMods) {
		k0 = _xcb->xcb_key_symbols_get_keysym(_keysyms, code, 0);
		// resolve only numlock
		if ((state & _numlock)) {
			k1 = _xcb->xcb_key_symbols_get_keysym(_keysyms, code, 1);
			if (_xcb->xcb_is_keypad_key(k1)) {
				if ((state & XCB_MOD_MASK_SHIFT)
						|| ((state & XCB_MOD_MASK_LOCK) && (state & _shiftlock))) {
					return k0;
				} else {
					return k1;
				}
			}
		}
		return k0;
	}

	if (state & _modeswitch) {
		k0 = _xcb->xcb_key_symbols_get_keysym(_keysyms, code, 2);
		k1 = _xcb->xcb_key_symbols_get_keysym(_keysyms, code, 3);
	} else {
		k0 = _xcb->xcb_key_symbols_get_keysym(_keysyms, code, 0);
		k1 = _xcb->xcb_key_symbols_get_keysym(_keysyms, code, 1);
	}

	if (k1 == XCB_NO_SYMBOL) {
		k1 = k0;
	}

	if ((state & _numlock) && _xcb->xcb_is_keypad_key(k1)) {
		if ((state & XCB_MOD_MASK_SHIFT) || ((state & XCB_MOD_MASK_LOCK) && (state & _shiftlock))) {
			return k0;
		} else {
			return k1;
		}
	} else if (!(state & XCB_MOD_MASK_SHIFT) && !(state & XCB_MOD_MASK_LOCK)) {
		return k0;
	} else if (!(state & XCB_MOD_MASK_SHIFT)
			&& ((state & XCB_MOD_MASK_LOCK) && (state & _capslock))) {
		if (k0 >= XK_0 && k0 <= XK_9) {
			return k0;
		}
		return k1;
	} else if ((state & XCB_MOD_MASK_SHIFT)
			&& ((state & XCB_MOD_MASK_LOCK) && (state & _capslock))) {
		return k1;
	} else if ((state & XCB_MOD_MASK_SHIFT)
			|| ((state & XCB_MOD_MASK_LOCK) && (state & _shiftlock))) {
		return k1;
	}

	return XCB_NO_SYMBOL;
}

xkb_keysym_t XcbConnection::composeSymbol(xkb_keysym_t sym,
		core::InputKeyComposeState &compose) const {
	if (sym == XKB_KEY_NoSymbol || !_xkbCompose) {
		XL_X11_LOG("Compose: ", sym, " (disabled)");
		return sym;
	}
	if (_xkb->xkb_compose_state_feed(_xkbCompose, sym) != XKB_COMPOSE_FEED_ACCEPTED) {
		XL_X11_LOG("Compose: ", sym, " (not accepted)");
		return sym;
	}
	auto composedSym = sym;
	auto state = _xkb->xkb_compose_state_get_status(_xkbCompose);
	switch (state) {
	case XKB_COMPOSE_COMPOSED:
		compose = core::InputKeyComposeState::Composed;
		composedSym = _xkb->xkb_compose_state_get_one_sym(_xkbCompose);
		_xkb->xkb_compose_state_reset(_xkbCompose);
		XL_X11_LOG("Compose: ", sym, ": ", composedSym, " (composed)");
		break;
	case XKB_COMPOSE_COMPOSING:
		compose = core::InputKeyComposeState::Composing;
		XL_X11_LOG("Compose: ", sym, ": ", composedSym, " (composing)");
		break;
	case XKB_COMPOSE_CANCELLED:
		_xkb->xkb_compose_state_reset(_xkbCompose);
		XL_X11_LOG("Compose: ", sym, ": ", composedSym, " (cancelled)");
		break;
	case XKB_COMPOSE_NOTHING:
		_xkb->xkb_compose_state_reset(_xkbCompose);
		XL_X11_LOG("Compose: ", sym, ": ", composedSym, " (nothing)");
		break;
	default: XL_X11_LOG("Compose: ", sym, ": ", composedSym, " (error)"); break;
	}
	return composedSym;
}

xcb_cursor_t XcbConnection::loadCursor(StringView str) {
	return _xcb->xcb_cursor_load_cursor(_cursorContext,
			str.terminated() ? str.data() : str.str<Interface>().data());
}

xcb_cursor_t XcbConnection::loadCursor(std::initializer_list<StringView> list) {
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

void XcbConnection::initXkb() {
	uint16_t xkbMajorVersion = 0;
	uint16_t xkbMinorVersion = 0;

	if (!_xkbSetup) {
		if (_xkb->xkb_x11_setup_xkb_extension(_connection, XKB_X11_MIN_MAJOR_XKB_VERSION,
					XKB_X11_MIN_MINOR_XKB_VERSION, XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS,
					&xkbMajorVersion, &xkbMinorVersion, &_xkbFirstEvent, &_xkbFirstError)
				!= 1) {
			return;
		}
	}

	_xkbSetup = true;
	_xkbDeviceId = _xkb->xkb_x11_get_core_keyboard_device_id(_connection);

	enum {
		required_events = (XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY
				| XCB_XKB_EVENT_TYPE_STATE_NOTIFY),

		required_nkn_details = (XCB_XKB_NKN_DETAIL_KEYCODES),

		required_map_parts = (XCB_XKB_MAP_PART_KEY_TYPES | XCB_XKB_MAP_PART_KEY_SYMS
				| XCB_XKB_MAP_PART_MODIFIER_MAP | XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS
				| XCB_XKB_MAP_PART_KEY_ACTIONS | XCB_XKB_MAP_PART_VIRTUAL_MODS
				| XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP),

		required_state_details =
				(XCB_XKB_STATE_PART_MODIFIER_BASE | XCB_XKB_STATE_PART_MODIFIER_LATCH
						| XCB_XKB_STATE_PART_MODIFIER_LOCK | XCB_XKB_STATE_PART_GROUP_BASE
						| XCB_XKB_STATE_PART_GROUP_LATCH | XCB_XKB_STATE_PART_GROUP_LOCK),
	};

	static const xcb_xkb_select_events_details_t details = {.affectNewKeyboard =
																	required_nkn_details,
		.newKeyboardDetails = required_nkn_details,
		.affectState = required_state_details,
		.stateDetails = required_state_details};

	_xcb->xcb_xkb_select_events(_connection, _xkbDeviceId, required_events, 0, required_events,
			required_map_parts, required_map_parts, &details);

	updateXkbMapping();
}

void XcbConnection::updateXkbMapping() {
	if (_xkbState) {
		_xkb->xkb_state_unref(_xkbState);
		_xkbState = nullptr;
	}
	if (_xkbKeymap) {
		_xkb->xkb_keymap_unref(_xkbKeymap);
		_xkbKeymap = nullptr;
	}
	if (_xkbCompose) {
		_xkb->xkb_compose_state_unref(_xkbCompose);
		_xkbCompose = nullptr;
	}

	_xkbKeymap = _xkb->xkb_x11_keymap_new_from_device(_xkb->getContext(), _connection, _xkbDeviceId,
			XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (_xkbKeymap == nullptr) {
		fprintf(stderr, "Failed to get Keymap for current keyboard device.\n");
		return;
	}

	_xkbState = _xkb->xkb_x11_state_new_from_device(_xkbKeymap, _connection, _xkbDeviceId);
	if (_xkbState == nullptr) {
		fprintf(stderr, "Failed to get state object for current keyboard device.\n");
		return;
	}

	memset(_keycodes, 0, sizeof(core::InputKeyCode) * 256);

	_xkb->xkb_keymap_key_for_each(_xkbKeymap,
			[](struct xkb_keymap *keymap, xkb_keycode_t key, void *data) {
		((XcbConnection *)data)->updateXkbKey(key);
	}, this);

	auto locale = getenv("LC_ALL");
	if (!locale) {
		locale = getenv("LC_CTYPE");
	}
	if (!locale) {
		locale = getenv("LANG");
	}

	auto composeTable = _xkb->xkb_compose_table_new_from_locale(_xkb->getContext(),
			locale ? locale : "C", XKB_COMPOSE_COMPILE_NO_FLAGS);
	if (composeTable) {
		_xkbCompose = _xkb->xkb_compose_state_new(composeTable, XKB_COMPOSE_STATE_NO_FLAGS);
		_xkb->xkb_compose_table_unref(composeTable);
	}
}

void XcbConnection::updateXkbKey(xcb_keycode_t code) {
	static const struct {
		core::InputKeyCode key;
		const char *name;
	} keymap[] = {{core::InputKeyCode::GRAVE_ACCENT, "TLDE"}, {core::InputKeyCode::_1, "AE01"},
		{core::InputKeyCode::_2, "AE02"}, {core::InputKeyCode::_3, "AE03"},
		{core::InputKeyCode::_4, "AE04"}, {core::InputKeyCode::_5, "AE05"},
		{core::InputKeyCode::_6, "AE06"}, {core::InputKeyCode::_7, "AE07"},
		{core::InputKeyCode::_8, "AE08"}, {core::InputKeyCode::_9, "AE09"},
		{core::InputKeyCode::_0, "AE10"}, {core::InputKeyCode::MINUS, "AE11"},
		{core::InputKeyCode::EQUAL, "AE12"}, {core::InputKeyCode::Q, "AD01"},
		{core::InputKeyCode::W, "AD02"}, {core::InputKeyCode::E, "AD03"},
		{core::InputKeyCode::R, "AD04"}, {core::InputKeyCode::T, "AD05"},
		{core::InputKeyCode::Y, "AD06"}, {core::InputKeyCode::U, "AD07"},
		{core::InputKeyCode::I, "AD08"}, {core::InputKeyCode::O, "AD09"},
		{core::InputKeyCode::P, "AD10"}, {core::InputKeyCode::LEFT_BRACKET, "AD11"},
		{core::InputKeyCode::RIGHT_BRACKET, "AD12"}, {core::InputKeyCode::A, "AC01"},
		{core::InputKeyCode::S, "AC02"}, {core::InputKeyCode::D, "AC03"},
		{core::InputKeyCode::F, "AC04"}, {core::InputKeyCode::G, "AC05"},
		{core::InputKeyCode::H, "AC06"}, {core::InputKeyCode::J, "AC07"},
		{core::InputKeyCode::K, "AC08"}, {core::InputKeyCode::L, "AC09"},
		{core::InputKeyCode::SEMICOLON, "AC10"}, {core::InputKeyCode::APOSTROPHE, "AC11"},
		{core::InputKeyCode::Z, "AB01"}, {core::InputKeyCode::X, "AB02"},
		{core::InputKeyCode::C, "AB03"}, {core::InputKeyCode::V, "AB04"},
		{core::InputKeyCode::B, "AB05"}, {core::InputKeyCode::N, "AB06"},
		{core::InputKeyCode::M, "AB07"}, {core::InputKeyCode::COMMA, "AB08"},
		{core::InputKeyCode::PERIOD, "AB09"}, {core::InputKeyCode::SLASH, "AB10"},
		{core::InputKeyCode::BACKSLASH, "BKSL"}, {core::InputKeyCode::WORLD_1, "LSGT"},
		{core::InputKeyCode::SPACE, "SPCE"}, {core::InputKeyCode::ESCAPE, "ESC"},
		{core::InputKeyCode::ENTER, "RTRN"}, {core::InputKeyCode::TAB, "TAB"},
		{core::InputKeyCode::BACKSPACE, "BKSP"}, {core::InputKeyCode::INSERT, "INS"},
		{core::InputKeyCode::DELETE, "DELE"}, {core::InputKeyCode::RIGHT, "RGHT"},
		{core::InputKeyCode::LEFT, "LEFT"}, {core::InputKeyCode::DOWN, "DOWN"},
		{core::InputKeyCode::UP, "UP"}, {core::InputKeyCode::PAGE_UP, "PGUP"},
		{core::InputKeyCode::PAGE_DOWN, "PGDN"}, {core::InputKeyCode::HOME, "HOME"},
		{core::InputKeyCode::END, "END"}, {core::InputKeyCode::CAPS_LOCK, "CAPS"},
		{core::InputKeyCode::SCROLL_LOCK, "SCLK"}, {core::InputKeyCode::NUM_LOCK, "NMLK"},
		{core::InputKeyCode::PRINT_SCREEN, "PRSC"}, {core::InputKeyCode::PAUSE, "PAUS"},
		{core::InputKeyCode::F1, "FK01"}, {core::InputKeyCode::F2, "FK02"},
		{core::InputKeyCode::F3, "FK03"}, {core::InputKeyCode::F4, "FK04"},
		{core::InputKeyCode::F5, "FK05"}, {core::InputKeyCode::F6, "FK06"},
		{core::InputKeyCode::F7, "FK07"}, {core::InputKeyCode::F8, "FK08"},
		{core::InputKeyCode::F9, "FK09"}, {core::InputKeyCode::F10, "FK10"},
		{core::InputKeyCode::F11, "FK11"}, {core::InputKeyCode::F12, "FK12"},
		{core::InputKeyCode::F13, "FK13"}, {core::InputKeyCode::F14, "FK14"},
		{core::InputKeyCode::F15, "FK15"}, {core::InputKeyCode::F16, "FK16"},
		{core::InputKeyCode::F17, "FK17"}, {core::InputKeyCode::F18, "FK18"},
		{core::InputKeyCode::F19, "FK19"}, {core::InputKeyCode::F20, "FK20"},
		{core::InputKeyCode::F21, "FK21"}, {core::InputKeyCode::F22, "FK22"},
		{core::InputKeyCode::F23, "FK23"}, {core::InputKeyCode::F24, "FK24"},
		{core::InputKeyCode::F25, "FK25"}, {core::InputKeyCode::KP_0, "KP0"},
		{core::InputKeyCode::KP_1, "KP1"}, {core::InputKeyCode::KP_2, "KP2"},
		{core::InputKeyCode::KP_3, "KP3"}, {core::InputKeyCode::KP_4, "KP4"},
		{core::InputKeyCode::KP_5, "KP5"}, {core::InputKeyCode::KP_6, "KP6"},
		{core::InputKeyCode::KP_7, "KP7"}, {core::InputKeyCode::KP_8, "KP8"},
		{core::InputKeyCode::KP_9, "KP9"}, {core::InputKeyCode::KP_DECIMAL, "KPDL"},
		{core::InputKeyCode::KP_DIVIDE, "KPDV"}, {core::InputKeyCode::KP_MULTIPLY, "KPMU"},
		{core::InputKeyCode::KP_SUBTRACT, "KPSU"}, {core::InputKeyCode::KP_ADD, "KPAD"},
		{core::InputKeyCode::KP_ENTER, "KPEN"}, {core::InputKeyCode::KP_EQUAL, "KPEQ"},
		{core::InputKeyCode::LEFT_SHIFT, "LFSH"}, {core::InputKeyCode::LEFT_CONTROL, "LCTL"},
		{core::InputKeyCode::LEFT_ALT, "LALT"}, {core::InputKeyCode::LEFT_SUPER, "LWIN"},
		{core::InputKeyCode::RIGHT_SHIFT, "RTSH"}, {core::InputKeyCode::RIGHT_CONTROL, "RCTL"},
		{core::InputKeyCode::RIGHT_ALT, "RALT"}, {core::InputKeyCode::RIGHT_ALT, "LVL3"},
		{core::InputKeyCode::RIGHT_ALT, "MDSW"}, {core::InputKeyCode::RIGHT_SUPER, "RWIN"},
		{core::InputKeyCode::MENU, "MENU"}};

	core::InputKeyCode key = core::InputKeyCode::Unknown;
	if (auto name = _xkb->xkb_keymap_key_get_name(_xkbKeymap, code)) {
		for (size_t i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++) {
			if (strncmp(name, keymap[i].name, 4) == 0) {
				key = keymap[i].key;
				break;
			}
		}
	}

	if (key != core::InputKeyCode::Unknown) {
		_keycodes[code] = key;
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

	if (_keysyms) {
		_xcb->xcb_key_symbols_free(_keysyms);
	}

	if (_xcb->hasKeysyms()) {
		_keysyms = _xcb->xcb_key_symbols_alloc(_connection);
	}

	if (!_keysyms) {
		return;
	}

	auto modifierCookie = _xcb->xcb_get_modifier_mapping_unchecked(_connection);

	xcb_get_keyboard_mapping_cookie_t mappingCookie;
	const xcb_setup_t *setup = _xcb->xcb_get_setup(_connection);

	if (!_xkb) {
		mappingCookie = _xcb->xcb_get_keyboard_mapping(_connection, setup->min_keycode,
				setup->max_keycode - setup->min_keycode + 1);
	}

	auto numlockcodes = _xcb->xcb_key_symbols_get_keycode(_keysyms, XK_Num_Lock);
	auto shiftlockcodes = _xcb->xcb_key_symbols_get_keycode(_keysyms, XK_Shift_Lock);
	auto capslockcodes = _xcb->xcb_key_symbols_get_keycode(_keysyms, XK_Caps_Lock);
	auto modeswitchcodes = _xcb->xcb_key_symbols_get_keycode(_keysyms, XK_Mode_switch);

	auto modmap_r = _xcb->xcb_get_modifier_mapping_reply(_connection, modifierCookie, nullptr);
	if (!modmap_r) {
		return;
	}

	xcb_keycode_t *modmap = _xcb->xcb_get_modifier_mapping_keycodes(modmap_r);

	_numlock = 0;
	_shiftlock = 0;
	_capslock = 0;
	_modeswitch = 0;

	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < modmap_r->keycodes_per_modifier; j++) {
			xcb_keycode_t kc = modmap[i * modmap_r->keycodes_per_modifier + j];
			look_for(_numlock, numlockcodes, kc, i);
			look_for(_shiftlock, shiftlockcodes, kc, i);
			look_for(_capslock, capslockcodes, kc, i);
			look_for(_modeswitch, modeswitchcodes, kc, i);
		}
	}

	::free(modmap_r);

	::free(numlockcodes);
	::free(shiftlockcodes);
	::free(capslockcodes);
	::free(modeswitchcodes);

	// only if no xkb available
	if (!_xkb) {
		memset(_keycodes, 0, sizeof(core::InputKeyCode) * 256);
		// from https://stackoverflow.com/questions/18689863/obtain-keyboard-layout-and-keysyms-with-xcb
		xcb_get_keyboard_mapping_reply_t *keyboard_mapping =
				_xcb->xcb_get_keyboard_mapping_reply(_connection, mappingCookie, NULL);

		int nkeycodes = keyboard_mapping->length / keyboard_mapping->keysyms_per_keycode;

		xcb_keysym_t *keysyms = (xcb_keysym_t *)(keyboard_mapping + 1);

		for (int keycode_idx = 0; keycode_idx < nkeycodes; ++keycode_idx) {
			_keycodes[setup->min_keycode + keycode_idx] =
					getKeysymCode(keysyms[keycode_idx * keyboard_mapping->keysyms_per_keycode]);
		}

		free(keyboard_mapping);
	}
}

bool XcbConnection::checkCookie(xcb_void_cookie_t cookie, StringView errMessage) {
	xcb_generic_error_t *error = _xcb->xcb_request_check(_connection, cookie);
	if (error) {
		log::error("XcbConnection", errMessage, "; code=", error->error_code);
		return false;
	}
	return true;
}

} // namespace stappler::xenolith::platform
