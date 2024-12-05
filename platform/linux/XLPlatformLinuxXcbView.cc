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
#include "XLCoreInput.h"
#include <X11/keysym.h>

uint32_t _glfwKeySym2Unicode(unsigned int keysym);

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

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

XcbView::~XcbView() {
	_defaultScreen = nullptr;
	if (_info.syncCounter) {
		_xcb->xcb_sync_destroy_counter(_connection->getConnection(), _info.syncCounter);
		_info.syncCounter = 0;
	}
	_connection = nullptr;
}

XcbView::XcbView(Rc<XcbConnection> &&conn, ViewInterface *view, StringView title, StringView bundleId, URect rect) {
	_connection = move(conn);
	_view = view;
	_xcb = _connection->getXcb();
	_xkb = _connection->getXkb();

#if DEBUG
	auto d = getenv("DISPLAY");
	if (!d) {
		stappler::log::warn("XcbView-Info", "DISPLAY is not defined");
	}
#endif

	if (_connection->hasErrors()) {
		return;
	}

	_wmClass.resize(title.size() + bundleId.size() + 1, char(0));
	memcpy(_wmClass.data(), title.data(), title.size());
	memcpy(_wmClass.data() + title.size() + 1, bundleId.data(), bundleId.size());

	_defaultScreen = _connection->getDefaultScreen();

	_info.parent = _defaultScreen->root;
	_info.visual = _defaultScreen->root_visual;

	_info.eventMask = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION
			| XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE
			| XCB_EVENT_MASK_VISIBILITY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY
			| XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_COLOR_MAP_CHANGE
			| XCB_EVENT_MASK_OWNER_GRAB_BUTTON;

	_info.overrideRedirect = 0;
	_info.overrideClose = true;
	//_info.enableSync = true;

	_info.rect = xcb_rectangle_t{
		static_cast<int16_t>(rect.x), static_cast<int16_t>(rect.y),
		static_cast<uint16_t>(rect.width), static_cast<uint16_t>(rect.height)
	};

	_info.title = title;
	_info.icon = title;
	_info.wmClass = _wmClass;

	if (!_connection->createWindow(_info)) {
		log::error("XCB", "Fail to create window");
	}

	_screenInfo = _connection->getScreenInfo(_defaultScreen);
	_rate = _screenInfo.primaryMode.rate;
}

void XcbView::handleConfigureNotify(xcb_configure_notify_event_t *ev) {
	XL_X11_LOG("XCB_CONFIGURE_NOTIFY: %d (%d) rect:%d,%d,%d,%d border:%d override:%d\n", ev->event, ev->window,
		ev->x, ev->y, ev->width, ev->height, uint32_t(ev->border_width), uint32_t(ev->override_redirect));
	_info.rect.x = ev->x;
	_info.rect.y = ev->y;
	_borderWidth = ev->border_width;
	if (ev->width != _info.rect.width || ev->height != _info.rect.height) {
		_info.rect.width = ev->width;
		_info.rect.height = ev->height;
		_deprecateSwapchain = true;
	}
}

void XcbView::handleButtonPress(xcb_button_press_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		_lastInputTime = ev->time;
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

	_pendingEvents.emplace_back(event);
}

void XcbView::handleButtonRelease(xcb_button_release_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		_lastInputTime = ev->time;
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
		_pendingEvents.emplace_back(event);
		break;
	}
}

void XcbView::handleMotionNotify(xcb_motion_notify_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		_lastInputTime = ev->time;
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

	_pendingEvents.emplace_back(event);
}

void XcbView::handleEnterNotify(xcb_enter_notify_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		_lastInputTime = ev->time;
	}

	auto ext = _view->getExtent();
	_pendingEvents.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::PointerEnter, true,
			Vec2(float(ev->event_x), float(ext.height - ev->event_y))));
}

void XcbView::handleLeaveNotify(xcb_leave_notify_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		_lastInputTime = ev->time;
	}

	auto ext = _view->getExtent();
	_pendingEvents.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::PointerEnter, false,
			Vec2(float(ev->event_x), float(ext.height - ev->event_y))));
}

void XcbView::handleFocusIn(xcb_focus_in_event_t *ev) {
	// xcb_focus_in_event_t *ev = (xcb_focus_in_event_t*) e;
	_pendingEvents.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::FocusGain, true));
}

void XcbView::handleFocusOut(xcb_focus_out_event_t *ev) {
	_pendingEvents.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::FocusGain, false));
}

void XcbView::handleKeyPress(xcb_key_press_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		_lastInputTime = ev->time;
	}

	auto mod = getModifiers(ev->state);
	auto ext = _view->getExtent();

	// in case of key autorepeat, ev->time will match
	// just replace event name from previous InputEventName::KeyReleased to InputEventName::KeyRepeated
	if (!_pendingEvents.empty() && _pendingEvents.back().event == core::InputEventName::KeyReleased) {
		auto &iev = _pendingEvents.back();
		if (iev.id == ev->time && iev.modifiers == mod && iev.x == float(ev->event_x)
				&& iev.y == float(ext.height - ev->event_y)
				&& iev.key.keysym == _connection->getKeysym(ev->detail, ev->state, false)) {
			iev.event = core::InputEventName::KeyRepeated;
			return;
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
		event.key.keycode = _connection->getKeyCode(ev->detail);
		event.key.compose = core::InputKeyComposeState::Nothing;
		event.key.keysym = _connection->getKeysym(ev->detail, ev->state, false);
		if (_view->isInputEnabled()) {
			const auto keysym = _connection->composeSymbol(_xkb->xkb_state_key_get_one_sym(_xkbState, ev->detail), event.key.compose);
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
		auto sym = _connection->getKeysym(ev->detail, ev->state, false); // state-inpependent keysym
		event.key.keycode = _connection->getKeysymCode(sym);
		event.key.compose = core::InputKeyComposeState::Nothing;
		event.key.keysym = sym;
		if (_view->isInputEnabled()) {
			event.key.keychar = _glfwKeySym2Unicode(_connection->getKeysym(ev->detail, ev->state, true)); // use state-dependent keysym
		} else {
			event.key.keychar = 0;
		}
	}

	_pendingEvents.emplace_back(event);

#if XL_X11_DEBUG
	String str;
	unicode::utf8Encode(str, event.key.keychar);
	XL_X11_LOG("Key pressed in window ", ev->event, " (", (int)ev->time, ") ", event.key.keysym,
			" '", str, "' ", uint32_t(event.key.keychar));
#endif
}

void XcbView::handleKeyRelease(xcb_key_release_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		_lastInputTime = ev->time;
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
		event.key.keycode = _connection->getKeyCode(ev->detail);
		event.key.compose = core::InputKeyComposeState::Nothing;
		event.key.keysym = _connection->getKeysym(ev->detail, ev->state, false);
		if (_view->isInputEnabled()) {
			event.key.keychar = _xkb->xkb_state_key_get_utf32(_xkbState, ev->detail);
		} else {
			event.key.keychar = 0;
		}
	} else {
		auto sym = _connection->getKeysym(ev->detail, ev->state, false); // state-inpependent keysym
		event.key.keycode = _connection->getKeysymCode(sym);
		event.key.compose = core::InputKeyComposeState::Nothing;
		event.key.keysym = sym;
		if (_view->isInputEnabled()) {
			event.key.keychar = _glfwKeySym2Unicode(_connection->getKeysym(ev->detail, ev->state, true)); // use state-dependent keysym
		} else {
			event.key.keychar = 0;
		}
	}

	_pendingEvents.emplace_back(event);

#if XL_X11_DEBUG
	String str;
	unicode::utf8Encode(str, event.key.keychar);
	XL_X11_LOG("Key released in window ", ev->event, " (", (int)ev->time, ") ", event.key.keysym,
			" '", str, "' ", uint32_t(event.key.keychar));
#endif
}

void XcbView::handleSelectionNotify(xcb_selection_notify_event_t *event) {
	xcb_get_property_reply_t *reply;

	if (event->property == _connection->getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD)) {
		reply = _xcb->xcb_get_property_reply(_connection->getConnection(),
				_xcb->xcb_get_property(_connection->getConnection(), 1, _info.window,
						_connection->getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD),
						_connection->getAtom(XcbAtomIndex::UTF8_STRING), 0, 300),
				NULL);
		notifyClipboard(BytesView((const uint8_t *)_xcb->xcb_get_property_value(reply), _xcb->xcb_get_property_value_length(reply)));
		free(reply);
	}
}

void XcbView::handleSelectionRequest(xcb_selection_request_event_t *event) {
	if (event->target == _connection->getAtom(XcbAtomIndex::TARGETS)) {
		// The list of supported targets was requested

		const xcb_atom_t targets[] = { _connection->getAtom(XcbAtomIndex::TARGETS), _connection->getAtom(XcbAtomIndex::UTF8_STRING)};

		_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE, event->requestor, event->property, XCB_ATOM_ATOM, 8*sizeof(xcb_atom_t),
				sizeof(targets) / sizeof(targets[0]), (unsigned char*) targets);
	}

	if (event->target == _connection->getAtom(XcbAtomIndex::SAVE_TARGETS)) {
		_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE, event->requestor, event->property,
				_connection->getAtom(XcbAtomIndex::XNULL), 32, 0, nullptr);
	}

	if (event->target == _connection->getAtom(XcbAtomIndex::UTF8_STRING)) {
		_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE, event->requestor, event->property, event->target, 8,
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

	_xcb->xcb_send_event(_connection->getConnection(), false, event->requestor, XCB_EVENT_MASK_NO_EVENT, // SelectionNotify events go without mask
			(const char*) &notify);
	_xcb->xcb_flush(_connection->getConnection());
}

void XcbView::handleSyncRequest(xcb_timestamp_t syncTime, xcb_sync_int64_t value) {
	_lastSyncTime = syncTime;
	_info.syncValue = value;
}

void XcbView::handleCloseRequest() {
	_shouldClose = true;
	_connection->detachWindow(_info.window);
}

void XcbView::handleScreenChangeNotify(xcb_randr_screen_change_notify_event_t *ev) {
	_screenInfo = _connection->getScreenInfo(ev->root);
	if (_rate != _screenInfo.primaryMode.rate) {
		_rate = _screenInfo.primaryMode.rate;
	}
}

void XcbView::dispatchPendingEvents() {
	if (!_pendingEvents.empty()) {
		_view->handleInputEvents(sp::move(_pendingEvents));
	}
	_pendingEvents.clear();

	if (_deprecateSwapchain) {
		_view->deprecateSwapchain();
		_deprecateSwapchain = false;
	}
}

uint64_t XcbView::getScreenFrameInterval() const {
	return 1'000'000 / _rate;
}

void XcbView::mapWindow() {
	_connection->attachWindow(_info.window, this);
	_xcb->xcb_map_window(_connection->getConnection(), _info.window);
	_xcb->xcb_flush(_connection->getConnection());
}

void XcbView::handleSwapchainRecreation() {
    if (_info.syncCounter && (_info.syncValue.lo != 0 || _info.syncValue.hi != 0)) {
    	_xcb->xcb_sync_set_counter(_connection->getConnection(), _info.syncCounter, _info.syncValue);
    	_xcb-> xcb_flush(_connection->getConnection());

        _info.syncValue.lo = 0;
        _info.syncValue.hi = 0;
    }
}

xcb_connection_t *XcbView::getConnection() const {
	return _connection->getConnection();
}

void XcbView::readFromClipboard(Function<void(BytesView, StringView)> &&cb, Ref *ref) {
	auto cookie = _xcb->xcb_get_selection_owner(_connection->getConnection(), _connection->getAtom(XcbAtomIndex::CLIPBOARD));
	auto reply = _xcb->xcb_get_selection_owner_reply(_connection->getConnection(), cookie, nullptr);

	if (reply->owner == _info.window) {
		cb(_clipboardSelection, StringView("text/plain"));
	} else {
		_xcb->xcb_convert_selection(_connection->getConnection(), _info.window,
				_connection->getAtom(XcbAtomIndex::CLIPBOARD),
				_connection->getAtom(XcbAtomIndex::UTF8_STRING),
				_connection->getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD), XCB_CURRENT_TIME);
		_xcb->xcb_flush(_connection->getConnection());

		if (_clipboardCallback) {
			_clipboardCallback(BytesView(), StringView());
		}

		_clipboardCallback = sp::move(cb);
		_clipboardTarget = ref;
	}
}

void XcbView::writeToClipboard(BytesView data, StringView contentType) {
	_clipboardSelection = data.bytes<Interface>();

	_xcb->xcb_set_selection_owner(_connection->getConnection(), _info.window, _connection->getAtom(XcbAtomIndex::CLIPBOARD), XCB_CURRENT_TIME);

	auto cookie = _xcb->xcb_get_selection_owner(_connection->getConnection(), _connection->getAtom(XcbAtomIndex::CLIPBOARD));
	auto reply = _xcb->xcb_get_selection_owner_reply(_connection->getConnection(), cookie, nullptr);
	if (reply->owner != _info.window) {
		log::error("XcbView", "Fail to set selection owner");
	}
	::free(reply);
}

int XcbView::getSocketFd() const {
	return _connection->getSocket();
}

bool XcbView::poll(bool frameReady) {
	_connection->poll();
	return !_shouldClose;
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

	if (request->target == _connection->getAtom(XcbAtomIndex::TARGETS)) {
		// The list of supported targets was requested

		const xcb_atom_t targets[] = { _connection->getAtom(XcbAtomIndex::TARGETS), _connection->getAtom(XcbAtomIndex::UTF8_STRING) };

		_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE, request->requestor, request->property, XCB_ATOM_ATOM, 8*sizeof(xcb_atom_t),
				sizeof(targets) / sizeof(targets[0]), (unsigned char*) targets);
		return request->property;
	}

	if (request->target == _connection->getAtom(XcbAtomIndex::SAVE_TARGETS)) {
		// The request is a check whether we support SAVE_TARGETS
		// It should be handled as a no-op side effect target

		_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE, request->requestor, request->property,
				_connection->getAtom(XcbAtomIndex::XNULL), 32, 0, nullptr);
		return request->property;
	}

	// Conversion to a data target was requested

	if (request->target == _connection->getAtom(XcbAtomIndex::UTF8_STRING)) {
		_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE, request->requestor, request->property, request->target, 8,
				_clipboardSelection.size(), _clipboardSelection.data());
		return request->property;
	}

	// The requested target is not supported

	return 0;
}

void XcbView::updateWindowAttributes() {
	uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
	uint32_t values[2];
	values[0] = _info.overrideRedirect;
	values[1] = _info.eventMask;

	_xcb->xcb_change_window_attributes(_connection->getConnection(), _info.window, mask, values);
}

void XcbView::configureWindow(xcb_rectangle_t r, uint16_t border_width) {
	const uint32_t values[] = {
		static_cast<uint32_t>(r.x),
		static_cast<uint32_t>(r.y),
		static_cast<uint32_t>(r.width),
		static_cast<uint32_t>(r.height),
		border_width
	};

	_xcb->xcb_configure_window(_connection->getConnection(), _info.window,
			XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH,
			values);
	_xcb->xcb_flush(_connection->getConnection());
}

}
