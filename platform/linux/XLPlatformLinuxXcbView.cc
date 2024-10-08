/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XLPlatformLinuxXcbView.h"
#include <X11/keysym.h>

uint32_t _glfwKeySym2Unicode(unsigned int keysym);

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

#if XL_X11_DEBUG
#define XL_X11_LOG(...) log::debug("Wayland", __VA_ARGS__)
#else
#define XL_X11_LOG(...)
#endif

void XcbView::ReportError(int error) {
	switch (error) {
	case XCB_CONN_ERROR:
		stappler::log::error("XcbView", "XCB_CONN_ERROR: socket error, pipe error or other stream error");
		break;
	case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
		stappler::log::error("XcbView", "XCB_CONN_CLOSED_EXT_NOTSUPPORTED: extension is not supported");
		break;
	case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
		stappler::log::error("XcbView", "XCB_CONN_CLOSED_MEM_INSUFFICIENT: out of memory");
		break;
	case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
		stappler::log::error("XcbView", "XCB_CONN_CLOSED_REQ_LEN_EXCEED: too large request");
		break;
	case XCB_CONN_CLOSED_PARSE_ERR:
		stappler::log::error("XcbView", "XCB_CONN_CLOSED_PARSE_ERR: error during parsing display string");
		break;
	case XCB_CONN_CLOSED_INVALID_SCREEN:
		stappler::log::error("XcbView", "XCB_CONN_CLOSED_INVALID_SCREEN: server does not have a screen matching the display");
		break;
	case XCB_CONN_CLOSED_FDPASSING_FAILED:
		stappler::log::error("XcbView", "XCB_CONN_CLOSED_FDPASSING_FAILED: fail to pass some FD");
		break;
	}
}

XcbView::XcbView(XcbLibrary *lib, ViewInterface *view, StringView title, StringView bundleId, URect rect) {
	_xcb = lib;
	_xkb = XkbLibrary::getInstance();
	_view = view;
#if DEBUG
	auto d = getenv("DISPLAY");
	if (!d) {
		stappler::log::warn("XcbView-Info", "DISPLAY is not defined");
	}
#endif

	auto connection = lib->acquireConnection();

	_connection = connection.connection;

	auto err = _xcb->xcb_connection_has_error(_connection);
	if (err != 0) {
		ReportError(err);
		return;
	}

	_defaultScreen = connection.screen;

	if (_xcb->hasRandr()) {
		auto ext = _xcb->xcb_get_extension_data(_connection, _xcb->xcb_randr_id);

		_randrEnabled = true;
		_randrFirstEvent = ext->first_event;

		_screenInfo = getScreenInfo();
		_rate = _screenInfo.primaryMode.rate;
	}

	if (_xkb && _xkb->hasX11() && _xcb->hasXkb()) {
		initXkb();
	}

	_socket = _xcb->xcb_get_file_descriptor(_connection); // assume it's non-blocking

	uint32_t mask = /*XCB_CW_BACK_PIXEL | */ XCB_CW_EVENT_MASK;
	uint32_t values[1];
	//values[0] = _defaultScreen->white_pixel;
	values[0] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |	XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION
			| XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE
			| XCB_EVENT_MASK_VISIBILITY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY
			| XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_COLOR_MAP_CHANGE
			| XCB_EVENT_MASK_OWNER_GRAB_BUTTON;

	/* Ask for our window's Id */
	_window = _xcb->xcb_generate_id(_connection);

	_width = rect.width;
	_height = rect.height;

	_xcb->xcb_create_window(_connection,
		XCB_COPY_FROM_PARENT, // depth (same as root)
		_window, // window Id
		_defaultScreen->root, // parent window
		rect.x, rect.y, rect.width, rect.height,
		0, // border_width
		XCB_WINDOW_CLASS_INPUT_OUTPUT, // class
		_defaultScreen->root_visual, // visual
		mask, values);

	xcb_intern_atom_cookie_t atomCookies[sizeof(s_atomRequests) / sizeof(XcbAtomRequest)];

	size_t i = 0;
	for (auto &it : s_atomRequests) {
		atomCookies[i] = _xcb->xcb_intern_atom(_connection, it.onlyIfExists ? 1 : 0, it.name.size(), it.name.data());
		++i;
	}

	_xcb->xcb_flush(_connection);

	i = 0;
	for (auto &it : atomCookies) {
		auto reply = _xcb->xcb_intern_atom_reply(_connection, it, nullptr);
		if (reply) {
			_atoms[i] = reply->atom;
			free(reply);
		} else {
			_atoms[i] = 0;
		}
		++i;
	}

	_wmClass.resize(title.size() + bundleId.size() + 1, char(0));
	memcpy(_wmClass.data(), title.data(), title.size());
	memcpy(_wmClass.data() + title.size() + 1, bundleId.data(), bundleId.size());

	_xcb->xcb_change_property( _connection, XCB_PROP_MODE_REPLACE, _window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, title.size(), title.data());
	_xcb->xcb_change_property( _connection, XCB_PROP_MODE_REPLACE, _window, XCB_ATOM_WM_ICON_NAME, XCB_ATOM_STRING, 8, title.size(), title.data());
	if (_atoms[0]) {
		_xcb->xcb_change_property( _connection, XCB_PROP_MODE_REPLACE, _window, _atoms[0], 4, 32, 1, &_atoms[1] );
	}
	_xcb->xcb_change_property( _connection, XCB_PROP_MODE_REPLACE, _window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, _wmClass.size(), _wmClass.data());

	updateKeysymMapping();

	_xcb->xcb_flush(_connection);
}

XcbView::~XcbView() {
	_defaultScreen = nullptr;
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

bool XcbView::valid() const {
	return _xcb->xcb_connection_has_error(_connection) == 0;
}

static core::InputModifier getModifiers(uint32_t mask) {
	core::InputModifier ret = core::InputModifier::None;
	core::InputModifier *mod, mods[] = { core::InputModifier::Shift, core::InputModifier::CapsLock, core::InputModifier::Ctrl,
			core::InputModifier::Alt, core::InputModifier::NumLock, core::InputModifier::Mod3, core::InputModifier::Mod4,
			core::InputModifier::Mod5, core::InputModifier::Button1, core::InputModifier::Button2, core::InputModifier::Button3,
			core::InputModifier::Button4, core::InputModifier::Button5, core::InputModifier::LayoutAlternative };
	for (mod = mods; mask; mask >>= 1, mod++) {
		if (mask & 1) {
			ret |= *mod;
		}
	}
	return ret;
}

static core::InputMouseButton getButton(xcb_button_t btn) {
	return core::InputMouseButton(btn);
}

bool XcbView::poll(bool frameReady) {
	bool ret = true;
	bool deprecateSwapchain = false;

	Vector<core::InputEventData> inputEvents;

	auto dispatchEvents = [&, this] {
		if (!inputEvents.empty()) {
			_view->handleInputEvents(move(inputEvents));
		}
		inputEvents.clear();
	};

	xcb_timestamp_t lastInputTime = 0;
	xcb_generic_event_t *e;
	while ((e = _xcb->xcb_poll_for_event(_connection))) {
		auto et = e->response_type & 0x7f;
		switch (et) {
		case XCB_PROPERTY_NOTIFY: {
			/* Ignore */
			//xcb_property_notify_event_t *ev = (xcb_property_notify_event_t*) e;
			//printf("XCB_PROPERTY_NOTIFY: %d of property %d\n", ev->window, ev->atom);
			break;
		}
		case XCB_SELECTION_NOTIFY: {
			xcb_get_property_reply_t *reply;
			xcb_selection_notify_event_t *sel_event = (xcb_selection_notify_event_t*) e;

			/* Since we have only a single 'thing' we request, we do not
			 * have to inspect the values of the event.
			 */
			if (sel_event->property == _atoms[toInt(XcbAtomIndex::XENOLITH_CLIPBOARD)]) {
				reply = _xcb->xcb_get_property_reply(_connection,
						_xcb->xcb_get_property(_connection, 1, _window,
								_atoms[toInt(XcbAtomIndex::XENOLITH_CLIPBOARD)],
								_atoms[toInt(XcbAtomIndex::STRING)], 0, 300), NULL);
				notifyClipboard(BytesView((const uint8_t *)_xcb->xcb_get_property_value(reply), _xcb->xcb_get_property_value_length(reply)));
				free(reply);
			}
			break;
		}
		case XCB_SELECTION_REQUEST: {
			handleSelectionRequest((xcb_selection_request_event_t *) e);
			break;
		}
		case XCB_EXPOSE: {
			// xcb_expose_event_t *ev = (xcb_expose_event_t*) e;
			// printf("XCB_EXPOSE: Window %d exposed. Region to be redrawn at location (%d,%d), with dimension (%d,%d)\n",
			//		ev->window, ev->x, ev->y, ev->width, ev->height);
			break;
		}
		case XCB_BUTTON_PRESS:
			if (_window == ((xcb_button_press_event_t *)e)->event) {
				auto ev = (xcb_button_press_event_t *)e;
				if (lastInputTime != ev->time) {
					dispatchEvents();
					lastInputTime = ev->time;
				}

				auto ext = _view->getExtent();
				auto mod = getModifiers(ev->state);
				auto btn = getButton(ev->detail);

				core::InputEventData event({
					ev->detail,
					core::InputEventName::Begin,
					btn,
					mod,
					float(ev->event_x),
					float(ext.height - ev->event_y)
				});

				switch (btn) {
				case core::InputMouseButton::MouseScrollUp:
					event.event = core::InputEventName::Scroll;
					event.point.valueX = 0.0f; event.point.valueY = 10.0f;
					break;
				case core::InputMouseButton::MouseScrollDown:
					event.event = core::InputEventName::Scroll;
					event.point.valueX = 0.0f; event.point.valueY = -10.0f;
					break;
				case core::InputMouseButton::MouseScrollLeft:
					event.event = core::InputEventName::Scroll;
					event.point.valueX = 10.0f; event.point.valueY = 0.0f;
					break;
				case core::InputMouseButton::MouseScrollRight:
					event.event = core::InputEventName::Scroll;
					event.point.valueX = -10.0f; event.point.valueY = 0.0f;
					break;
				default:
					break;
				}

				inputEvents.emplace_back(event);
			}
			break;
		case XCB_BUTTON_RELEASE:
			if (_window == ((xcb_button_release_event_t *)e)->event) {
				auto ev = (xcb_button_release_event_t *)e;
				if (lastInputTime != ev->time) {
					dispatchEvents();
					lastInputTime = ev->time;
				}

				auto ext = _view->getExtent();
				auto mod = getModifiers(ev->state);
				auto btn = getButton(ev->detail);

				core::InputEventData event({
					ev->detail,
					core::InputEventName::End,
					btn,
					mod,
					float(ev->event_x),
					float(ext.height - ev->event_y)
				});

				switch (btn) {
				case core::InputMouseButton::MouseScrollUp:
				case core::InputMouseButton::MouseScrollDown:
				case core::InputMouseButton::MouseScrollLeft:
				case core::InputMouseButton::MouseScrollRight:
					break;
				default:
					inputEvents.emplace_back(event);
					break;
				}
			}
			break;
		case XCB_MOTION_NOTIFY:
			if (_window == ((xcb_motion_notify_event_t *)e)->event) {
				auto ev = (xcb_motion_notify_event_t *)e;
				if (lastInputTime != ev->time) {
					dispatchEvents();
					lastInputTime = ev->time;
				}

				auto ext = _view->getExtent();
				auto mod = getModifiers(ev->state);

				core::InputEventData event({
					maxOf<uint32_t>(),
					core::InputEventName::MouseMove,
					core::InputMouseButton::None,
					mod,
					float(ev->event_x),
					float(ext.height - ev->event_y)
				});

				inputEvents.emplace_back(event);
			}
			break;
		case XCB_ENTER_NOTIFY: {
			xcb_enter_notify_event_t *ev = (xcb_enter_notify_event_t*) e;
			if (lastInputTime != ev->time) {
				dispatchEvents();
				lastInputTime = ev->time;
			}

			auto ext = _view->getExtent();
			inputEvents.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::PointerEnter, true,
					Vec2(float(ev->event_x), float(ext.height - ev->event_y))));
			// printf("Mouse entered window %d, at coordinates (%d,%d)\n", ev->event, ev->event_x, ev->event_y);
			break;
		}
		case XCB_LEAVE_NOTIFY: {
			xcb_leave_notify_event_t *ev = (xcb_leave_notify_event_t*) e;
			if (lastInputTime != ev->time) {
				dispatchEvents();
				lastInputTime = ev->time;
			}

			auto ext = _view->getExtent();
			inputEvents.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::PointerEnter, false,
					Vec2(float(ev->event_x), float(ext.height - ev->event_y))));
			// printf("Mouse left window %d, at coordinates (%d,%d)\n", ev->event, ev->event_x, ev->event_y);
			break;
		}
		case XCB_FOCUS_IN: {
			// xcb_focus_in_event_t *ev = (xcb_focus_in_event_t*) e;
			inputEvents.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::FocusGain, true));
			updateKeysymMapping();
			break;
		}
		case XCB_FOCUS_OUT: {
			// xcb_focus_in_event_t *ev = (xcb_focus_in_event_t*) e;
			inputEvents.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::FocusGain, false));
			break;
		}
		case XCB_KEY_PRESS: {
			xcb_key_press_event_t *ev = (xcb_key_press_event_t*) e;
			if (lastInputTime != ev->time) {
				dispatchEvents();
				lastInputTime = ev->time;
			}

			auto mod = getModifiers(ev->state);
			auto ext = _view->getExtent();

			// in case of key autorepeat, ev->time will match
			// just replace event name from previous InputEventName::KeyReleased to InputEventName::KeyRepeated
			if (!inputEvents.empty() && inputEvents.back().event == core::InputEventName::KeyReleased) {
				auto &iev = inputEvents.back();
				if (iev.id == ev->time && iev.modifiers == mod && iev.x == float(ev->event_x)
						&& iev.y == float(ext.height - ev->event_y)
						&& iev.key.keysym == getKeysym(ev->detail, ev->state, false)) {
					iev.event = core::InputEventName::KeyRepeated;
					break;
				}
			}

			core::InputEventData event({
				ev->time,
				core::InputEventName::KeyPressed,
				core::InputMouseButton::None,
				mod,
				float(ev->event_x),
				float(ext.height - ev->event_y)
			});

			if (_xkb) {
				event.key.keycode = getKeyCode(ev->detail);
				event.key.compose = core::InputKeyComposeState::Nothing;
				event.key.keysym = getKeysym(ev->detail, ev->state, false);
				if (_view->isInputEnabled()) {
					const auto keysym = composeSymbol(_xkb->xkb_state_key_get_one_sym(_xkbState, ev->detail), event.key.compose);
					const uint32_t cp = _xkb->xkb_keysym_to_utf32(keysym);
					if (cp != 0 && keysym != XKB_KEY_NoSymbol) {
						event.key.keychar = cp;
					} else {
						event.key.keychar = 0;
					}
				} else {
					event.key.keychar = 0;
				}
			} else {
				auto sym = getKeysym(ev->detail, ev->state, false); // state-inpependent keysym
				event.key.keycode = getKeysymCode(sym);
				event.key.compose = core::InputKeyComposeState::Nothing;
				event.key.keysym = sym;
				if (_view->isInputEnabled()) {
					event.key.keychar = _glfwKeySym2Unicode(getKeysym(ev->detail, ev->state)); // use state-dependent keysym
				} else {
					event.key.keychar = 0;
				}
			}

			inputEvents.emplace_back(event);

			String str;
			unicode::utf8Encode(str, event.key.keychar);
			XL_X11_LOG("Key pressed in window ", ev->event, " (", (int)ev->time, ") ", event.key.keysym,
					" '", str, "' ", uint32_t(event.key.keychar));
			break;
		}
		case XCB_KEY_RELEASE: {
			xcb_key_release_event_t *ev = (xcb_key_release_event_t*) e;
			if (lastInputTime != ev->time) {
				dispatchEvents();
				lastInputTime = ev->time;
			}

			auto mod = getModifiers(ev->state);
			auto ext = _view->getExtent();

			core::InputEventData event({
				ev->time,
				core::InputEventName::KeyReleased,
				core::InputMouseButton::None,
				mod,
				float(ev->event_x),
				float(ext.height - ev->event_y)
			});

			if (_xkb) {
				event.key.keycode = getKeyCode(ev->detail);
				event.key.compose = core::InputKeyComposeState::Nothing;
				event.key.keysym = getKeysym(ev->detail, ev->state, false);
				if (_view->isInputEnabled()) {
					event.key.keychar = _xkb->xkb_state_key_get_utf32(_xkbState, ev->detail);
				} else {
					event.key.keychar = 0;
				}
			} else {
				auto sym = getKeysym(ev->detail, ev->state, false); // state-inpependent keysym
				event.key.keycode = getKeysymCode(sym);
				event.key.compose = core::InputKeyComposeState::Nothing;
				event.key.keysym = sym;
				if (_view->isInputEnabled()) {
					event.key.keychar = _glfwKeySym2Unicode(getKeysym(ev->detail, ev->state)); // use state-dependent keysym
				} else {
					event.key.keychar = 0;
				}
			}

			inputEvents.emplace_back(event);

			String str;
			unicode::utf8Encode(str, event.key.keychar);
			XL_X11_LOG("Key released in window ", ev->event, " (", (int)ev->time, ") ", event.key.keysym,
					" '", str, "' ", uint32_t(event.key.keychar));
			break;
		}
		case XCB_VISIBILITY_NOTIFY: {
			SPUNUSED xcb_visibility_notify_event_t *ev = (xcb_visibility_notify_event_t*) e;
			XL_X11_LOG("XCB_VISIBILITY_NOTIFY: ", ev->window);
			break;
		}
		case XCB_MAP_NOTIFY: {
			SPUNUSED xcb_map_notify_event_t *ev = (xcb_map_notify_event_t*) e;
			XL_X11_LOG("XCB_MAP_NOTIFY: ", ev->event);
			break;
		}
		case XCB_REPARENT_NOTIFY: {
			//xcb_reparent_notify_event_t *ev = (xcb_reparent_notify_event_t*) e;
			//XL_X11_LOG("XCB_REPARENT_NOTIFY: %d %d to %d\n", ev->event, ev->window, ev->parent);
			break;
		}
		case XCB_CONFIGURE_NOTIFY: {
			xcb_configure_notify_event_t *ev = (xcb_configure_notify_event_t*) e;
			// printf("XCB_CONFIGURE_NOTIFY: %d (%d) rect:%d,%d,%d,%d border:%d override:%d\n", ev->event, ev->window,
			//		ev->x, ev->y, ev->width, ev->height, uint32_t(ev->border_width), uint32_t(ev->override_redirect));
			if (ev->width != _width || ev->height != _height) {
				_width = ev->width;
				_height = ev->height;
				deprecateSwapchain = true;
			}
			break;
		}
		case XCB_CLIENT_MESSAGE: {
			xcb_client_message_event_t *ev = (xcb_client_message_event_t*) e;
			XL_X11_LOG("XCB_CLIENT_MESSAGE: ", ev->window, " of type ", ev->type);
			if (ev->type == _atoms[0] && ev->data.data32[0] == _atoms[1]) {
				ret = false;
			}
			break;
		}
		case XCB_MAPPING_NOTIFY: {
			xcb_mapping_notify_event_t *ev = (xcb_mapping_notify_event_t*) e;
			if (_keysyms) {
				_xcb->xcb_refresh_keyboard_mapping(_keysyms, ev);
			}
			XL_X11_LOG("XCB_MAPPING_NOTIFY: ", (int) ev->request, " ", (int) ev->first_keycode, " ", (int) ev->count);
			break;
		}
		case XCB_COLORMAP_NOTIFY: {
			XL_X11_LOG("XCB_COLORMAP_NOTIFY: ", (int) ev->request);
			printf("XCB_PROPERTY_NOTIFY\n");
			break;
		}
		default:
			if (et == _xkbFirstEvent) {
				switch (e->pad0) {
				case XCB_XKB_NEW_KEYBOARD_NOTIFY:
					initXkb();
					break;
				case XCB_XKB_MAP_NOTIFY:
					updateXkbMapping();
					break;
				case XCB_XKB_STATE_NOTIFY: {
					xcb_xkb_state_notify_event_t *ev = (xcb_xkb_state_notify_event_t*)e;
					_xkb->xkb_state_update_mask(_xkbState, ev->baseMods, ev->latchedMods, ev->lockedMods,
							ev->baseGroup, ev->latchedGroup, ev->lockedGroup);
					break;
				}
				}
			} else if (et == _randrFirstEvent) {
				switch (e->pad0) {
				case XCB_RANDR_SCREEN_CHANGE_NOTIFY:
					_screenInfo = getScreenInfo();
					break;
				default:
					break;
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

	dispatchEvents();

	if (deprecateSwapchain) {
		_view->deprecateSwapchain();
	}

	return ret;
}

uint64_t XcbView::getScreenFrameInterval() const {
	return 1'000'000 / _rate;
}

void XcbView::mapWindow() {
	_xcb->xcb_map_window(_connection, _window);
	_xcb->xcb_flush(_connection);
}

void XcbView::readFromClipboard(Function<void(BytesView, StringView)> &&cb, Ref *ref) {
	auto cookie = _xcb->xcb_get_selection_owner(_connection, _atoms[toInt(XcbAtomIndex::CLIPBOARD)]);
	auto reply = _xcb->xcb_get_selection_owner_reply(_connection, cookie, nullptr);

	if (reply->owner == _window) {
		cb(_clipboardSelection, StringView("text/plain"));
	} else {
		_xcb->xcb_convert_selection(_connection, _window,
	    		_atoms[toInt(XcbAtomIndex::CLIPBOARD)],
				_atoms[toInt(XcbAtomIndex::STRING)],
				_atoms[toInt(XcbAtomIndex::XENOLITH_CLIPBOARD)], XCB_CURRENT_TIME);
		_xcb->xcb_flush(_connection);

		if (_clipboardCallback) {
			_clipboardCallback(BytesView(), StringView());
		}

		_clipboardCallback = move(cb);
		_clipboardTarget = ref;
	}
}

void XcbView::writeToClipboard(BytesView data, StringView contentType) {
	_clipboardSelection = data.bytes<Interface>();

	_xcb->xcb_set_selection_owner(_connection, _window, _atoms[toInt(XcbAtomIndex::CLIPBOARD)], XCB_CURRENT_TIME);

	auto cookie = _xcb->xcb_get_selection_owner(_connection, _atoms[toInt(XcbAtomIndex::CLIPBOARD)]);
	auto reply = _xcb->xcb_get_selection_owner_reply(_connection, cookie, nullptr);
	if (reply->owner != _window) {
		log::error("XcbView", "Fail to set selection owner");
	}
	::free(reply);
}

XcbView::ScreenInfoData XcbView::getScreenInfo() const {
	if (!_xcb->hasRandr()) {
		return ScreenInfoData();
	}

	// submit our version to X11
	auto versionCookie = _xcb->xcb_randr_query_version( _connection, XcbLibrary::RANDR_MAJOR_VERSION, XcbLibrary::RANDR_MINOR_VERSION);
	if (auto versionReply = _xcb->xcb_randr_query_version_reply( _connection, versionCookie, nullptr)) {
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
	auto screenResCurrentCookie = _xcb->xcb_randr_get_screen_resources_current_unchecked(_connection, _defaultScreen->root);
	auto outputPrimaryCookie = _xcb->xcb_randr_get_output_primary_unchecked(_connection, _defaultScreen->root);
	auto screenResCookie = _xcb->xcb_randr_get_screen_resources_unchecked(_connection, _defaultScreen->root);
	auto screenInfoCookie = _xcb->xcb_randr_get_screen_info_unchecked(_connection, _defaultScreen->root);
	xcb_randr_get_output_info_cookie_t outputInfoCookie;

	Vector<Pair<xcb_randr_crtc_t, xcb_randr_get_crtc_info_cookie_t>> crtcCookies;

	do {
		// process current modes
		auto curReply = _xcb->xcb_randr_get_screen_resources_current_reply(_connection, screenResCurrentCookie, nullptr);
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
				auto rate = uint16_t(floor(double(curModes->dot_clock) / (double(curModes->htotal) * double(vTotal))));
				ret.currentModeInfo.emplace_back(ModeInfo{curModes->id, curModes->width, curModes->height, rate,
					String((const char *)names, curModes->name_len)});
			}

			names += curModes->name_len;
			++ curModes;
			-- curNmodes;
		}

		auto outputs = _xcb->xcb_randr_get_screen_resources_current_outputs(curReply);
		auto noutputs = _xcb->xcb_randr_get_screen_resources_current_outputs_length(curReply);

		while (noutputs > 0) {
			ret.currentOutputs.emplace_back(*outputs);
			++ outputs;
			-- noutputs;
		}

		ret.config = curReply->config_timestamp;

		auto crtcs = _xcb->xcb_randr_get_screen_resources_current_crtcs(curReply);
		auto ncrtcs = _xcb->xcb_randr_get_screen_resources_current_crtcs_length(curReply);

		crtcCookies.reserve(ncrtcs);

		while (ncrtcs > 0) {
			ret.currentCrtcs.emplace_back(*crtcs);

			crtcCookies.emplace_back(*crtcs, _xcb->xcb_randr_get_crtc_info_unchecked(_connection, *crtcs, ret.config));

			++ crtcs;
			-- ncrtcs;
		}

		::free(curReply);
	} while (0);

	do {
		auto reply = _xcb->xcb_randr_get_output_primary_reply(_connection, outputPrimaryCookie, nullptr);
		ret.primaryOutput.output = reply->output;
		::free(reply);

		outputInfoCookie = _xcb->xcb_randr_get_output_info_unchecked(_connection, ret.primaryOutput.output, ret.config);
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
				++ rates;
				-- tmpNRates;
			}

			_xcb->xcb_randr_refresh_rates_next(&ratesIt);
			ratesIt.rem += 1 - nRates; // bypass rem bug

			ratesVec.emplace_back(move(tmp));
			tmp.clear();
		}

		auto sizesData = _xcb->xcb_randr_get_screen_info_sizes(reply);
		for (size_t i = 0; i < sizes; ++ i) {
			auto &it = sizesData[i];
			ScreenInfo info { it.width, it.height, it.mwidth, it.mheight };

			if (ratesVec.size() > i) {
				info.rates = ratesVec[i];
			} else if (ratesVec.size() == 1) {
				info.rates = ratesVec[0];
			} else {
				info.rates = Vector<uint16_t>{ _rate };
			}

			ret.screenInfo.emplace_back(move(info));
		}

		::free(reply);
	} while (0);

	do {
		auto modesReply = _xcb->xcb_randr_get_screen_resources_reply(_connection, screenResCookie, nullptr);
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
				auto rate = uint16_t(floor(double(modes->dot_clock) / (double(modes->htotal) * double(vTotal))));
				ret.modeInfo.emplace_back(ModeInfo{modes->id, modes->width, modes->height, rate});
			}

			++ modes;
			-- nmodes;
		}

		::free(modesReply);
	} while (0);

	do {
		auto reply = _xcb->xcb_randr_get_output_info_reply(_connection, outputInfoCookie, nullptr);
		auto modes = _xcb->xcb_randr_get_output_info_modes(reply);
		auto nmodes = _xcb->xcb_randr_get_output_info_modes_length(reply);

		while (nmodes > 0) {
			ret.primaryOutput.modes.emplace_back(*modes);

			++ modes;
			-- nmodes;
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
			++ outputsPtr;
			-- noutputs;
		}

		auto possiblePtr = _xcb->xcb_randr_get_crtc_info_possible(reply);
		auto npossible = _xcb->xcb_randr_get_crtc_info_possible_length(reply);

		possible.reserve(npossible);

		while (npossible) {
			possible.emplace_back(*possiblePtr);
			++ possiblePtr;
			-- npossible;
		}

		ret.crtcInfo.emplace_back(CrtcInfo{
			crtcCookie.first, reply->x, reply->y, reply->width, reply->height, reply->mode, reply->rotation, reply->rotations,
			move(outputs), move(possible)
		});

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

static void look_for(uint16_t &mask, xcb_keycode_t *codes, xcb_keycode_t kc, int i) {
	if (mask == 0 && codes) {
		for (xcb_keycode_t *ktest = codes; *ktest; ktest++) {
			if (*ktest == kc) {
				mask = (uint16_t) (1 << i);
				break;
			}
		}
	}
}

void XcbView::initXkb() {
	uint16_t xkbMajorVersion = 0;
	uint16_t xkbMinorVersion = 0;

	if (!_xcbSetup) {
		if (_xkb->xkb_x11_setup_xkb_extension(_connection, XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION,
				XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS, &xkbMajorVersion, &xkbMinorVersion, &_xkbFirstEvent, &_xkbFirstError) != 1) {
			return;
		}
	}

	_xcbSetup = true;
	_xkbDeviceId = _xkb->xkb_x11_get_core_keyboard_device_id(_connection);

	enum {
		required_events = (XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY | XCB_XKB_EVENT_TYPE_STATE_NOTIFY),

		required_nkn_details = (XCB_XKB_NKN_DETAIL_KEYCODES),

		required_map_parts = (XCB_XKB_MAP_PART_KEY_TYPES | XCB_XKB_MAP_PART_KEY_SYMS | XCB_XKB_MAP_PART_MODIFIER_MAP
				| XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS | XCB_XKB_MAP_PART_KEY_ACTIONS
				| XCB_XKB_MAP_PART_VIRTUAL_MODS | XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP),

		required_state_details = (XCB_XKB_STATE_PART_MODIFIER_BASE | XCB_XKB_STATE_PART_MODIFIER_LATCH
				| XCB_XKB_STATE_PART_MODIFIER_LOCK | XCB_XKB_STATE_PART_GROUP_BASE
				| XCB_XKB_STATE_PART_GROUP_LATCH | XCB_XKB_STATE_PART_GROUP_LOCK),
	};

	static const xcb_xkb_select_events_details_t details = {
			.affectNewKeyboard = required_nkn_details,
			.newKeyboardDetails = required_nkn_details,
			.affectState = required_state_details,
			.stateDetails = required_state_details
	};

	_xcb->xcb_xkb_select_events(_connection, _xkbDeviceId, required_events, 0,
			required_events, required_map_parts, required_map_parts, &details);

	updateXkbMapping();
}

void XcbView::updateXkbMapping() {
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

	_xkbKeymap = _xkb->xkb_x11_keymap_new_from_device(_xkb->getContext(), _connection, _xkbDeviceId, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (_xkbKeymap == nullptr) {
		fprintf( stderr, "Failed to get Keymap for current keyboard device.\n");
		return;
	}

	_xkbState = _xkb->xkb_x11_state_new_from_device(_xkbKeymap, _connection, _xkbDeviceId);
	if (_xkbState == nullptr) {
		fprintf( stderr, "Failed to get state object for current keyboard device.\n");
		return;
	}

	memset(_keycodes, 0, sizeof(core::InputKeyCode) * 256);

	_xkb->xkb_keymap_key_for_each(_xkbKeymap, [] (struct xkb_keymap *keymap, xkb_keycode_t key, void *data) {
		((XcbView *)data)->updateXkbKey(key);
	}, this);

	auto locale = getenv("LC_ALL");
	if (!locale) { locale = getenv("LC_CTYPE"); }
	if (!locale) { locale = getenv("LANG"); }

	auto composeTable = _xkb->xkb_compose_table_new_from_locale(_xkb->getContext(),
			locale ? locale : "C", XKB_COMPOSE_COMPILE_NO_FLAGS);
	if (composeTable) {
		_xkbCompose = _xkb->xkb_compose_state_new(composeTable, XKB_COMPOSE_STATE_NO_FLAGS);
		_xkb->xkb_compose_table_unref(composeTable);
	}
}

void XcbView::updateKeysymMapping() {
	if (_keysyms) {
		_xcb->xcb_key_symbols_free(_keysyms);
	}

	if (_xcb->hasKeysyms()) {
		_keysyms = _xcb->xcb_key_symbols_alloc( _connection );
	}

	if (!_keysyms) {
		return;
	}

	auto modifierCookie = _xcb->xcb_get_modifier_mapping_unchecked( _connection );

	xcb_get_keyboard_mapping_cookie_t mappingCookie;
	const xcb_setup_t* setup = _xcb->xcb_get_setup(_connection);

	if (!_xkb) {
		mappingCookie = _xcb->xcb_get_keyboard_mapping(_connection, setup->min_keycode, setup->max_keycode - setup->min_keycode + 1);
	}

	auto numlockcodes = _xcb->xcb_key_symbols_get_keycode( _keysyms, XK_Num_Lock );
	auto shiftlockcodes = _xcb->xcb_key_symbols_get_keycode( _keysyms, XK_Shift_Lock );
	auto capslockcodes = _xcb->xcb_key_symbols_get_keycode( _keysyms, XK_Caps_Lock );
	auto modeswitchcodes = _xcb->xcb_key_symbols_get_keycode( _keysyms, XK_Mode_switch );

	auto modmap_r = _xcb->xcb_get_modifier_mapping_reply( _connection, modifierCookie, nullptr );
	if (!modmap_r) {
		return;
	}

	xcb_keycode_t *modmap = _xcb->xcb_get_modifier_mapping_keycodes( modmap_r );

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
		xcb_get_keyboard_mapping_reply_t *keyboard_mapping = _xcb->xcb_get_keyboard_mapping_reply(_connection, mappingCookie, NULL);

		int nkeycodes = keyboard_mapping->length / keyboard_mapping->keysyms_per_keycode;

		xcb_keysym_t *keysyms = (xcb_keysym_t*) (keyboard_mapping + 1);

		for (int keycode_idx = 0; keycode_idx < nkeycodes; ++keycode_idx) {
			_keycodes[setup->min_keycode + keycode_idx] = getKeysymCode(keysyms[keycode_idx * keyboard_mapping->keysyms_per_keycode]);
		}

		free(keyboard_mapping);
	}
}

xcb_keysym_t XcbView::getKeysym(xcb_keycode_t code, uint16_t state, bool resolveMods) {
	xcb_keysym_t k0, k1;

	if (!resolveMods) {
		k0 = _xcb->xcb_key_symbols_get_keysym(_keysyms, code, 0);
		// resolve only numlock
		if ((state & _numlock)) {
			k1 = _xcb->xcb_key_symbols_get_keysym(_keysyms, code, 1);
			if (_xcb->xcb_is_keypad_key(k1)) {
				if ((state & XCB_MOD_MASK_SHIFT) || ((state & XCB_MOD_MASK_LOCK) && (state & _shiftlock))) {
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

	if (k1 == XCB_NO_SYMBOL)
		k1 = k0;

	if ((state & _numlock) && _xcb->xcb_is_keypad_key(k1)) {
		if ((state & XCB_MOD_MASK_SHIFT) || ((state & XCB_MOD_MASK_LOCK) && (state & _shiftlock))) {
			return k0;
		} else {
			return k1;
		}
	} else if (!(state & XCB_MOD_MASK_SHIFT) && !(state & XCB_MOD_MASK_LOCK)) {
		return k0;
	} else if (!(state & XCB_MOD_MASK_SHIFT) && ((state & XCB_MOD_MASK_LOCK) && (state & _capslock))) {
		if (k0 >= XK_0 && k0 <= XK_9) {
			return k0;
		}
		return k1;
	} else if ((state & XCB_MOD_MASK_SHIFT) && ((state & XCB_MOD_MASK_LOCK) && (state & _capslock))) {
		return k1;
	} else if ((state & XCB_MOD_MASK_SHIFT) || ((state & XCB_MOD_MASK_LOCK) && (state & _shiftlock))) {
		return k1;
	}

	return XCB_NO_SYMBOL;
}

xkb_keysym_t XcbView::composeSymbol(xkb_keysym_t sym, core::InputKeyComposeState &compose) const {
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
	default:
		XL_X11_LOG("Compose: ", sym, ": ", composedSym, " (error)");
		break;
	}
	return composedSym;
}

void XcbView::updateXkbKey(xcb_keycode_t code) {
	static const struct {
		core::InputKeyCode key;
		const char *name;
	} keymap[] = {
		{ core::InputKeyCode::GRAVE_ACCENT, "TLDE" },
		{ core::InputKeyCode::_1, "AE01" },
		{ core::InputKeyCode::_2, "AE02" },
		{ core::InputKeyCode::_3, "AE03" },
		{ core::InputKeyCode::_4, "AE04" },
		{ core::InputKeyCode::_5, "AE05" },
		{ core::InputKeyCode::_6, "AE06" },
		{ core::InputKeyCode::_7, "AE07" },
		{ core::InputKeyCode::_8, "AE08" },
		{ core::InputKeyCode::_9, "AE09" },
		{ core::InputKeyCode::_0, "AE10" },
		{ core::InputKeyCode::MINUS, "AE11" },
		{ core::InputKeyCode::EQUAL, "AE12" },
		{ core::InputKeyCode::Q, "AD01" },
		{ core::InputKeyCode::W, "AD02" },
		{ core::InputKeyCode::E, "AD03" },
		{ core::InputKeyCode::R, "AD04" },
		{ core::InputKeyCode::T, "AD05" },
		{ core::InputKeyCode::Y, "AD06" },
		{ core::InputKeyCode::U, "AD07" },
		{ core::InputKeyCode::I, "AD08" },
		{ core::InputKeyCode::O, "AD09" },
		{ core::InputKeyCode::P, "AD10" },
		{ core::InputKeyCode::LEFT_BRACKET, "AD11" },
		{ core::InputKeyCode::RIGHT_BRACKET, "AD12" },
		{ core::InputKeyCode::A, "AC01" },
		{ core::InputKeyCode::S, "AC02" },
		{ core::InputKeyCode::D, "AC03" },
		{ core::InputKeyCode::F, "AC04" },
		{ core::InputKeyCode::G, "AC05" },
		{ core::InputKeyCode::H, "AC06" },
		{ core::InputKeyCode::J, "AC07" },
		{ core::InputKeyCode::K, "AC08" },
		{ core::InputKeyCode::L, "AC09" },
		{ core::InputKeyCode::SEMICOLON, "AC10" },
		{ core::InputKeyCode::APOSTROPHE, "AC11" },
		{ core::InputKeyCode::Z, "AB01" },
		{ core::InputKeyCode::X, "AB02" },
		{ core::InputKeyCode::C, "AB03" },
		{ core::InputKeyCode::V, "AB04" },
		{ core::InputKeyCode::B, "AB05" },
		{ core::InputKeyCode::N, "AB06" },
		{ core::InputKeyCode::M, "AB07" },
		{ core::InputKeyCode::COMMA, "AB08" },
		{ core::InputKeyCode::PERIOD, "AB09" },
		{ core::InputKeyCode::SLASH, "AB10" },
		{ core::InputKeyCode::BACKSLASH, "BKSL" },
		{ core::InputKeyCode::WORLD_1, "LSGT" },
		{ core::InputKeyCode::SPACE, "SPCE" },
		{ core::InputKeyCode::ESCAPE, "ESC" },
		{ core::InputKeyCode::ENTER, "RTRN" },
		{ core::InputKeyCode::TAB, "TAB" },
		{ core::InputKeyCode::BACKSPACE, "BKSP" },
		{ core::InputKeyCode::INSERT, "INS" },
		{ core::InputKeyCode::DELETE, "DELE" },
		{ core::InputKeyCode::RIGHT, "RGHT" },
		{ core::InputKeyCode::LEFT, "LEFT" },
		{ core::InputKeyCode::DOWN, "DOWN" },
		{ core::InputKeyCode::UP, "UP" },
		{ core::InputKeyCode::PAGE_UP, "PGUP" },
		{ core::InputKeyCode::PAGE_DOWN, "PGDN" },
		{ core::InputKeyCode::HOME, "HOME" },
		{ core::InputKeyCode::END, "END" },
		{ core::InputKeyCode::CAPS_LOCK, "CAPS" },
		{ core::InputKeyCode::SCROLL_LOCK, "SCLK" },
		{ core::InputKeyCode::NUM_LOCK, "NMLK" },
		{ core::InputKeyCode::PRINT_SCREEN, "PRSC" },
		{ core::InputKeyCode::PAUSE, "PAUS" },
		{ core::InputKeyCode::F1, "FK01" },
		{ core::InputKeyCode::F2, "FK02" },
		{ core::InputKeyCode::F3, "FK03" },
		{ core::InputKeyCode::F4, "FK04" },
		{ core::InputKeyCode::F5, "FK05" },
		{ core::InputKeyCode::F6, "FK06" },
		{ core::InputKeyCode::F7, "FK07" },
		{ core::InputKeyCode::F8, "FK08" },
		{ core::InputKeyCode::F9, "FK09" },
		{ core::InputKeyCode::F10, "FK10" },
		{ core::InputKeyCode::F11, "FK11" },
		{ core::InputKeyCode::F12, "FK12" },
		{ core::InputKeyCode::F13, "FK13" },
		{ core::InputKeyCode::F14, "FK14" },
		{ core::InputKeyCode::F15, "FK15" },
		{ core::InputKeyCode::F16, "FK16" },
		{ core::InputKeyCode::F17, "FK17" },
		{ core::InputKeyCode::F18, "FK18" },
		{ core::InputKeyCode::F19, "FK19" },
		{ core::InputKeyCode::F20, "FK20" },
		{ core::InputKeyCode::F21, "FK21" },
		{ core::InputKeyCode::F22, "FK22" },
		{ core::InputKeyCode::F23, "FK23" },
		{ core::InputKeyCode::F24, "FK24" },
		{ core::InputKeyCode::F25, "FK25" },
		{ core::InputKeyCode::KP_0, "KP0" },
		{ core::InputKeyCode::KP_1, "KP1" },
		{ core::InputKeyCode::KP_2, "KP2" },
		{ core::InputKeyCode::KP_3, "KP3" },
		{ core::InputKeyCode::KP_4, "KP4" },
		{ core::InputKeyCode::KP_5, "KP5" },
		{ core::InputKeyCode::KP_6, "KP6" },
		{ core::InputKeyCode::KP_7, "KP7" },
		{ core::InputKeyCode::KP_8, "KP8" },
		{ core::InputKeyCode::KP_9, "KP9" },
		{ core::InputKeyCode::KP_DECIMAL, "KPDL" },
		{ core::InputKeyCode::KP_DIVIDE, "KPDV" },
		{ core::InputKeyCode::KP_MULTIPLY, "KPMU" },
		{ core::InputKeyCode::KP_SUBTRACT, "KPSU" },
		{ core::InputKeyCode::KP_ADD, "KPAD" },
		{ core::InputKeyCode::KP_ENTER, "KPEN" },
		{ core::InputKeyCode::KP_EQUAL, "KPEQ" },
		{ core::InputKeyCode::LEFT_SHIFT, "LFSH" },
		{ core::InputKeyCode::LEFT_CONTROL, "LCTL" },
		{ core::InputKeyCode::LEFT_ALT, "LALT" },
		{ core::InputKeyCode::LEFT_SUPER, "LWIN" },
		{ core::InputKeyCode::RIGHT_SHIFT, "RTSH" },
		{ core::InputKeyCode::RIGHT_CONTROL, "RCTL" },
		{ core::InputKeyCode::RIGHT_ALT, "RALT" },
		{ core::InputKeyCode::RIGHT_ALT, "LVL3" },
		{ core::InputKeyCode::RIGHT_ALT, "MDSW" },
		{ core::InputKeyCode::RIGHT_SUPER, "RWIN" },
		{ core::InputKeyCode::MENU, "MENU" }
	};

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

core::InputKeyCode XcbView::getKeyCode(xcb_keycode_t code) const {
	return _keycodes[code];
}

core::InputKeyCode XcbView::getKeysymCode(xcb_keysym_t sym) const {
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
	case XK_F25: return core::InputKeyCode::F25;

		// Numeric keypad
	case XK_KP_Divide: return core::InputKeyCode::KP_DIVIDE;
	case XK_KP_Multiply: return core::InputKeyCode::KP_MULTIPLY;
	case XK_KP_Subtract: return core::InputKeyCode::KP_SUBTRACT;
	case XK_KP_Add: return core::InputKeyCode::KP_ADD;

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
	case XK_KP_Enter: return core::InputKeyCode::KP_ENTER;

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

void XcbView::notifyClipboard(BytesView data) {
	if (_clipboardCallback) {
		_clipboardCallback(data, "text/plain");
	}
	_clipboardCallback = nullptr;
	_clipboardTarget = nullptr;
}

xcb_atom_t XcbView::writeTargetToProperty(xcb_selection_request_event_t *request) {
	if (request->property == 0) {
		// The requester is a legacy client (ICCCM section 2.2)
		// We don't support legacy clients, so fail here
		return 0;
	}

	if (request->target == _atoms[toInt(XcbAtomIndex::TARGETS)]) {
		// The list of supported targets was requested

		const xcb_atom_t targets[] = { _atoms[toInt(XcbAtomIndex::TARGETS)], _atoms[toInt(XcbAtomIndex::STRING)] };

		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, request->requestor, request->property, XCB_ATOM_ATOM, 8*sizeof(xcb_atom_t),
				sizeof(targets) / sizeof(targets[0]), (unsigned char*) targets);
		return request->property;
	}

	if (request->target == _atoms[toInt(XcbAtomIndex::SAVE_TARGETS)]) {
		// The request is a check whether we support SAVE_TARGETS
		// It should be handled as a no-op side effect target

		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, request->requestor, request->property,
				_atoms[toInt(XcbAtomIndex::XNULL)], 32, 0, nullptr);
		return request->property;
	}

	// Conversion to a data target was requested

	if (request->target == _atoms[toInt(XcbAtomIndex::STRING)]) {
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, request->requestor, request->property, request->target, 8,
				_clipboardSelection.size(), _clipboardSelection.data());
		return request->property;
	}

	// The requested target is not supported

	return 0;
}

void XcbView::handleSelectionRequest(xcb_selection_request_event_t *event) {
	if (event->target == _atoms[toInt(XcbAtomIndex::TARGETS)]) {
		// The list of supported targets was requested

		const xcb_atom_t targets[] = { _atoms[toInt(XcbAtomIndex::TARGETS)], _atoms[toInt(XcbAtomIndex::STRING)] };

		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, event->requestor, event->property, XCB_ATOM_ATOM, 8*sizeof(xcb_atom_t),
				sizeof(targets) / sizeof(targets[0]), (unsigned char*) targets);
	}

	if (event->target == _atoms[toInt(XcbAtomIndex::SAVE_TARGETS)]) {
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, event->requestor, event->property,
				_atoms[toInt(XcbAtomIndex::XNULL)], 32, 0, nullptr);
	}

	if (event->target == _atoms[toInt(XcbAtomIndex::STRING)]) {
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, event->requestor, event->property, event->target, 8,
				_clipboardSelection.size(), _clipboardSelection.data());
	}

	xcb_selection_notify_event_t notify;
	notify.response_type = XCB_SELECTION_NOTIFY;
	notify.pad0          = 0;
	notify.sequence      = 0;
	notify.time          = event->time;
	notify.requestor     = event->requestor;
	notify.selection     = event->selection;
	notify.target        = event->target;
	notify.property      = event->property;

	_xcb->xcb_send_event(_connection, false, event->requestor, XCB_EVENT_MASK_NO_EVENT, // SelectionNotify events go without mask
			(const char*) &notify);
	_xcb->xcb_flush(_connection);
}

}
