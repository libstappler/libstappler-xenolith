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

#include "XLPlatformLinuxWaylandView.h"

#if XL_ENABLE_WAYLAND

#include <linux/input.h>

#ifndef XL_WAYLAND_LOG
#if XL_WAYLAND_DEBUG
#define XL_WAYLAND_LOG(...) log::debug("Wayland", __VA_ARGS__)
#else
#define XL_WAYLAND_LOG(...)
#endif
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static struct wl_surface_listener s_WaylandSurfaceListener{
	[] (void *data, wl_surface *surface, wl_output *output) {
		((WaylandView *)data)->handleSurfaceEnter(surface, output);
	},
	[] (void *data, wl_surface *surface, wl_output *output) {
		((WaylandView *)data)->handleSurfaceLeave(surface, output);
	},
};

static const wl_callback_listener s_WaylandSurfaceFrameListener{
	[] (void *data, wl_callback *wl_callback, uint32_t callback_data) {
		((WaylandView *)data)->handleSurfaceFrameDone(wl_callback, callback_data);
	},
};

static xdg_surface_listener const s_XdgSurfaceListener{
	[] (void *data, xdg_surface *xdg_surface, uint32_t serial) {
		((WaylandView *)data)->handleSurfaceConfigure(xdg_surface, serial);
	},
};

static const xdg_toplevel_listener s_XdgToplevelListener{
	[] (void *data, xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, wl_array *states) {
		((WaylandView *)data)->handleToplevelConfigure(xdg_toplevel, width, height, states);
	},
	[] (void* data, struct xdg_toplevel* xdg_toplevel) {
		((WaylandView *)data)->handleToplevelClose(xdg_toplevel);
	},
	[] (void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {
		((WaylandView *)data)->handleToplevelBounds(xdg_toplevel, width, height);
	}
};

WaylandView::WaylandView(WaylandLibrary *lib, ViewInterface *view, StringView name, StringView bundleName, URect rect) {
	_display = Rc<WaylandDisplay>::create(lib);

	_view = view;
	_currentExtent = Extent2(rect.width, rect.height);

	_surface = _display->createSurface(this);
	if (_surface) {
		_display->wayland->wl_surface_set_user_data(_surface, this);
		_display->wayland->wl_surface_add_listener(_surface, &s_WaylandSurfaceListener, this);

		auto region = _display->wayland->wl_compositor_create_region(_display->compositor);
		_display->wayland->wl_region_add(region, 0, 0, _currentExtent.width, _currentExtent.height);
		_display->wayland->wl_surface_set_opaque_region(_surface, region);

		_xdgSurface = _display->wayland->xdg_wm_base_get_xdg_surface(_display->xdgWmBase, _surface);

		_display->wayland->xdg_surface_add_listener(_xdgSurface, &s_XdgSurfaceListener, this);
		_toplevel = _display->wayland->xdg_surface_get_toplevel(_xdgSurface);
		_display->wayland->xdg_toplevel_set_title(_toplevel, name.data());
		_display->wayland->xdg_toplevel_set_app_id(_toplevel, bundleName.data());
		_display->wayland->xdg_toplevel_add_listener(_toplevel, &s_XdgToplevelListener, this);

		if (_clientSizeDecoration) {
			createDecorations();
		}

		_display->wayland->wl_surface_commit(_surface);
		_display->wayland->wl_region_destroy(region);
	}

	uint32_t rate = 60000;
	for (auto &it : _display->outputs) {
		rate = std::max(rate, uint32_t(it->mode.refresh));
	}
	_screenFrameInterval = 1'000'000'000ULL / rate;
}

WaylandView::~WaylandView() {
	_iconMaximized = nullptr;
	_decors.clear();
	if (_toplevel) {
		_display->wayland->xdg_toplevel_destroy(_toplevel);
		_toplevel = nullptr;
	}
	if (_xdgSurface) {
		_display->wayland->xdg_surface_destroy(_xdgSurface);
		_xdgSurface = nullptr;
	}
	if (_surface) {
		_display->destroySurface(_surface);
		_surface = nullptr;
	}
	_display = nullptr;
}

bool WaylandView::poll(bool frameReady) {
	if (_shouldClose) {
		return false;
	}

	if (_display->seatDirty) {
		_display->seat->update();
	}

	if (frameReady && ((_continuousRendering && _state.test(XDG_TOPLEVEL_STATE_ACTIVATED))  || _scheduleNext)) {
		auto frame = _display->wayland->wl_surface_frame(_surface);
		_display->wayland->wl_callback_add_listener(frame, &s_WaylandSurfaceFrameListener, this);
		_display->wayland->wl_surface_commit(_surface);
		_scheduleNext = false;
	}

	_display->flush();

	if (!_shouldClose) {
		if (!_keys.empty()) {
			handleKeyRepeat();
		}
	}

	return !_shouldClose;
}

int WaylandView::getSocketFd() const {
	return _display->getSocketFd();
}

uint64_t WaylandView::getScreenFrameInterval() const {
	// On Wayland, limit on full interval causes vblank miss due Mailbox implementation, so, limit on half-interval
	// Mailbox do appropriate sync even without specified frame interval, it's just a little help for engine
	return _screenFrameInterval /*/ 2*/;
}

void WaylandView::mapWindow() {
	_display->flush();
}

void WaylandView::handleSurfaceEnter(wl_surface *surface, wl_output *output) {
	if (!_display->wayland->ownsProxy(output)) {
		return;
	}

	auto out = (WaylandOutput *)_display->wayland->wl_output_get_user_data(output);
	if (out) {
		_activeOutputs.emplace(out);
		XL_WAYLAND_LOG("handleSurfaceEnter: output: ", out->description());
	}
}

void WaylandView::handleSurfaceLeave(wl_surface *surface, wl_output *output) {
	if (!_display->wayland->ownsProxy(output)) {
		return;
	}

	auto out = (WaylandOutput *)_display->wayland->wl_output_get_user_data(output);
	if (out) {
		_activeOutputs.erase(out);
		XL_WAYLAND_LOG("handleSurfaceLeave: output: ", out->description());
	}
}

void WaylandView::handleSurfaceConfigure(xdg_surface *surface, uint32_t serial) {
	XL_WAYLAND_LOG("handleSurfaceConfigure: serial: ", serial);
	_configureSerial = serial;
}

void WaylandView::handleToplevelConfigure(xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, wl_array *states) {
	StringStream stream;
	stream << "handleToplevelConfigure: width: " << width << ", height: " << height << ";";

	auto oldState = _state;
	_state.reset();

	for (uint32_t *it = (uint32_t *)states->data; (const char *)it < ((const char *) states->data + states->size); ++ it) {
		_state.set(*it);
		switch (*it) {
		case XDG_TOPLEVEL_STATE_MAXIMIZED: stream << " MAXIMIZED;"; break;
		case XDG_TOPLEVEL_STATE_FULLSCREEN: stream << " FULLSCREEN;"; break;
		case XDG_TOPLEVEL_STATE_RESIZING: stream << " RESIZING;"; break;
		case XDG_TOPLEVEL_STATE_ACTIVATED: stream << " ACTIVATED;"; break;
		case XDG_TOPLEVEL_STATE_TILED_LEFT: stream << " TILED_LEFT;"; break;
		case XDG_TOPLEVEL_STATE_TILED_RIGHT: stream << " TILED_RIGHT;"; break;
		case XDG_TOPLEVEL_STATE_TILED_TOP: stream << " TILED_TOP;"; break;
		case XDG_TOPLEVEL_STATE_TILED_BOTTOM: stream << " TILED_BOTTOM;"; break;
		}
	}

	if (_state.test(XDG_TOPLEVEL_STATE_ACTIVATED) != oldState.test(XDG_TOPLEVEL_STATE_ACTIVATED)) {
		_view->handleInputEvent(core::InputEventData::BoolEvent(core::InputEventName::FocusGain, _state.test(XDG_TOPLEVEL_STATE_ACTIVATED)));
	}

	if (width && height) {
		if (_currentExtent.width != uint32_t(width) || _currentExtent.height != uint32_t(height)) {
			_currentExtent.width = width;
			_currentExtent.height = height - DecorOffset - DecorInset;
			_view->deprecateSwapchain();

			stream << "surface: " << _currentExtent.width << " " << _currentExtent.height;
		}
	}

	auto checkVisible = [&, this] (WaylandDecorationName name) {
		switch (name) {
		case WaylandDecorationName::RightSide:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) { return false; }
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_RIGHT)) { return false; }
			break;
		case WaylandDecorationName::TopRigntCorner:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) { return false; }
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_TOP) && _state.test(XDG_TOPLEVEL_STATE_TILED_RIGHT)) { return false; }
			break;
		case WaylandDecorationName::TopSide:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) { return false; }
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_TOP)) { return false; }
			break;
		case WaylandDecorationName::TopLeftCorner:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) { return false; }
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_TOP) && _state.test(XDG_TOPLEVEL_STATE_TILED_LEFT)) { return false; }
			break;
		case WaylandDecorationName::BottomRightCorner:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) { return false; }
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_BOTTOM) && _state.test(XDG_TOPLEVEL_STATE_TILED_RIGHT)) { return false; }
			break;
		case WaylandDecorationName::BottomSide:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) { return false; }
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_BOTTOM)) { return false; }
			break;
		case WaylandDecorationName::BottomLeftCorner:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) { return false; }
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_BOTTOM) && _state.test(XDG_TOPLEVEL_STATE_TILED_LEFT)) { return false; }
			break;
		case WaylandDecorationName::LeftSide:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) { return false; }
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_LEFT)) { return false; }
			break;
		default:
			break;
		}
		return true;
	};

	for (auto &it : _decors) {
		it->setActive(_state.test(XDG_TOPLEVEL_STATE_ACTIVATED));
		it->setVisible(checkVisible(it->name));
	}

	XL_WAYLAND_LOG(stream.str());
}

void WaylandView::handleToplevelClose(xdg_toplevel *xdg_toplevel) {
	XL_WAYLAND_LOG("handleToplevelClose");
	_shouldClose = true;
}

void WaylandView::handleToplevelBounds(xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {
	XL_WAYLAND_LOG("handleToplevelBounds: width: ", width, ", height: ", height);
}

void WaylandView::handleSurfaceFrameDone(wl_callback *frame, uint32_t data) {
	_display->wayland->wl_callback_destroy(frame);
}

void WaylandView::handlePointerEnter(wl_fixed_t surface_x, wl_fixed_t surface_y) {
	if (!_pointerInit || _display->seat->hasPointerFrames) {
		auto &ev = _pointerEvents.emplace_back(PointerEvent{ PointerEvent::Enter });
		ev.enter.x = surface_x;
		ev.enter.y = surface_y;
	} else {
		_view->handleInputEvent(core::InputEventData::BoolEvent(core::InputEventName::PointerEnter, true,
				Vec2(float(wl_fixed_to_double(surface_x)), float(_currentExtent.height - wl_fixed_to_double(surface_y)))));

		_surfaceX = wl_fixed_to_double(surface_x);
		_surfaceY = wl_fixed_to_double(surface_y);
	}

	XL_WAYLAND_LOG("handlePointerEnter: x: ", wl_fixed_to_int(surface_x), ", y: ", wl_fixed_to_int(surface_y));
}

void WaylandView::handlePointerLeave() {
	if (!_pointerInit) {
		_pointerInit = true;
		if (!_display->seat->hasPointerFrames) {
			handlePointerFrame();
		}
	}

	handlePointerFrame(); // drop pending events
	_view->handleInputEvent(core::InputEventData::BoolEvent(core::InputEventName::PointerEnter, false,
			Vec2(float(_surfaceX), float(_currentExtent.height - _surfaceY))));
}

void WaylandView::handlePointerMotion(uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	// XL_WAYLAND_LOG("handlePointerMotion: x: ", wl_fixed_to_int(surface_x), ", y: ", wl_fixed_to_int(surface_y));

	if (!_pointerInit) {
		_pointerInit = true;
		if (!_display->seat->hasPointerFrames) {
			handlePointerFrame();
		}
	}

	if (_display->seat->hasPointerFrames) {
		auto &ev = _pointerEvents.emplace_back(PointerEvent{ PointerEvent::Motion });
		ev.motion.time = time;
		ev.motion.x = surface_x;
		ev.motion.y = surface_y;
	} else {
		_view->handleInputEvent(core::InputEventData({
			maxOf<uint32_t>(),
			core::InputEventName::MouseMove,
			core::InputMouseButton::None,
			_activeModifiers,
			float(wl_fixed_to_double(surface_x)),
			float(_currentExtent.height - wl_fixed_to_double(surface_y))
		}));

		_surfaceX = wl_fixed_to_double(surface_x);
		_surfaceY = wl_fixed_to_double(surface_y);
	}
}

static core::InputMouseButton getButton(uint32_t button) {
	switch (button) {
	case BTN_LEFT: return core::InputMouseButton::MouseLeft; break;
	case BTN_RIGHT: return core::InputMouseButton::MouseRight; break;
	case BTN_MIDDLE: return core::InputMouseButton::MouseMiddle; break;
	default:
		return core::InputMouseButton(toInt(core::InputMouseButton::Mouse8) + (button - 0x113));
		break;
	}
	return core::InputMouseButton::None;
}

void WaylandView::handlePointerButton(uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
	if (!_pointerInit) {
		return;
	}

	XL_WAYLAND_LOG("handlePointerButton");
	if (_display->seat->hasPointerFrames) {
		auto &ev = _pointerEvents.emplace_back(PointerEvent{ PointerEvent::Button });
		ev.button.serial = serial;
		ev.button.time = time;
		ev.button.button = button;
		ev.button.state = state;
	} else {
		_view->handleInputEvent(core::InputEventData({
			button,
			((state == WL_POINTER_BUTTON_STATE_PRESSED) ? core::InputEventName::Begin : core::InputEventName::End),
			getButton(button),
			_activeModifiers,
			float(_surfaceX),
			float(_currentExtent.height - _surfaceY)
		}));
	}
}

void WaylandView::handlePointerAxis(uint32_t time, uint32_t axis, wl_fixed_t value) {
	if (!_pointerInit) {
		return;
	}

	if (_display->seat->hasPointerFrames) {
		auto &ev = _pointerEvents.emplace_back(PointerEvent{ PointerEvent::Axis });
		ev.axis.time = time;
		ev.axis.axis = axis;
		ev.axis.value = value;
	} else {
		core::InputMouseButton btn = core::InputMouseButton::None;
		auto val = wl_fixed_to_int(value);
		switch (axis) {
		case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
			if (val < 0) {
				btn = core::InputMouseButton::MouseScrollUp;
			} else {
				btn = core::InputMouseButton::MouseScrollDown;
			}
			break;
		case WL_POINTER_AXIS_VERTICAL_SCROLL:
			if (val > 0) {
				btn = core::InputMouseButton::MouseScrollRight;
			} else {
				btn = core::InputMouseButton::MouseScrollLeft;
			}
			break;
		}

		core::InputEventData event({
			toInt(btn),
			core::InputEventName::Scroll,
			btn,
			_activeModifiers,
			float(_surfaceX),
			float(_currentExtent.height - _surfaceY)
		});

		switch (axis) {
		case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
			event.point.valueX = float(wl_fixed_to_double(value));
			event.point.valueY = 0.0f;
			break;
		case WL_POINTER_AXIS_VERTICAL_SCROLL:
			event.point.valueX = 0.0f;
			event.point.valueY = -float(wl_fixed_to_double(value));
			break;
		}

		_view->handleInputEvent(event);
	}
}

void WaylandView::handlePointerAxisSource(uint32_t axis_source) {
	if (!_pointerInit) {
		return;
	}

	auto &ev = _pointerEvents.emplace_back(PointerEvent{ PointerEvent::AxisSource });
	ev.axisSource.axis_source = axis_source;
}

void WaylandView::handlePointerAxisStop(uint32_t time, uint32_t axis) {
	if (!_pointerInit) {
		return;
	}

	auto &ev = _pointerEvents.emplace_back(PointerEvent{ PointerEvent::AxisStop });
	ev.axisStop.time = time;
	ev.axisStop.axis = axis;
}

void WaylandView::handlePointerAxisDiscrete(uint32_t axis, int32_t discrete) {
	if (!_pointerInit) {
		return;
	}

	auto &ev = _pointerEvents.emplace_back(PointerEvent{ PointerEvent::AxisDiscrete });
	ev.axisDiscrete.axis = axis;
	ev.axisDiscrete.discrete = discrete;
}

void WaylandView::handlePointerFrame() {
	if (!_pointerInit || _pointerEvents.empty()) {
		return;
	}

	Vector<core::InputEventData> inputEvents;

	bool positionChanged = false;
	double x = 0.0f;
	double y = 0.0f;

	core::InputMouseButton axisBtn = core::InputMouseButton::None;
	uint32_t axisSource = 0;
	bool hasAxis = false;
	double axisX = 0.0f;
	double axisY = 0.0f;

	for (auto &it : _pointerEvents) {
		switch (it.event) {
		case PointerEvent::None: break;
		case PointerEvent::Enter:
			inputEvents.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::PointerEnter, true,
					Vec2(float(wl_fixed_to_double(it.enter.x)), float(_currentExtent.height - wl_fixed_to_double(it.enter.y)))));
			positionChanged = true;
			x = wl_fixed_to_double(it.enter.x);
			y = wl_fixed_to_double(it.enter.y);
			break;
		case PointerEvent::Leave:
			break;
		case PointerEvent::Motion:
			positionChanged = true;
			x = wl_fixed_to_double(it.motion.x);
			y = wl_fixed_to_double(it.motion.y);
			break;
		case PointerEvent::Button:
			break;
		case PointerEvent::Axis:
			switch (it.axis.axis) {
			case WL_POINTER_AXIS_VERTICAL_SCROLL:
				hasAxis = true;
				axisY -= wl_fixed_to_double(it.axis.value);
				if (wl_fixed_to_int(it.axis.value) < 0) {
					axisBtn = core::InputMouseButton::MouseScrollUp;
				} else {
					axisBtn = core::InputMouseButton::MouseScrollDown;
				}
				break;
			case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
				hasAxis = true;
				axisX += wl_fixed_to_double(it.axis.value);
				if (wl_fixed_to_int(it.axis.value) > 0) {
					axisBtn = core::InputMouseButton::MouseScrollRight;
				} else {
					axisBtn = core::InputMouseButton::MouseScrollLeft;
				}
				break;
			default: break;
			}
			break;
		case PointerEvent::AxisSource:
			axisSource = it.axisSource.axis_source;
			break;
		case PointerEvent::AxisStop:
			break;
		case PointerEvent::AxisDiscrete:
			break;
		}
	}

	if (positionChanged) {
		inputEvents.emplace_back(core::InputEventData({
			maxOf<uint32_t>(),
			core::InputEventName::MouseMove,
			core::InputMouseButton::None,
			_activeModifiers,
			float(x),
			float(_currentExtent.height - y)
		}));

		_surfaceX = x;
		_surfaceY = y;
	}

	if (hasAxis) {
		auto &event = inputEvents.emplace_back(core::InputEventData({
			axisSource,
			core::InputEventName::Scroll,
			axisBtn,
			_activeModifiers,
			float(_surfaceX),
			float(_currentExtent.height - _surfaceY)
		}));

		event.point.valueX = float(axisX);
		event.point.valueY = float(axisY);
	}

	for (auto &it : _pointerEvents) {
		switch (it.event) {
		case PointerEvent::None: break;
		case PointerEvent::Enter: break;
		case PointerEvent::Leave:
			inputEvents.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::PointerEnter, false,
					Vec2(float(_surfaceX), float(_currentExtent.height - _surfaceY))));
			break;
		case PointerEvent::Motion: break;
		case PointerEvent::Button:
			inputEvents.emplace_back(core::InputEventData({
				it.button.button,
				((it.button.state == WL_POINTER_BUTTON_STATE_PRESSED) ? core::InputEventName::Begin : core::InputEventName::End),
				getButton(it.button.button),
				_activeModifiers,
				float(_surfaceX),
				float(_currentExtent.height - _surfaceY)
			}));
			break;
		case PointerEvent::Axis: break;
		case PointerEvent::AxisSource: break;
		case PointerEvent::AxisStop: break;
		case PointerEvent::AxisDiscrete: break;
		}
	}

	if (!inputEvents.empty()) {
		_view->handleInputEvents(move(inputEvents));
	}
	_pointerEvents.clear();
}

void WaylandView::handleKeyboardEnter(Vector<uint32_t> &&keys, uint32_t depressed, uint32_t latched, uint32_t locked) {
	handleKeyModifiers(depressed, latched, locked);
	uint32_t n = 1;
	for (auto &it : keys) {
		handleKey(n, it, WL_KEYBOARD_KEY_STATE_PRESSED);
		++ n;
	}
}

void WaylandView::handleKeyboardLeave() {
	Vector<core::InputEventData> events;
	uint32_t n = 1;
	for (auto &it : _keys) {
		core::InputEventData event({
			n,
			core::InputEventName::KeyCanceled,
			core::InputMouseButton::None,
			_activeModifiers,
			float(_surfaceX),
			float(_currentExtent.height - _surfaceY)
		});

		event.key.keycode = _display->seat->translateKey(it.second.scancode);
		event.key.keysym = it.second.scancode;
		event.key.keychar = it.second.codepoint;

		events.emplace_back(move(event));

		++ n;
	}

	if (!events.empty()) {
		_view->handleInputEvents(move(events));
	}
}

void WaylandView::handleKey(uint32_t time, uint32_t scancode, uint32_t state) {
	core::InputEventData event({
		time,
		(state == WL_KEYBOARD_KEY_STATE_PRESSED) ? core::InputEventName::KeyPressed : core::InputEventName::KeyReleased,
				core::InputMouseButton::None,
		_activeModifiers,
		float(_surfaceX),
		float(_currentExtent.height - _surfaceY)
	});

	event.key.keycode = _display->seat->translateKey(scancode);
	event.key.compose = core::InputKeyComposeState::Nothing;
	event.key.keysym = scancode;
	event.key.keychar = 0;

	const xkb_keysym_t *keysyms = nullptr;
	const xkb_keycode_t keycode = scancode + 8;

	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		char32_t codepoint = 0;
		if (_display->xkb && _view->isInputEnabled()) {
			if (_display->xkb->xkb_state_key_get_syms(_display->seat->state, keycode, &keysyms) == 1) {
				const xkb_keysym_t keysym = _display->seat->composeSymbol(keysyms[0], event.key.compose);
				const uint32_t cp = _display->xkb->xkb_keysym_to_utf32(keysym);
				if (cp != 0 && keysym != XKB_KEY_NoSymbol) {
					codepoint = cp;
				}
			}
		}

		auto it = _keys.emplace(scancode, KeyData{
			scancode,
			codepoint,
			platform::clock(core::ClockType::Monotonic),
			false
		}).first;

		if (_display->xkb && _display->xkb->xkb_keymap_key_repeats(
				_display->xkb->xkb_state_get_keymap(_display->seat->state), keycode)) {
			it->second.repeats = true;
		}
	} else {
		auto it = _keys.find(scancode);
		if (it == _keys.end()) {
			return;
		}

		event.key.keychar = it->second.codepoint;
		_keys.erase(it);
	}

	_view->handleInputEvent(move(event));
}

void WaylandView::handleKeyModifiers(uint32_t depressed, uint32_t latched, uint32_t locked) {
	if (!_display->seat->state) {
		return;
	}

	_activeModifiers = core::InputModifier::None;
	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
			_display->seat->keyState.controlIndex, XKB_STATE_MODS_EFFECTIVE) == 1) {
		_activeModifiers |= core::InputModifier::Ctrl;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
			_display->seat->keyState.altIndex, XKB_STATE_MODS_EFFECTIVE) == 1) {
		_activeModifiers |= core::InputModifier::Alt;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
			_display->seat->keyState.shiftIndex, XKB_STATE_MODS_EFFECTIVE) == 1) {
		_activeModifiers |= core::InputModifier::Shift;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
			_display->seat->keyState.superIndex, XKB_STATE_MODS_EFFECTIVE) == 1) {
		_activeModifiers |= core::InputModifier::Mod4;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
			_display->seat->keyState.capsLockIndex, XKB_STATE_MODS_EFFECTIVE) == 1) {
		_activeModifiers |= core::InputModifier::CapsLock;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
			_display->seat->keyState.numLockIndex, XKB_STATE_MODS_EFFECTIVE) == 1) {
		_activeModifiers |= core::InputModifier::NumLock;
	}
}

void WaylandView::handleKeyRepeat() {
	Vector<core::InputEventData> events;
	auto spawnRepeatEvent = [&, this] (const KeyData &it) {
		core::InputEventData event({
			uint32_t(events.size() + 1),
			core::InputEventName::KeyRepeated,
			core::InputMouseButton::None,
			_activeModifiers,
			float(_surfaceX),
			float(_currentExtent.height - _surfaceY)
		});

		event.key.keycode = _display->seat->translateKey(it.scancode);
		event.key.keysym = it.scancode;
		event.key.keychar = it.codepoint;

		events.emplace_back(move(event));
	};

	uint64_t repeatDelay = _display->seat->keyState.keyRepeatDelay;
	uint64_t repeatInterval = _display->seat->keyState.keyRepeatInterval;
	auto t = platform::clock(core::ClockType::Monotonic);
	for (auto &it : _keys) {
		if (it.second.repeats) {
			if (!it.second.lastRepeat) {
				auto dt = t - it.second.time;
				if (dt > repeatDelay * 1000) {
					dt -= repeatDelay * 1000;
					it.second.lastRepeat = t - dt;
				}
			}
			if (it.second.lastRepeat) {
				auto dt = t - it.second.lastRepeat;
				while (dt > repeatInterval) {
					spawnRepeatEvent(it.second);
					dt -= repeatInterval;
					it.second.lastRepeat += repeatInterval;
				}
			}
		}
	}

	if (!events.empty()) {
		_view->handleInputEvents(move(events));
	}
}

void WaylandView::handleDecorationPress(WaylandDecoration *decor, uint32_t serial, bool released) {
	auto switchMaximized = [&, this] {
		if (!_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) {
			_display->wayland->xdg_toplevel_set_maximized(_toplevel);
			_iconMaximized->setAlternative(true);
		} else {
			_display->wayland->xdg_toplevel_unset_maximized(_toplevel);
			_iconMaximized->setAlternative(false);
		}
	};
	switch (decor->name) {
	case WaylandDecorationName::IconClose:
		_shouldClose = true;
		return;
		break;
	case WaylandDecorationName::IconMaximize:
		switchMaximized();
		return;
		break;
	case WaylandDecorationName::IconMinimize:
		_display->wayland->xdg_toplevel_set_minimized(_toplevel);
		return;
	default:
		break;
	}
	uint32_t edges = 0;
	switch (decor->image) {
	case WaylandCursorImage::RightSide: edges = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT; break;
	case WaylandCursorImage::TopRigntCorner: edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT; break;
	case WaylandCursorImage::TopSide: edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP; break;
	case WaylandCursorImage::TopLeftCorner: edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT; break;
	case WaylandCursorImage::BottomRightCorner: edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT; break;
	case WaylandCursorImage::BottomSide: edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM; break;
	case WaylandCursorImage::BottomLeftCorner: edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT; break;
	case WaylandCursorImage::LeftSide: edges = XDG_TOPLEVEL_RESIZE_EDGE_LEFT; break;
	case WaylandCursorImage::LeftPtr:
		if (released) {
			switchMaximized();
			return;
		}
		break;
	case WaylandCursorImage::Max:
		break;
	}

	if (edges != 0) {
		_display->wayland->xdg_toplevel_resize(_toplevel, _display->seat->seat, serial, edges);
	} else {
		_display->wayland->xdg_toplevel_move(_toplevel, _display->seat->seat, serial);
	}
}

void WaylandView::scheduleFrame() {
	_scheduleNext = true;
}

void WaylandView::onSurfaceInfo(core::SurfaceInfo &info) const {
	info.currentExtent = _currentExtent;
}

void WaylandView::createDecorations() {
	if (!_display->viewporter || !_clientSizeDecoration) {
		return;
	}

	WaylandShm::ShadowBuffers buf;
	if (!_display->shm->allocateDecorations(&buf, DecorWidth, DecorInset, Color::Grey_100, Color::Grey_200)) {
		return;
	}

	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.top), move(buf.topActive), WaylandDecorationName::TopSide));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.bottom), move(buf.bottomActive), WaylandDecorationName::BottomSide));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.left), move(buf.leftActive), WaylandDecorationName::LeftSide));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.right), move(buf.rightActive), WaylandDecorationName::RightSide));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.topLeft), move(buf.topLeftActive), WaylandDecorationName::TopLeftCorner));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.topRight), move(buf.topRightActive), WaylandDecorationName::TopRigntCorner));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.bottomLeft), move(buf.bottomLeftActive), WaylandDecorationName::BottomLeftCorner));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.bottomRight), move(buf.bottomRightActive), WaylandDecorationName::BottomRightCorner));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.headerLeft), move(buf.headerLeftActive), WaylandDecorationName::HeaderLeft));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.headerRight), move(buf.headerRightActive), WaylandDecorationName::HeaderRight));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, Rc<WaylandBuffer>(buf.headerCenter), Rc<WaylandBuffer>(buf.headerCenterActive), WaylandDecorationName::HeaderCenter));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, Rc<WaylandBuffer>(buf.headerCenter), Rc<WaylandBuffer>(buf.headerCenterActive), WaylandDecorationName::HeaderBottom));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.iconClose), move(buf.iconCloseActive), WaylandDecorationName::IconClose));
	_iconMaximized = _decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.iconMaximize), move(buf.iconMaximizeActive), WaylandDecorationName::IconMaximize));
	_iconMaximized->setAltBuffers(move(buf.iconRestore), move(buf.iconRestoreActive));

	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.iconMinimize), move(buf.iconMinimizeActive), WaylandDecorationName::IconMinimize));
}

void WaylandView::commit(uint32_t width, uint32_t height) {
	bool dirty = _commitedExtent.width != width || _commitedExtent.height != height || _configureSerial != maxOf<uint32_t>();

	if (!dirty) {
		for (auto &it : _decors) {
			if (it->dirty) {
				dirty = true;
				break;
			}
		}
	}

	if (!dirty) {
		return;
	}

	StringStream stream;
	stream << "commit: " << width << " " << height << ";";
	if (_configureSerial != maxOf<uint32_t>()) {
		_display->wayland->xdg_toplevel_set_min_size(_toplevel, DecorWidth * 2 + IconSize * 3, DecorWidth * 2  + DecorOffset);
		_display->wayland->xdg_surface_set_window_geometry(_xdgSurface, 0, -DecorInset - DecorOffset,
				width, height + DecorInset + DecorOffset);

		_display->wayland->xdg_surface_ack_configure(_xdgSurface, _configureSerial);
		stream << " configure: " << _configureSerial << ";";
		_configureSerial = maxOf<uint32_t>();
	}

	_commitedExtent.width = width;
	_commitedExtent.height = height;

	auto insetWidth = _commitedExtent.width - DecorInset * 2;
	auto insetHeight = _commitedExtent.height - DecorInset;
	auto cornerSize = DecorWidth + DecorInset;

	for (auto &it : _decors) {
		switch (it->name) {
		case WaylandDecorationName::TopSide:
			it->setGeometry(DecorInset, - DecorWidth - DecorInset, insetWidth, DecorWidth);
			break;
		case WaylandDecorationName::BottomSide:
			it->setGeometry(DecorInset, _commitedExtent.height, insetWidth, DecorWidth);
			break;
		case WaylandDecorationName::LeftSide:
			it->setGeometry(- DecorWidth, 0, DecorWidth, insetHeight);
			break;
		case WaylandDecorationName::RightSide:
			it->setGeometry(_commitedExtent.width, 0, DecorWidth, insetHeight);
			break;
		case WaylandDecorationName::TopLeftCorner:
			it->setGeometry(- DecorWidth, - DecorWidth - DecorInset, cornerSize, cornerSize);
			break;
		case WaylandDecorationName::TopRigntCorner:
			it->setGeometry(_commitedExtent.width - DecorInset, - DecorWidth - DecorInset, cornerSize, cornerSize);
			break;
		case WaylandDecorationName::BottomLeftCorner:
			it->setGeometry(- DecorWidth, _commitedExtent.height - DecorInset, cornerSize, cornerSize);
			break;
		case WaylandDecorationName::BottomRightCorner:
			it->setGeometry(_commitedExtent.width - DecorInset, _commitedExtent.height - DecorInset, cornerSize, cornerSize);
			break;
		case WaylandDecorationName::HeaderLeft:
			it->setGeometry(0, - DecorInset - DecorOffset, DecorInset, DecorInset);
			break;
		case WaylandDecorationName::HeaderRight:
			it->setGeometry(_commitedExtent.width - DecorInset, - DecorInset - DecorOffset, DecorInset, DecorInset);
			break;
		case WaylandDecorationName::HeaderCenter:
			it->setGeometry(DecorInset, - DecorInset - DecorOffset, _commitedExtent.width - DecorInset * 2, DecorInset);
			break;
		case WaylandDecorationName::HeaderBottom:
			it->setGeometry(0, - DecorOffset, _commitedExtent.width, DecorOffset);
			break;
		case WaylandDecorationName::IconClose:
			it->setGeometry(_commitedExtent.width - (IconSize + 4), -IconSize, IconSize, IconSize);
			break;
		case WaylandDecorationName::IconMaximize:
			it->setGeometry(_commitedExtent.width - (IconSize + 4) * 2, -IconSize, IconSize, IconSize);
			break;
		case WaylandDecorationName::IconMinimize:
			it->setGeometry(_commitedExtent.width - (IconSize + 4) * 3, -IconSize, IconSize, IconSize);
			break;
		default:
			break;
		}
	}

	bool surfacesDirty = false;
	for (auto &it : _decors) {
		if (it->commit()) {
			surfacesDirty = true;
		}
	}
	if (surfacesDirty) {
		stream << " Surfaces Dirty;";
	}

	XL_WAYLAND_LOG(stream.str());
}

void WaylandView::readFromClipboard(Function<void(BytesView, StringView)> &&, Ref *) {

}

void WaylandView::writeToClipboard(BytesView, StringView contentType) {

}

}

#endif
