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

#include "XLLinuxXcbWindow.h"
#include "SPLog.h"
#include "SPNotNull.h"
#include "SPStatus.h"
#include "XLContextInfo.h"
#include "XLLinuxContextController.h"
#include "linux/XLLinuxXcbConnection.h"
#include <xcb/xcb.h>

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkPresentationEngine.h"
#endif

#ifndef XL_X11_DEBUG
#if XL_X11_DEBUG
#define XL_X11_LOG(...) log::debug("XCB", __VA_ARGS__)
#else
#define XL_X11_LOG(...)
#endif
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static core::InputModifier getModifiers(uint32_t mask) {
	core::InputModifier ret = core::InputModifier::None;
	core::InputModifier *mod,
			mods[] = {core::InputModifier::Shift, core::InputModifier::CapsLock,
				core::InputModifier::Ctrl, core::InputModifier::Alt, core::InputModifier::NumLock,
				core::InputModifier::Mod3, core::InputModifier::Mod4, core::InputModifier::Mod5,
				core::InputModifier::Button1, core::InputModifier::Button2,
				core::InputModifier::Button3, core::InputModifier::Button4,
				core::InputModifier::Button5, core::InputModifier::LayoutAlternative};
	for (mod = mods; mask; mask >>= 1, mod++) {
		if (mask & 1) {
			ret |= *mod;
		}
	}
	return ret;
}

static core::InputMouseButton getButton(xcb_button_t btn) { return core::InputMouseButton(btn); }

XcbWindow::~XcbWindow() {
	if (_controller && _isRootWindow) {
		_controller.get_cast<LinuxContextController>()->handleRootWindowClosed();
	}
	if (_connection) {
		_defaultScreen = nullptr;
		if (_xinfo.syncCounter) {
			_xcb->xcb_sync_destroy_counter(_connection->getConnection(), _xinfo.syncCounter);
			_xinfo.syncCounter = 0;
		}

		if (_xinfo.window) {
			_xcb->xcb_destroy_window(_connection->getConnection(), _xinfo.window);
			_xinfo.window = 0;
		}
		_connection = nullptr;
	}
}

XcbWindow::XcbWindow() { }

bool XcbWindow::init(NotNull<XcbConnection> conn, Rc<WindowInfo> &&info,
		NotNull<const ContextInfo> ctx, NotNull<LinuxContextController> c) {
	if (!ContextNativeWindow::init(c, move(info),
				WindowCapabilities::FullscreenSwitch | WindowCapabilities::Subwindows)) {
		return false;
	}

	_connection = conn;
	_controller = c;

	_xcb = _connection->getXcb();

	if (_connection->hasErrors()) {
		return false;
	}

	_wmClass.resize(_info->title.size() + ctx->bundleName.size() + 1, char(0));
	memcpy(_wmClass.data(), _info->title.data(), _info->title.size());
	memcpy(_wmClass.data() + _info->title.size() + 1, ctx->bundleName.data(),
			ctx->bundleName.size());

	_defaultScreen = _connection->getDefaultScreen();

	_xinfo.parent = _defaultScreen->root;
	_xinfo.visual = _defaultScreen->root_visual;

	_xinfo.eventMask = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS
			| XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION
			| XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_KEY_PRESS
			| XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_VISIBILITY_CHANGE
			| XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY
			| XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
			| XCB_EVENT_MASK_COLOR_MAP_CHANGE | XCB_EVENT_MASK_OWNER_GRAB_BUTTON;

	_xinfo.overrideRedirect = 0;
	_xinfo.overrideClose = true;
	_xinfo.enableSync = true;

	_xinfo.rect = xcb_rectangle_t{static_cast<int16_t>(_info->rect.x),
		static_cast<int16_t>(_info->rect.y), static_cast<uint16_t>(_info->rect.width),
		static_cast<uint16_t>(_info->rect.height)};

	_xinfo.title = _info->title;
	_xinfo.icon = _info->title;
	_xinfo.wmClass = _wmClass;

	if (!_connection->createWindow(_info, _xinfo)) {
		log::error("XCB", "Fail to create window");
	}

	_screenInfo = _connection->getScreenInfo(_defaultScreen);
	_rate = _screenInfo->getCommonRate();

	_xcb->xcb_flush(_connection->getConnection());

	auto udpi = _connection->getUnscaledDpi();
	auto dpi = _connection->getDpi();

	if (udpi == 0) {
		udpi = 122'880;
	}

	_density = float(dpi) / float(udpi);

	if (_info->monitor != MonitorId::None) {
		setFullscreen(_info->monitor, _info->mode);
	}

	return true;
}

void XcbWindow::handleConfigureNotify(xcb_configure_notify_event_t *ev) {
	XL_X11_LOG("XCB_CONFIGURE_NOTIFY: %d (%d) rect:%d,%d,%d,%d border:%d override:%d\n", ev->event,
			ev->window, ev->x, ev->y, ev->width, ev->height, uint32_t(ev->border_width),
			uint32_t(ev->override_redirect));
	_xinfo.rect.x = ev->x;
	_xinfo.rect.y = ev->y;
	_borderWidth = ev->border_width;
	if (ev->width != _xinfo.rect.width || ev->height != _xinfo.rect.height) {
		_xinfo.rect.width = ev->width;
		_xinfo.rect.height = ev->height;
		_controller->notifyWindowConstraintsChanged(this, true);
	}

	_info->rect = URect{uint16_t(_xinfo.rect.x), uint16_t(_xinfo.rect.y), _xinfo.rect.width,
		_xinfo.rect.height};
}

void XcbWindow::handlePropertyNotify(xcb_property_notify_event_t *ev) {
	if (ev->atom == _connection->getAtom(XcbAtomIndex::_NET_WM_STATE)) {
		auto cookie = _xcb->xcb_get_property_unchecked(_connection->getConnection(), 0,
				_xinfo.window, _connection->getAtom(XcbAtomIndex::_NET_WM_STATE), XCB_ATOM_ATOM, 0,
				sizeof(xcb_atom_t) * 16);
		auto reply = _connection->perform(_xcb->xcb_get_property_reply, cookie);
		if (reply) {
			auto values = (xcb_atom_t *)_xcb->xcb_get_property_value(reply);
			auto len = _xcb->xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);

			auto state = StateFlags::None;

			while (len > 0) {
				if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_MODAL) == *values) {
					state |= StateFlags::Modal;
				}
				if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_STICKY) == *values) {
					state |= StateFlags::Sticky;
				}
				if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_MAXIMIZED_VERT) == *values) {
					state |= StateFlags::MaximizedVert;
				}
				if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_MAXIMIZED_HORZ) == *values) {
					state |= StateFlags::MaximizedHorz;
				}
				if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_SHADED) == *values) {
					state |= StateFlags::Shaded;
				}
				if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_SKIP_TASKBAR) == *values) {
					state |= StateFlags::SkipTaskbar;
				}
				if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_SKIP_PAGER) == *values) {
					state |= StateFlags::SkipPager;
				}
				if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_HIDDEN) == *values) {
					state |= StateFlags::Hidden;
				}
				if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_FULLSCREEN) == *values) {
					state |= StateFlags::Fullscreen;
				}
				if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_ABOVE) == *values) {
					state |= StateFlags::Above;
				}
				if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_BELOW) == *values) {
					state |= StateFlags::Below;
				}
				if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_DEMANDS_ATTENTION)
						== *values) {
					state |= StateFlags::DemandsAttention;
				}
				if (_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_FOCUSED) == *values) {
					state |= StateFlags::Focused;
				}
				++values;
				--len;
			}

			if (!hasFlag(_state, Fullscreen) && hasFlag(state, Fullscreen)) {
				// enter fullscreen
				_pendingEvents.emplace_back(
						core::InputEventData::BoolEvent(core::InputEventName::Fullscreen, true));
			}
			if (hasFlag(_state, Fullscreen) && !hasFlag(state, Fullscreen)) {
				// exit fullscreen
				_pendingEvents.emplace_back(
						core::InputEventData::BoolEvent(core::InputEventName::Fullscreen, false));
			}

			_state = state;
		}
	} else {
		log::debug("XcbWindow", "handlePropertyNotify: ", _connection->getAtomName(ev->atom));
	}
}

void XcbWindow::handleButtonPress(xcb_button_press_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		_lastInputTime = ev->time;
	}

	auto ext = Extent2(_xinfo.rect.width, _xinfo.rect.height);
	auto mod = getModifiers(ev->state);
	auto btn = getButton(ev->detail);

	core::InputEventData event({ev->detail, core::InputEventName::Begin, btn, mod,
		float(ev->event_x), float(ext.height - ev->event_y)});

	switch (btn) {
	case core::InputMouseButton::MouseScrollUp:
		event.event = core::InputEventName::Scroll;
		event.point.valueX = 0.0f;
		event.point.valueY = 10.0f;
		break;
	case core::InputMouseButton::MouseScrollDown:
		event.event = core::InputEventName::Scroll;
		event.point.valueX = 0.0f;
		event.point.valueY = -10.0f;
		break;
	case core::InputMouseButton::MouseScrollLeft:
		event.event = core::InputEventName::Scroll;
		event.point.valueX = 10.0f;
		event.point.valueY = 0.0f;
		break;
	case core::InputMouseButton::MouseScrollRight:
		event.event = core::InputEventName::Scroll;
		event.point.valueX = -10.0f;
		event.point.valueY = 0.0f;
		break;
	default: break;
	}

	_pendingEvents.emplace_back(event);
}

void XcbWindow::handleButtonRelease(xcb_button_release_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		_lastInputTime = ev->time;
	}

	auto ext = Extent2(_xinfo.rect.width, _xinfo.rect.height);
	auto mod = getModifiers(ev->state);
	auto btn = getButton(ev->detail);

	core::InputEventData event({ev->detail, core::InputEventName::End, btn, mod, float(ev->event_x),
		float(ext.height - ev->event_y)});

	switch (btn) {
	case core::InputMouseButton::MouseScrollUp:
	case core::InputMouseButton::MouseScrollDown:
	case core::InputMouseButton::MouseScrollLeft:
	case core::InputMouseButton::MouseScrollRight: break;
	default: _pendingEvents.emplace_back(event); break;
	}
}

void XcbWindow::handleMotionNotify(xcb_motion_notify_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		_lastInputTime = ev->time;
	}

	auto ext = Extent2(_xinfo.rect.width, _xinfo.rect.height);
	auto mod = getModifiers(ev->state);

	core::InputEventData event({maxOf<uint32_t>(), core::InputEventName::MouseMove,
		core::InputMouseButton::None, mod, float(ev->event_x), float(ext.height - ev->event_y)});

	_pendingEvents.emplace_back(event);
}

void XcbWindow::handleEnterNotify(xcb_enter_notify_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		_lastInputTime = ev->time;
	}

	auto ext = Extent2(_xinfo.rect.width, _xinfo.rect.height);
	_pendingEvents.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::PointerEnter,
			true, Vec2(float(ev->event_x), float(ext.height - ev->event_y))));
}

void XcbWindow::handleLeaveNotify(xcb_leave_notify_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		_lastInputTime = ev->time;
	}

	auto ext = Extent2(_xinfo.rect.width, _xinfo.rect.height);
	_pendingEvents.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::PointerEnter,
			false, Vec2(float(ev->event_x), float(ext.height - ev->event_y))));
}

void XcbWindow::handleFocusIn(xcb_focus_in_event_t *ev) {
	// xcb_focus_in_event_t *ev = (xcb_focus_in_event_t*) e;
	_pendingEvents.emplace_back(
			core::InputEventData::BoolEvent(core::InputEventName::FocusGain, true));
}

void XcbWindow::handleFocusOut(xcb_focus_out_event_t *ev) {
	_pendingEvents.emplace_back(
			core::InputEventData::BoolEvent(core::InputEventName::FocusGain, false));
}

void XcbWindow::handleKeyPress(xcb_key_press_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		_lastInputTime = ev->time;
	}

	auto mod = getModifiers(ev->state);
	auto ext = Extent2(_xinfo.rect.width, _xinfo.rect.height);

	// in case of key autorepeat, ev->time will match
	// just replace event name from previous InputEventName::KeyReleased to InputEventName::KeyRepeated
	if (!_pendingEvents.empty()
			&& _pendingEvents.back().event == core::InputEventName::KeyReleased) {
		auto &iev = _pendingEvents.back();
		if (iev.id == ev->time && iev.modifiers == mod && iev.x == float(ev->event_x)
				&& iev.y == float(ext.height - ev->event_y)
				&& iev.key.keysym == _connection->getKeysym(ev->detail, ev->state, false)) {
			iev.event = core::InputEventName::KeyRepeated;
			return;
		}
	}

	core::InputEventData event({ev->time, core::InputEventName::KeyPressed,
		core::InputMouseButton::None, mod, float(ev->event_x), float(ext.height - ev->event_y)});

	_connection->fillTextInputData(event, ev->detail, ev->state, isTextInputEnabled(), true);

	_pendingEvents.emplace_back(event);
}

void XcbWindow::handleKeyRelease(xcb_key_release_event_t *ev) {
	if (_lastInputTime != ev->time) {
		dispatchPendingEvents();
		_lastInputTime = ev->time;
	}

	auto mod = getModifiers(ev->state);
	auto ext = Extent2(_xinfo.rect.width, _xinfo.rect.height);

	core::InputEventData event({ev->time, core::InputEventName::KeyReleased,
		core::InputMouseButton::None, mod, float(ev->event_x), float(ext.height - ev->event_y)});

	_connection->fillTextInputData(event, ev->detail, ev->state, isTextInputEnabled(), false);

	_pendingEvents.emplace_back(event);
}

void XcbWindow::handleSyncRequest(xcb_timestamp_t syncTime, xcb_sync_int64_t value) {
	_lastSyncTime = syncTime;
	_xinfo.syncValue = value;
	_xinfo.syncFrameOrder = _frameOrder;
}

void XcbWindow::handleCloseRequest() { _controller->notifyWindowClosed(this); }

void XcbWindow::handleScreenChangeNotify(xcb_randr_notify_event_t *ev,
		NotNull<ScreenInfoData> screenInfo) {
	_screenInfo = screenInfo;

	core::InputEventData event =
			core::InputEventData::BoolEvent(core::InputEventName::ScreenUpdate, true);

	_pendingEvents.emplace_back(event);

	auto commonRate = _screenInfo->getCommonRate();
	if (_rate != commonRate) {
		_rate = commonRate;
	}
}

void XcbWindow::handleSettingsUpdated() {
	auto udpi = _connection->getUnscaledDpi();
	auto dpi = _connection->getDpi();

	if (udpi == 0) {
		udpi = 122'880;
	}

	auto d = float(dpi) / float(udpi);
	if (d != _density) {
		_density = float(dpi) / float(udpi);
		_controller->notifyWindowConstraintsChanged(this, false);
	}
}

void XcbWindow::dispatchPendingEvents() {
	if (!_pendingEvents.empty()) {
		_controller->notifyWindowInputEvents(this, sp::move(_pendingEvents));
	}
	_pendingEvents.clear();
}

uint64_t XcbWindow::getScreenFrameInterval() const { return 1'000'000'000 / _rate; }

void XcbWindow::mapWindow() {
	_connection->attachWindow(_xinfo.window, this);
	_xcb->xcb_map_window(_connection->getConnection(), _xinfo.window);
	_xcb->xcb_flush(_connection->getConnection());
}

void XcbWindow::unmapWindow() {
	_xcb->xcb_unmap_window(_connection->getConnection(), _xinfo.window);
	_xcb->xcb_flush(_connection->getConnection());
	_connection->detachWindow(_xinfo.window);
}

void XcbWindow::handleFramePresented(NotNull<core::PresentationFrame> frame) {
	if (_xinfo.syncCounter && (_xinfo.syncValue.lo != 0 || _xinfo.syncValue.hi != 0)
			&& frame->getFrameOrder() > _xinfo.syncFrameOrder) {
		_xcb->xcb_sync_set_counter(_connection->getConnection(), _xinfo.syncCounter,
				_xinfo.syncValue);
		_xcb->xcb_flush(_connection->getConnection());

		_xinfo.syncValue.lo = 0;
		_xinfo.syncValue.hi = 0;
	}
}

xcb_connection_t *XcbWindow::getConnection() const { return _connection->getConnection(); }

core::FrameConstraints XcbWindow::exportConstraints(core::FrameConstraints &&c) const {
	c.extent = Extent3(_xinfo.rect.width, _xinfo.rect.height, 1);
	if (c.density == 0.0f) {
		c.density = 1.0f;
	}
	if (_density != 0.0f) {
		c.density *= _density;
	}
	c.frameInterval = getScreenFrameInterval();
	return move(c);
}

void XcbWindow::handleLayerUpdate(const WindowLayer &layer) {
	uint32_t cursorId = _connection->loadCursor("left_ptr");
	switch (layer.flags & WindowLayerFlags::CursorMask) {
	case WindowLayerFlags::CursorText: cursorId = _connection->loadCursor({"text", "xterm"}); break;
	case WindowLayerFlags::CursorPointer:
		cursorId = _connection->loadCursor({"hand2", "hand", "pointer"});
		break;
	case WindowLayerFlags::CursorHelp:
		cursorId = _connection->loadCursor({"help", "question_arrow", "whats_this"});
		break;
	case WindowLayerFlags::CursorProgress:
		cursorId = _connection->loadCursor({"progress", "left_ptr_watch", "half-busy"});
		break;
	case WindowLayerFlags::CursorWait: cursorId = _connection->loadCursor({"wait", "watch"}); break;
	case WindowLayerFlags::CursorCopy: cursorId = _connection->loadCursor({"copy"}); break;
	case WindowLayerFlags::CursorAlias:
		cursorId = _connection->loadCursor({"alias", "dnd-link"});
		break;
	case WindowLayerFlags::CursorNoDrop:
		cursorId = _connection->loadCursor({"no-drop", "forbidden"});
		break;
	case WindowLayerFlags::CursorNotAllowed:
		cursorId = _connection->loadCursor({"not-allowed", "crossed_circle"});
		break;
	case WindowLayerFlags::CursorAllScroll:
		cursorId = _connection->loadCursor({"all-scroll"});
		break;
	case WindowLayerFlags::CursorRowResize:
		cursorId = _connection->loadCursor({"row-resize"});
		break;
	case WindowLayerFlags::CursorColResize:
		cursorId = _connection->loadCursor({"col-resize"});
		break;
	default: break;
	}

	if (cursorId == XCB_CURSOR_NONE) {
		cursorId = _connection->loadCursor("left_ptr");
	}

	if (_xinfo.cursorId != cursorId) {
		_connection->setCursorId(_xinfo.window, cursorId);
		_xinfo.cursorId = cursorId;
	}
}

Extent2 XcbWindow::getExtent() const { return Extent2(_xinfo.rect.width, _xinfo.rect.height); }

Rc<core::Surface> XcbWindow::makeSurface(NotNull<core::Instance> cinstance) {
#if MODULE_XENOLITH_BACKEND_VK
	if (cinstance->getApi() != core::InstanceApi::Vulkan) {
		return nullptr;
	}

	auto instance = static_cast<vk::Instance *>(cinstance.get());
	auto connection = getConnection();
	auto window = getWindow();

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkXcbSurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR, nullptr, 0,
		connection, window};
	if (instance->vkCreateXcbSurfaceKHR(instance->getInstance(), &createInfo, nullptr, &surface)
			!= VK_SUCCESS) {
		return nullptr;
	}
	return Rc<vk::Surface>::create(instance, surface, this);
#else
	log::error("XcbWindow", "No available GAPI found for a surface");
	return nullptr;
#endif
}

bool XcbWindow::close() {
	if (!_xinfo.closed) {
		_xinfo.closed = true;
		if (!_controller->notifyWindowClosed(this)) {
			_xinfo.closed = false;
		}
		return true;
	}
	return false;
}

Status XcbWindow::setFullscreen(const MonitorId &id, const xenolith::ModeInfo &info) {
	auto switchFullscreen = [&] {
		auto screenInfo = _connection->getScreenInfo(_connection->getDefaultScreen()->root);
		auto newMon = getMonitor(screenInfo, id);
		auto currentMon = getMonitor(screenInfo, _info->monitor);

		if (id != MonitorId::None && !newMon) {
			log::info("XcbWindow", "Monitor ", id.name, " not available");
			return Status::ErrorInvalidArguemnt;
		}

		if (newMon == currentMon) {
			if (currentMon) {
				return setMode(screenInfo, newMon, info);
			}
		}

		if (newMon == nullptr) {
			// exit fullscreen

			// restore original mode
			if (currentMon) {
				auto it = _capturedModes.find(*currentMon);
				if (it != _capturedModes.end()) {
					setMode(screenInfo, currentMon, it->second);
					_capturedModes.erase(it);
				}
			}

			// remove fullscreen state
			if (hasFlag(_state, Fullscreen)) {
				_xcb->xcb_delete_property(_connection->getConnection(), _xinfo.window,
						_connection->getAtom(XcbAtomIndex::_NET_WM_FULLSCREEN_MONITORS));

				xcb_client_message_event_t fullscreen;
				fullscreen.response_type = XCB_CLIENT_MESSAGE;
				fullscreen.format = 32;
				fullscreen.sequence = 0;
				fullscreen.window = _xinfo.window;
				fullscreen.type = _connection->getAtom(XcbAtomIndex::_NET_WM_STATE);
				fullscreen.data.data32[0] = 0; // _NET_WM_STATE_REMOVE
				fullscreen.data.data32[1] =
						_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_FULLSCREEN);
				fullscreen.data.data32[2] = 0;
				fullscreen.data.data32[3] = 1; // EWMH says 1 for normal applications
				fullscreen.data.data32[4] = 0;
				_xcb->xcb_send_event(_connection->getConnection(), 0,
						_connection->getDefaultScreen()->root,
						XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
						(const char *)&fullscreen);
				_xcb->xcb_flush(_connection->getConnection());

				if (hasFlag(_info->flags, WindowFlags::ExclusiveFullscreen)) {
					auto a = _connection->getAtom(XcbAtomIndex::_NET_WM_BYPASS_COMPOSITOR);
					if (a) {
						_xcb->xcb_delete_property(_connection->getConnection(), _xinfo.window, a);
					}
				}

				_info->monitor = id;

				if constexpr (SetPrimaryOnFullscreen) {
					if (_originalPrimary != MonitorId::None) {
						auto primaryMon = getMonitor(screenInfo, _originalPrimary);
						if (primaryMon) {
							_xcb->xcb_randr_set_output_primary(_connection->getConnection(),
									_connection->getDefaultScreen()->root,
									primaryMon->outputs.front()->output);
							_originalPrimary = MonitorId::None;
						}
					}
				}

				return Status::Ok;
			}

			log::info("XcbWindow", "Window not in fullscreen mode");
			return Status::ErrorInvalidArguemnt;
		} else {
			// check if mode is available
			if (!checkIfModeAvailable(newMon, info)) {
				log::info("XcbWindow", "Monitor ", newMon->name, " has no mode ", info.width, "x",
						info.height, "@", info.rate);
				return Status::ErrorNotSupported;
			}

			// check if we have saved mode, if we have not - save it
			auto it = _capturedModes.find(*newMon);
			if (it == _capturedModes.end()) {
				_capturedModes.emplace(*newMon, *newMon->outputs.front()->crtc->mode);
			}

			if constexpr (SetPrimaryOnFullscreen) {
				if (_originalPrimary == MonitorId::None && screenInfo->primary) {
					_originalPrimary = *screenInfo->primary;
				}

				if (newMon != screenInfo->primary) {
					_xcb->xcb_randr_set_output_primary(_connection->getConnection(),
							_connection->getDefaultScreen()->root, newMon->outputs.front()->output);
					_xcb->xcb_flush(_connection->getConnection());
				}
			}

			auto ret = setMode(screenInfo, newMon, info);

			if (ret == Status::Ok || ret == Status::Done) {
				xcb_client_message_event_t monitors;
				monitors.response_type = XCB_CLIENT_MESSAGE;
				monitors.format = 32;
				monitors.sequence = 0;
				monitors.window = _xinfo.window;
				monitors.type = _connection->getAtom(XcbAtomIndex::_NET_WM_FULLSCREEN_MONITORS);
				monitors.data.data32[0] = 0; //newMon->index;
				monitors.data.data32[1] = 0; //newMon->index;
				monitors.data.data32[2] = 0; //newMon->index;
				monitors.data.data32[3] = 0; //newMon->index;
				monitors.data.data32[4] = 1; // EWMH says 1 for normal applications
				_xcb->xcb_send_event(_connection->getConnection(), 0,
						_connection->getDefaultScreen()->root,
						XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
						(const char *)&monitors);

				if (!hasFlag(_state, Fullscreen)) {
					xcb_client_message_event_t fullscreen;
					fullscreen.response_type = XCB_CLIENT_MESSAGE;
					fullscreen.format = 32;
					fullscreen.sequence = 0;
					fullscreen.window = _xinfo.window;
					fullscreen.type = _connection->getAtom(XcbAtomIndex::_NET_WM_STATE);
					fullscreen.data.data32[0] = 1; // _NET_WM_STATE_ADD
					fullscreen.data.data32[1] =
							_connection->getAtom(XcbAtomIndex::_NET_WM_STATE_FULLSCREEN);
					fullscreen.data.data32[2] = 0;
					fullscreen.data.data32[3] = 1; // EWMH says 1 for normal applications
					fullscreen.data.data32[4] = 0;
					_xcb->xcb_send_event(_connection->getConnection(), 0,
							_connection->getDefaultScreen()->root,
							XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
							(const char *)&fullscreen);
				}

				const unsigned long value = 1;
				if (hasFlag(_info->flags, WindowFlags::ExclusiveFullscreen)) {
					auto a = _connection->getAtom(XcbAtomIndex::_NET_WM_BYPASS_COMPOSITOR);
					if (a) {
						_xcb->xcb_change_property(_connection->getConnection(),
								XCB_PROP_MODE_REPLACE, _xinfo.window, a, XCB_ATOM_CARDINAL, 32, 1,
								&value);
					}
				}

				_xcb->xcb_flush(_connection->getConnection());
				_info->monitor = id;

				if (currentMon) {
					// check if we have mode to restore on prev monitor
					auto prevModeIt = _capturedModes.find(*currentMon);
					if (prevModeIt != _capturedModes.end()) {
						setMode(screenInfo, currentMon, prevModeIt->second);
						_capturedModes.erase(prevModeIt);
					}
				}
			}

			return Status::Ok;
		}

		return Status::ErrorNotSupported;
	};

	Status ret = Status::ErrorCancelled;

	// grab server to receive only actual status
	// All other clients will receive notifications after we done
	//_xcb->xcb_grab_server(_connection->getConnection());
	_xcb->xcb_flush(_connection->getConnection());
	std::cout << "xcb_grab_server\n";

	ret = switchFullscreen();

	std::cout << "xcb_ungrab_server\n";
	//_xcb->xcb_ungrab_server(_connection->getConnection());
	_xcb->xcb_flush(_connection->getConnection());
	std::cout << "done\n";
	return ret;
}

bool XcbWindow::updateTextInput(const TextInputRequest &, TextInputFlags flags) { return true; }

void XcbWindow::cancelTextInput() { }

void XcbWindow::updateWindowAttributes() {
	uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
	uint32_t values[2];
	values[0] = _xinfo.overrideRedirect;
	values[1] = _xinfo.eventMask;

	_xcb->xcb_change_window_attributes(_connection->getConnection(), _xinfo.window, mask, values);
}

void XcbWindow::configureWindow(xcb_rectangle_t r, uint16_t border_width) {
	const uint32_t values[] = {static_cast<uint32_t>(r.x), static_cast<uint32_t>(r.y),
		static_cast<uint32_t>(r.width), static_cast<uint32_t>(r.height), border_width};

	_xcb->xcb_configure_window(_connection->getConnection(), _xinfo.window,
			XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH
					| XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH,
			values);
	_xcb->xcb_flush(_connection->getConnection());
}

const MonitorInfo *XcbWindow::getMonitor(NotNull<ScreenInfoData> info, const MonitorId &id) const {
	const MonitorInfo *mon = nullptr;
	if (id == MonitorId::None) {
		return nullptr;
	} else if (id == MonitorId::Primary) {
		mon = info->primary;
	} else {
		for (auto &it : info->monitors) {
			if (it.isMatch(id)) {
				mon = &it;
				break;
			}
		}
	}
	return mon;
}

Status XcbWindow::setMode(NotNull<ScreenInfoData> info, const MonitorInfo *mon,
		const xenolith::ModeInfo &mode) {

	log::debug("XcbWindow", mon->name, ": set mode: ", mode.width, "x", mode.height, "@",
			mode.rate);

	auto output = mon->outputs.front();
	auto crtc = output->crtc;

	for (auto &it : output->modes) {
		if (*it == mode) {
			auto crtcCookie = _xcb->xcb_randr_get_crtc_info(_connection->getConnection(),
					crtc->crtc, info->config);
			auto crtcReply = _connection->perform(_xcb->xcb_randr_get_crtc_info_reply, crtcCookie);
			if (crtcReply) {
				// re-check if we need to reset mode
				if (crtcReply->mode == it->id) {
					log::debug("XcbWindow", mon->name, ": no need to switch");
					return Status::Done; //  no need to switch
				}

				// First - disable CRTC (or WM will fail to increse resolution or framerate)
				auto cookieDisable = _xcb->xcb_randr_set_crtc_config(_connection->getConnection(),
						crtc->crtc, XCB_CURRENT_TIME, XCB_CURRENT_TIME, 0, 0, 0,
						crtcReply->rotation, 0, nullptr);

				auto replyDisable =
						_connection->perform(_xcb->xcb_randr_set_crtc_config_reply, cookieDisable);
				if (replyDisable) {
					log::debug("XcbWindow", mon->name, ": disabled");
					// if CRTC was disabled - re-enable it with the new mode
					auto cookie = _xcb->xcb_randr_set_crtc_config(_connection->getConnection(),
							crtc->crtc, XCB_CURRENT_TIME, XCB_CURRENT_TIME, crtcReply->x,
							crtcReply->y, it->id, crtcReply->rotation,
							_xcb->xcb_randr_get_crtc_info_outputs_length(crtcReply),
							_xcb->xcb_randr_get_crtc_info_outputs(crtcReply));

					auto reply =
							_connection->perform(_xcb->xcb_randr_set_crtc_config_reply, cookie);
					if (reply) {
						log::debug("XcbWindow", mon->name, ": enabled");
						//  Complete!
						return Status::Ok;
					}
				}
			}
		}
	}

	return Status::ErrorCancelled;
}

bool XcbWindow::checkIfModeAvailable(const MonitorInfo *mon, const xenolith::ModeInfo &mode) const {
	if (mode == xenolith::ModeInfo::Current) {
		return true;
	}

	if (mode == xenolith::ModeInfo::Preferred) {
		// check if we ACTUALLY have this mode
		if (mon->outputs.front()->preferred) {
			return true;
		}
	}

	auto output = mon->outputs.front();
	for (auto &it : output->modes) {
		if (*it == mode) {
			return true;
		}
	}
	return false;
}

const CallbackStream &operator<<(const CallbackStream &out, XcbWindow::StateFlags flags) {
	if (hasFlag(flags, XcbWindow::StateFlags::Modal)) {
		out << " Modal";
	}
	if (hasFlag(flags, XcbWindow::StateFlags::Sticky)) {
		out << " Sticky";
	}
	if (hasFlag(flags, XcbWindow::StateFlags::MaximizedVert)) {
		out << " MaximizedVert";
	}
	if (hasFlag(flags, XcbWindow::StateFlags::MaximizedHorz)) {
		out << " MaximizedHorz";
	}
	if (hasFlag(flags, XcbWindow::StateFlags::Shaded)) {
		out << " Shaded";
	}
	if (hasFlag(flags, XcbWindow::StateFlags::SkipTaskbar)) {
		out << " SkipTaskbar";
	}
	if (hasFlag(flags, XcbWindow::StateFlags::SkipPager)) {
		out << " SkipPager";
	}
	if (hasFlag(flags, XcbWindow::StateFlags::Hidden)) {
		out << " Hidden";
	}
	if (hasFlag(flags, XcbWindow::StateFlags::Fullscreen)) {
		out << " Fullscreen";
	}
	if (hasFlag(flags, XcbWindow::StateFlags::Above)) {
		out << " Above";
	}
	if (hasFlag(flags, XcbWindow::StateFlags::Below)) {
		out << " Below";
	}
	if (hasFlag(flags, XcbWindow::StateFlags::DemandsAttention)) {
		out << " DemandsAttention";
	}
	if (hasFlag(flags, XcbWindow::StateFlags::Focused)) {
		out << " Focused";
	}
	return out;
}

} // namespace stappler::xenolith::platform
