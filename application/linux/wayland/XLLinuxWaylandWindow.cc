/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "XLLinuxWaylandWindow.h"
#include "XLContextInfo.h"
#include "XLCorePresentationEngine.h"
#include "XLLinuxWaylandProtocol.h"
#include "XlCoreMonitorInfo.h"
#include "linux/XLLinuxContextController.h"
#include "linux/thirdparty/wayland-protocols/xdg-shell.h"
#include "XLAppWindow.h"
#include <wayland-client-protocol.h>

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkPresentationEngine.h"
#endif

#include <linux/input.h>

#ifndef XL_WAYLAND_LOG
#if XL_WAYLAND_DEBUG
#define XL_WAYLAND_LOG(...) log::debug("Wayland", __VA_ARGS__)
#else
#define XL_WAYLAND_LOG(...)
#endif
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

// clang-format off

static struct wl_surface_listener s_WaylandSurfaceListener{
	.enter = [](void *data, wl_surface *surface, wl_output *output) {
		reinterpret_cast<WaylandWindow *>(data)->handleSurfaceEnter(surface, output);
	},
	.leave = [](void *data, wl_surface *surface, wl_output *output) {
		reinterpret_cast<WaylandWindow *>(data)->handleSurfaceLeave(surface, output);
	},
	.preferred_buffer_scale = [](void *data, struct wl_surface *wl_surface, int32_t factor) {
		reinterpret_cast<WaylandWindow *>(data)->setPreferredScale(factor);
	},
	.preferred_buffer_transform = [](void *data, struct wl_surface *wl_surface, uint32_t transform) {
		reinterpret_cast<WaylandWindow *>(data)->setPreferredTransform(transform);
	}
};

static const wl_callback_listener s_WaylandSurfaceFrameListener{
	.done = [](void *data, wl_callback *wl_callback, uint32_t callback_data) {
		reinterpret_cast<WaylandWindow *>(data)->handleSurfaceFrameDone(wl_callback, callback_data);
	},
};

static xdg_surface_listener const s_XdgSurfaceListener{
	.configure = [](void *data, xdg_surface *xdg_surface, uint32_t serial) {
		reinterpret_cast<WaylandWindow *>(data)->handleSurfaceConfigure(xdg_surface, serial);
	},
};

static const xdg_toplevel_listener s_XdgToplevelListener{
	.configure = [](void *data, xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, wl_array *states) {
		reinterpret_cast<WaylandWindow *>(data)->handleToplevelConfigure(xdg_toplevel,
			width, height, states);
	},
	.close = [](void *data, struct xdg_toplevel *xdg_toplevel) {
		reinterpret_cast<WaylandWindow *>(data)->handleToplevelClose(xdg_toplevel);
	},
	.configure_bounds = [](void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {
		reinterpret_cast<WaylandWindow *>(data)->handleToplevelBounds(xdg_toplevel, width, height);
	},
	.wm_capabilities = [](void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities) {
		reinterpret_cast<WaylandWindow *>(data)->handleToplevelCapabilities(xdg_toplevel, capabilities);
	}
};

// clang-format on

WaylandWindow::~WaylandWindow() {
	if (_controller && _isRootWindow) {
		_controller.get_cast<LinuxContextController>()->handleRootWindowClosed();
	}

	_iconMaximized = nullptr;
	_decors.clear();
	if (_toplevel) {
		_wayland->xdg_toplevel_destroy(_toplevel);
		_toplevel = nullptr;
	}
	if (_xdgSurface) {
		_wayland->xdg_surface_destroy(_xdgSurface);
		_xdgSurface = nullptr;
	}
	if (_surface) {
		_display->destroySurface(this);
		_surface = nullptr;
	}
	_display = nullptr;
}

WaylandWindow::WaylandWindow() { }

bool WaylandWindow::init(NotNull<WaylandDisplay> display, Rc<WindowInfo> &&info,
		NotNull<const ContextInfo> content, NotNull<LinuxContextController> c) {
	if (!ContextNativeWindow::init(c, move(info),
				WindowCapabilities::FullscreenSwitch | WindowCapabilities::Subwindows)) {
		return false;
	}

	_display = display;
	_wayland = _display->wayland;
	_controller = c;

	_currentExtent = Extent2(_info->rect.width, _info->rect.height);

	_surface = _display->createSurface(this);
	if (_surface) {
		_wayland->wl_surface_set_user_data(_surface, this);
		_wayland->wl_surface_add_listener(_surface, &s_WaylandSurfaceListener, this);

		auto region = _wayland->wl_compositor_create_region(_display->compositor);
		_wayland->wl_region_add(region, 0, 0, _currentExtent.width, _currentExtent.height);
		_wayland->wl_surface_set_opaque_region(_surface, region);

		_xdgSurface = _wayland->xdg_wm_base_get_xdg_surface(_display->xdgWmBase, _surface);

		_wayland->xdg_surface_add_listener(_xdgSurface, &s_XdgSurfaceListener, this);

		_toplevel = _wayland->xdg_surface_get_toplevel(_xdgSurface);
		_wayland->xdg_toplevel_set_title(_toplevel, _info->title.data());
		_wayland->xdg_toplevel_set_app_id(_toplevel, _info->id.data());
		_wayland->xdg_toplevel_add_listener(_toplevel, &s_XdgToplevelListener, this);

		if (_clientSizeDecoration) {
			createDecorations();
		}

		//_wayland->wl_surface_commit(_surface);
		_wayland->wl_region_destroy(region);
	}

	uint32_t rate = 60'000;
	for (auto &it : _display->outputs) { rate = std::max(rate, uint32_t(it->currentMode.rate)); }
	_frameRate = rate;

	return true;
}

void WaylandWindow::mapWindow() { _display->flush(); }

void WaylandWindow::unmapWindow() {
	if (_frameCallback) {
		_wayland->wl_callback_destroy(_frameCallback);
		_frameCallback = nullptr;
	}
}

bool WaylandWindow::close() {
	if (!_shouldClose) {
		_shouldClose = true;
		if (!_controller->notifyWindowClosed(this)) {
			_shouldClose = false;
		}
		return true;
	}
	return false;
}

void WaylandWindow::handleFramePresented(NotNull<core::PresentationFrame> frame) {
	auto &c = frame->getFrameConstraints();

	bool dirty = _commitedExtent.width != c.extent.width
			|| _commitedExtent.height != c.extent.height || _configureSerial != maxOf<uint32_t>();

	if (!dirty) {
		for (auto &it : _decors) {
			if (it->dirty) {
				dirty = true;
				break;
			}
		}
	}

	if (!dirty) {
		if (!_frameCallback) {
			_frameCallback = _wayland->wl_surface_frame(_surface);
			_wayland->wl_callback_add_listener(_frameCallback, &s_WaylandSurfaceFrameListener,
					this);
			_wayland->wl_surface_commit(_surface);
		}
		return;
	}

	StringStream stream;
	stream << "commit: " << c.extent.width << " " << c.extent.height << ";";

	_commitedExtent.width = c.extent.width;
	_commitedExtent.height = c.extent.height;

	auto insetWidth = _commitedExtent.width - DecorInset * 2;
	auto insetHeight = _commitedExtent.height - DecorInset;
	auto cornerSize = DecorWidth + DecorInset;

	for (auto &it : _decors) {
		switch (it->name) {
		case WaylandDecorationName::TopSide:
			it->setGeometry(DecorInset, -DecorWidth - DecorInset, insetWidth, DecorWidth);
			break;
		case WaylandDecorationName::BottomSide:
			it->setGeometry(DecorInset, _commitedExtent.height, insetWidth, DecorWidth);
			break;
		case WaylandDecorationName::LeftSide:
			it->setGeometry(-DecorWidth, 0, DecorWidth, insetHeight);
			break;
		case WaylandDecorationName::RightSide:
			it->setGeometry(_commitedExtent.width, 0, DecorWidth, insetHeight);
			break;
		case WaylandDecorationName::TopLeftCorner:
			it->setGeometry(-DecorWidth, -DecorWidth - DecorInset, cornerSize, cornerSize);
			break;
		case WaylandDecorationName::TopRightCorner:
			it->setGeometry(_commitedExtent.width - DecorInset, -DecorWidth - DecorInset,
					cornerSize, cornerSize);
			break;
		case WaylandDecorationName::BottomLeftCorner:
			it->setGeometry(-DecorWidth, _commitedExtent.height - DecorInset, cornerSize,
					cornerSize);
			break;
		case WaylandDecorationName::BottomRightCorner:
			it->setGeometry(_commitedExtent.width - DecorInset, _commitedExtent.height - DecorInset,
					cornerSize, cornerSize);
			break;
		case WaylandDecorationName::HeaderLeft:
			it->setGeometry(0, -DecorInset - DecorOffset, DecorInset, DecorInset);
			break;
		case WaylandDecorationName::HeaderRight:
			it->setGeometry(_commitedExtent.width - DecorInset, -DecorInset - DecorOffset,
					DecorInset, DecorInset);
			break;
		case WaylandDecorationName::HeaderCenter:
			it->setGeometry(DecorInset, -DecorInset - DecorOffset,
					_commitedExtent.width - DecorInset * 2, DecorInset);
			break;
		case WaylandDecorationName::HeaderBottom:
			it->setGeometry(0, -DecorOffset, _commitedExtent.width, DecorOffset);
			break;
		case WaylandDecorationName::IconClose:
			it->setGeometry(_commitedExtent.width - (IconSize + 4), -IconSize, IconSize, IconSize);
			break;
		case WaylandDecorationName::IconMaximize:
			it->setGeometry(_commitedExtent.width - (IconSize + 4) * 2, -IconSize, IconSize,
					IconSize);
			break;
		case WaylandDecorationName::IconMinimize:
			it->setGeometry(_commitedExtent.width - (IconSize + 4) * 3, -IconSize, IconSize,
					IconSize);
			break;
		default: break;
		}
	}

	bool surfacesDirty = false;
	for (auto &it : _decors) {
		if (it->commit()) {
			surfacesDirty = true;
		}
	}

	if (_configureSerial != maxOf<uint32_t>()) {
		_wayland->xdg_toplevel_set_min_size(_toplevel, DecorWidth * 2 + IconSize * 3,
				DecorWidth * 2 + DecorOffset + DecorInset);

		// Capture decoration
		_wayland->xdg_surface_set_window_geometry(_xdgSurface, 0, -DecorInset - DecorOffset,
				c.extent.width, c.extent.height + DecorInset + DecorOffset);

		_wayland->xdg_surface_ack_configure(_xdgSurface, _configureSerial);
		stream << " configure: " << _configureSerial << ";";
		_configureSerial = maxOf<uint32_t>();
	}

	if (!_frameCallback) {
		_frameCallback = _wayland->wl_surface_frame(_surface);
		_wayland->wl_callback_add_listener(_frameCallback, &s_WaylandSurfaceFrameListener, this);
	}

	_wayland->wl_surface_commit(_surface);

	if (surfacesDirty) {
		_appWindow->setReadyForNextFrame();
		stream << " Surfaces Dirty;";
	}

	XL_WAYLAND_LOG(stream.str());
}

core::FrameConstraints WaylandWindow::exportConstraints(core::FrameConstraints &&c) const {
	c.extent = _currentExtent;
	if (c.density == 0.0f) {
		c.density = 1.0f;
	}
	if (_density != 0.0f) {
		c.density *= _density;
	}
	c.frameInterval = 1'000'000'000 / _frameRate;
	return move(c);
}

core::SurfaceInfo WaylandWindow::getSurfaceOptions(core::SurfaceInfo &&info) const {
	info.currentExtent = _currentExtent;
	return sp::move(info);
}

Extent2 WaylandWindow::getExtent() const { return _currentExtent; }

Rc<core::Surface> WaylandWindow::makeSurface(NotNull<core::Instance> cinstance) {
#if MODULE_XENOLITH_BACKEND_VK
	if (cinstance->getApi() != core::InstanceApi::Vulkan) {
		return nullptr;
	}

	auto instance = static_cast<vk::Instance *>(cinstance.get());

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkWaylandSurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR, nullptr,
		0, _display->display, _surface};
	if (instance->vkCreateWaylandSurfaceKHR(instance->getInstance(), &createInfo, nullptr, &surface)
			!= VK_SUCCESS) {
		return nullptr;
	}
	return Rc<vk::Surface>::create(instance, surface, this);
#else
	log::error("XcbWindow", "No available GAPI found for a surface");
	return nullptr;
#endif
}

core::PresentationOptions WaylandWindow::getPreferredOptions() const {
	core::PresentationOptions opts;
	opts.followDisplayLinkBarrier = true;
	return opts;
}

void WaylandWindow::setFullscreen(const core::MonitorId &id, const core::ModeInfo &mode,
		Function<void(Status)> &&cb, Ref *ref) {
	if (mode != core::ModeInfo::Current && id != core::MonitorId::None) {
		auto dcm = _controller.get_cast<LinuxContextController>()->getDisplayConfigManager();
		if (dcm) {
			dcm->setModeExclusive(id, mode, sp::move(cb), ref);
		}
	}
}

void WaylandWindow::handleSurfaceEnter(wl_surface *surface, wl_output *output) {
	if (!_wayland->ownsProxy(output)) {
		return;
	}

	auto out = (WaylandOutput *)_wayland->wl_output_get_user_data(output);
	if (out) {
		_activeOutputs.emplace(out);
		XL_WAYLAND_LOG("handleSurfaceEnter: output: ", out->description());
	}
}

void WaylandWindow::handleSurfaceLeave(wl_surface *surface, wl_output *output) {
	if (!_wayland->ownsProxy(output)) {
		return;
	}

	auto out = (WaylandOutput *)_wayland->wl_output_get_user_data(output);
	if (out) {
		_activeOutputs.erase(out);
		XL_WAYLAND_LOG("handleSurfaceLeave: output: ", out->description());
	}
}

void WaylandWindow::handleSurfaceConfigure(xdg_surface *surface, uint32_t serial) {
	XL_WAYLAND_LOG("handleSurfaceConfigure: serial: ", serial);
	_configureSerial = serial;
}

void WaylandWindow::handleToplevelConfigure(xdg_toplevel *xdg_toplevel, int32_t width,
		int32_t height, wl_array *states) {
	StringStream stream;
	stream << "handleToplevelConfigure: width: " << width << ", height: " << height << ";";

	auto oldState = _state;
	_state.reset();

	for (uint32_t *it = (uint32_t *)states->data;
			(const char *)it < ((const char *)states->data + states->size); ++it) {
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
		_pendingEvents.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::FocusGain,
				_state.test(XDG_TOPLEVEL_STATE_ACTIVATED)));
	}

	auto checkVisible = [&, this](WaylandDecorationName name) {
		switch (name) {
		case WaylandDecorationName::RightSide:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) {
				return false;
			}
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_RIGHT)) {
				return false;
			}
			break;
		case WaylandDecorationName::TopRightCorner:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) {
				return false;
			}
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_TOP)
					&& _state.test(XDG_TOPLEVEL_STATE_TILED_RIGHT)) {
				return false;
			}
			break;
		case WaylandDecorationName::TopSide:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) {
				return false;
			}
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_TOP)) {
				return false;
			}
			break;
		case WaylandDecorationName::TopLeftCorner:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) {
				return false;
			}
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_TOP)
					&& _state.test(XDG_TOPLEVEL_STATE_TILED_LEFT)) {
				return false;
			}
			break;
		case WaylandDecorationName::BottomRightCorner:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) {
				return false;
			}
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_BOTTOM)
					&& _state.test(XDG_TOPLEVEL_STATE_TILED_RIGHT)) {
				return false;
			}
			break;
		case WaylandDecorationName::BottomSide:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) {
				return false;
			}
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_BOTTOM)) {
				return false;
			}
			break;
		case WaylandDecorationName::BottomLeftCorner:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) {
				return false;
			}
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_BOTTOM)
					&& _state.test(XDG_TOPLEVEL_STATE_TILED_LEFT)) {
				return false;
			}
			break;
		case WaylandDecorationName::LeftSide:
			if (_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) {
				return false;
			}
			if (_state.test(XDG_TOPLEVEL_STATE_TILED_LEFT)) {
				return false;
			}
			break;
		default: break;
		}
		return true;
	};

	for (auto &it : _decors) {
		it->setActive(_state.test(XDG_TOPLEVEL_STATE_ACTIVATED));
		it->setVisible(checkVisible(it->name));
	}

	if (width && height) {
		height -= (DecorInset + DecorOffset);

		if (_currentExtent.width != static_cast<uint32_t>(width)
				|| _currentExtent.height != static_cast<uint32_t>(height)) {
			_currentExtent.width = static_cast<uint32_t>(width);
			_currentExtent.height = static_cast<uint32_t>(height);
			_controller->notifyWindowConstraintsChanged(this, true);

			stream << "surface: " << _currentExtent.width << " " << _currentExtent.height;
		}
	}

	XL_WAYLAND_LOG(stream.str());
}

void WaylandWindow::handleToplevelClose(xdg_toplevel *xdg_toplevel) {
	XL_WAYLAND_LOG("handleToplevelClose");
	_controller->notifyWindowClosed(this);
}

void WaylandWindow::handleToplevelBounds(xdg_toplevel *xdg_toplevel, int32_t width,
		int32_t height) {
	XL_WAYLAND_LOG("handleToplevelBounds: width: ", width, ", height: ", height);
}

void WaylandWindow::handleToplevelCapabilities(xdg_toplevel *xdg_toplevel, wl_array *capabilities) {
	StringStream stream;

	_capabilities.reset();

	for (uint32_t *it = (uint32_t *)capabilities->data;
			(const char *)it < ((const char *)capabilities->data + capabilities->size); ++it) {
		_capabilities.set(*it);
		switch (*it) {
		case XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU: stream << " WINDOW_MENU;"; break;
		case XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE: stream << " MAXIMIZE;"; break;
		case XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN: stream << " FULLSCREEN;"; break;
		case XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE: stream << " MINIMIZE;"; break;
		}
	}

	XL_WAYLAND_LOG("handleToplevelCapabilities: ", stream.str());
}

void WaylandWindow::handleSurfaceFrameDone(wl_callback *frame, uint32_t data) {
	if (frame != _frameCallback) {
		_wayland->wl_callback_destroy(frame);
	} else {
		_wayland->wl_callback_destroy(frame);
		_frameCallback = nullptr;
		_appWindow->update(core::PresentationUpdateFlags::DisplayLink);
	}
}

void WaylandWindow::handlePointerEnter(wl_fixed_t surface_x, wl_fixed_t surface_y) {
	if (!_pointerInit || _display->seat->hasPointerFrames) {
		auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::Enter});
		ev.enter.x = surface_x;
		ev.enter.y = surface_y;
	} else {
		_pendingEvents.emplace_back(
				core::InputEventData::BoolEvent(core::InputEventName::PointerEnter, true,
						Vec2(float(wl_fixed_to_double(surface_x)),
								float(_currentExtent.height - wl_fixed_to_double(surface_y)))));

		_surfaceX = wl_fixed_to_double(surface_x);
		_surfaceY = wl_fixed_to_double(surface_y);
	}

	XL_WAYLAND_LOG("handlePointerEnter: x: ", wl_fixed_to_int(surface_x),
			", y: ", wl_fixed_to_int(surface_y));
}

void WaylandWindow::handlePointerLeave() {
	if (!_pointerInit) {
		_pointerInit = true;
		if (!_display->seat->hasPointerFrames) {
			handlePointerFrame();
		}
	}

	handlePointerFrame(); // drop pending events
	_pendingEvents.emplace_back(core::InputEventData::BoolEvent(core::InputEventName::PointerEnter,
			false, Vec2(float(_surfaceX), float(_currentExtent.height - _surfaceY))));
}

void WaylandWindow::handlePointerMotion(uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	// XL_WAYLAND_LOG("handlePointerMotion: x: ", wl_fixed_to_int(surface_x), ", y: ", wl_fixed_to_int(surface_y));

	if (!_pointerInit) {
		_pointerInit = true;
		if (!_display->seat->hasPointerFrames) {
			handlePointerFrame();
		}
	}

	if (_display->seat->hasPointerFrames) {
		auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::Motion});
		ev.motion.time = time;
		ev.motion.x = surface_x;
		ev.motion.y = surface_y;
	} else {
		_pendingEvents.emplace_back(core::InputEventData(
				{maxOf<uint32_t>(), core::InputEventName::MouseMove, core::InputMouseButton::None,
					_activeModifiers, float(wl_fixed_to_double(surface_x)),
					float(_currentExtent.height - wl_fixed_to_double(surface_y))}));

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

void WaylandWindow::handlePointerButton(uint32_t serial, uint32_t time, uint32_t button,
		uint32_t state) {
	if (!_pointerInit) {
		return;
	}

	XL_WAYLAND_LOG("handlePointerButton");
	if (_display->seat->hasPointerFrames) {
		auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::Button});
		ev.button.serial = serial;
		ev.button.time = time;
		ev.button.button = button;
		ev.button.state = state;
	} else {
		_pendingEvents.emplace_back(core::InputEventData({button,
			((state == WL_POINTER_BUTTON_STATE_PRESSED) ? core::InputEventName::Begin
														: core::InputEventName::End),
			getButton(button), _activeModifiers, float(_surfaceX),
			float(_currentExtent.height - _surfaceY)}));
	}
}

void WaylandWindow::handlePointerAxis(uint32_t time, uint32_t axis, float val) {
	if (!_pointerInit) {
		return;
	}

	XL_WAYLAND_LOG("handlePointerAxis: ", time);

	if (_display->seat->hasPointerFrames) {
		auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::Axis});
		ev.axis.time = time;
		ev.axis.axis = axis;
		ev.axis.value = val;
	} else {
		core::InputMouseButton btn = core::InputMouseButton::None;
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

		core::InputEventData event({toInt(btn), core::InputEventName::Scroll, btn, _activeModifiers,
			float(_surfaceX), float(_currentExtent.height - _surfaceY)});

		switch (axis) {
		case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
			event.point.valueX = val;
			event.point.valueY = 0.0f;
			break;
		case WL_POINTER_AXIS_VERTICAL_SCROLL:
			event.point.valueX = 0.0f;
			event.point.valueY = -val;
			break;
		}

		_pendingEvents.emplace_back(event);
	}
}

void WaylandWindow::handlePointerAxisSource(uint32_t axis_source) {
	if (!_pointerInit) {
		return;
	}

	XL_WAYLAND_LOG("handlePointerAxisSource");

	auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::AxisSource});
	ev.axisSource.axis_source = axis_source;
}

void WaylandWindow::handlePointerAxisStop(uint32_t time, uint32_t axis) {
	if (!_pointerInit) {
		return;
	}

	XL_WAYLAND_LOG("handlePointerAxisStop");

	auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::AxisStop});
	ev.axisStop.time = time;
	ev.axisStop.axis = axis;
}

void WaylandWindow::handlePointerAxisDiscrete(uint32_t axis, int32_t discrete) {
	if (!_pointerInit) {
		return;
	}

	XL_WAYLAND_LOG("handlePointerAxisDiscrete");

	auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::AxisDiscrete});
	ev.axisDiscrete.axis = axis;
	ev.axisDiscrete.discrete120 = discrete;
}

void WaylandWindow::handlePointerAxisRelativeDirection(uint32_t axis, uint32_t direction) {
	if (!_pointerInit) {
		return;
	}

	XL_WAYLAND_LOG("handlePointerAxisRelativeDirection");

	auto &ev = _pointerEvents.emplace_back(PointerEvent{PointerEvent::AxisDiscrete});
	ev.axisRelativeDirection.axis = axis;
	ev.axisRelativeDirection.direction = direction;
}

void WaylandWindow::handlePointerFrame() {
	if (!_pointerInit || _pointerEvents.empty()) {
		return;
	}

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
			_pendingEvents.emplace_back(core::InputEventData::BoolEvent(
					core::InputEventName::PointerEnter, true,
					Vec2(float(wl_fixed_to_double(it.enter.x)),
							float(_currentExtent.height - wl_fixed_to_double(it.enter.y)))));
			positionChanged = true;
			x = wl_fixed_to_double(it.enter.x);
			y = wl_fixed_to_double(it.enter.y);
			break;
		case PointerEvent::Leave: break;
		case PointerEvent::Motion:
			positionChanged = true;
			x = wl_fixed_to_double(it.motion.x);
			y = wl_fixed_to_double(it.motion.y);
			break;
		case PointerEvent::Button: break;
		case PointerEvent::Axis:
			switch (it.axis.axis) {
			case WL_POINTER_AXIS_VERTICAL_SCROLL:
				hasAxis = true;
				axisY -= it.axis.value;
				if (it.axis.value < 0) {
					axisBtn = core::InputMouseButton::MouseScrollUp;
				} else {
					axisBtn = core::InputMouseButton::MouseScrollDown;
				}
				break;
			case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
				hasAxis = true;
				axisX += it.axis.value;
				if (it.axis.value > 0) {
					axisBtn = core::InputMouseButton::MouseScrollRight;
				} else {
					axisBtn = core::InputMouseButton::MouseScrollLeft;
				}
				break;
			default: break;
			}
			break;
		case PointerEvent::AxisSource: axisSource = it.axisSource.axis_source; break;
		case PointerEvent::AxisStop: break;
		case PointerEvent::AxisDiscrete: break;
		}
	}

	if (positionChanged) {
		_pendingEvents.emplace_back(core::InputEventData(
				{maxOf<uint32_t>(), core::InputEventName::MouseMove, core::InputMouseButton::None,
					_activeModifiers, float(x), float(_currentExtent.height - y)}));

		_surfaceX = x;
		_surfaceY = y;
	}

	if (hasAxis) {
		auto &event = _pendingEvents.emplace_back(
				core::InputEventData({axisSource, core::InputEventName::Scroll, axisBtn,
					_activeModifiers, float(_surfaceX), float(_currentExtent.height - _surfaceY)}));

		event.point.valueX = float(axisX);
		event.point.valueY = float(axisY);
	}

	for (auto &it : _pointerEvents) {
		switch (it.event) {
		case PointerEvent::None: break;
		case PointerEvent::Enter: break;
		case PointerEvent::Leave:
			_pendingEvents.emplace_back(
					core::InputEventData::BoolEvent(core::InputEventName::PointerEnter, false,
							Vec2(float(_surfaceX), float(_currentExtent.height - _surfaceY))));
			break;
		case PointerEvent::Motion: break;
		case PointerEvent::Button:
			_pendingEvents.emplace_back(core::InputEventData({it.button.button,
				((it.button.state == WL_POINTER_BUTTON_STATE_PRESSED) ? core::InputEventName::Begin
																	  : core::InputEventName::End),
				getButton(it.button.button), _activeModifiers, float(_surfaceX),
				float(_currentExtent.height - _surfaceY)}));
			break;
		case PointerEvent::Axis: break;
		case PointerEvent::AxisSource: break;
		case PointerEvent::AxisStop: break;
		case PointerEvent::AxisDiscrete: break;
		}
	}

	_pointerEvents.clear();
}

void WaylandWindow::handleKeyboardEnter(Vector<uint32_t> &&keys, uint32_t depressed,
		uint32_t latched, uint32_t locked) {
	handleKeyModifiers(depressed, latched, locked);
	uint32_t n = 1;
	for (auto &it : keys) {
		handleKey(n, it, WL_KEYBOARD_KEY_STATE_PRESSED);
		++n;
	}
}

void WaylandWindow::handleKeyboardLeave() {
	uint32_t n = 1;
	for (auto &it : _keys) {
		core::InputEventData event(
				{n, core::InputEventName::KeyCanceled, core::InputMouseButton::None,
					_activeModifiers, float(_surfaceX), float(_currentExtent.height - _surfaceY)});

		event.key.keycode = _display->seat->translateKey(it.second.scancode);
		event.key.keysym = it.second.scancode;
		event.key.keychar = it.second.codepoint;

		_pendingEvents.emplace_back(move(event));

		++n;
	}
}

void WaylandWindow::handleKey(uint32_t time, uint32_t scancode, uint32_t state) {
	core::InputEventData event({time,
		(state == WL_KEYBOARD_KEY_STATE_PRESSED) ? core::InputEventName::KeyPressed
												 : core::InputEventName::KeyReleased,
		core::InputMouseButton::None, _activeModifiers, float(_surfaceX),
		float(_currentExtent.height - _surfaceY)});

	event.key.keycode = _display->seat->translateKey(scancode);
	event.key.compose = core::InputKeyComposeState::Nothing;
	event.key.keysym = scancode;
	event.key.keychar = 0;

	const xkb_keysym_t *keysyms = nullptr;
	const xkb_keycode_t keycode = scancode + 8;

	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		char32_t codepoint = 0;
		if (_display->xkb && isTextInputEnabled()) {
			if (_display->xkb->xkb_state_key_get_syms(_display->seat->state, keycode, &keysyms)
					== 1) {
				const xkb_keysym_t keysym =
						_display->seat->composeSymbol(keysyms[0], event.key.compose);
				const uint32_t cp = _display->xkb->xkb_keysym_to_utf32(keysym);
				if (cp != 0 && keysym != XKB_KEY_NoSymbol) {
					codepoint = cp;
				}
			}
		}

		auto it = _keys.emplace(scancode,
							   KeyData{scancode, codepoint,
								   sp::platform::clock(ClockType::Monotonic), false})
						  .first;

		if (_display->xkb
				&& _display->xkb->xkb_keymap_key_repeats(
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

	_pendingEvents.emplace_back(sp::move(event));
}

void WaylandWindow::handleKeyModifiers(uint32_t depressed, uint32_t latched, uint32_t locked) {
	if (!_display->seat->state) {
		return;
	}

	_activeModifiers = core::InputModifier::None;
	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.controlIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= core::InputModifier::Ctrl;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.altIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= core::InputModifier::Alt;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.shiftIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= core::InputModifier::Shift;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.superIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= core::InputModifier::Mod4;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.capsLockIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= core::InputModifier::CapsLock;
	}

	if (_display->xkb->xkb_state_mod_index_is_active(_display->seat->state,
				_display->seat->keyState.numLockIndex, XKB_STATE_MODS_EFFECTIVE)
			== 1) {
		_activeModifiers |= core::InputModifier::NumLock;
	}
}

void WaylandWindow::handleKeyRepeat() {
	Vector<core::InputEventData> events;
	auto spawnRepeatEvent = [&, this](const KeyData &it) {
		core::InputEventData event({uint32_t(events.size() + 1), core::InputEventName::KeyRepeated,
			core::InputMouseButton::None, _activeModifiers, float(_surfaceX),
			float(_currentExtent.height - _surfaceY)});

		event.key.keycode = _display->seat->translateKey(it.scancode);
		event.key.keysym = it.scancode;
		event.key.keychar = it.codepoint;

		events.emplace_back(move(event));
	};

	uint64_t repeatDelay = _display->seat->keyState.keyRepeatDelay;
	uint64_t repeatInterval = _display->seat->keyState.keyRepeatInterval;
	auto t = sp::platform::clock(ClockType::Monotonic);
	for (auto &it : _keys) {
		if (it.second.repeats) {
			if (!it.second.lastRepeat) {
				auto dt = t - it.second.time;
				if (dt > repeatDelay * 1'000) {
					dt -= repeatDelay * 1'000;
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

	for (auto &it : events) { _pendingEvents.emplace_back(it); }
}

void WaylandWindow::notifyScreenChange() {
	XL_WAYLAND_LOG("notifyScreenChange");
	core::InputEventData event =
			core::InputEventData::BoolEvent(core::InputEventName::ScreenUpdate, true);

	_pendingEvents.emplace_back(event);

	if (!_controller.get_cast<LinuxContextController>()->isInPoll()) {
		dispatchPendingEvents();
	}
}

void WaylandWindow::handleDecorationPress(WaylandDecoration *decor, uint32_t serial, uint32_t btn,
		bool released) {
	auto switchMaximized = [&, this] {
		if (!_state.test(XDG_TOPLEVEL_STATE_MAXIMIZED)) {
			_wayland->xdg_toplevel_set_maximized(_toplevel);
			_iconMaximized->setAlternative(true);
		} else {
			_wayland->xdg_toplevel_unset_maximized(_toplevel);
			_iconMaximized->setAlternative(false);
		}
	};
	switch (decor->name) {
	case WaylandDecorationName::IconClose:
		_appWindow->setReadyForNextFrame();
		handleToplevelClose(_toplevel);
		return;
		break;
	case WaylandDecorationName::IconMaximize:
		switchMaximized();
		_appWindow->setReadyForNextFrame();
		return;
		break;
	case WaylandDecorationName::IconMinimize:
		_wayland->xdg_toplevel_set_minimized(_toplevel);
		return;
	case WaylandDecorationName::HeaderCenter:
	case WaylandDecorationName::HeaderBottom:
	case WaylandDecorationName::HeaderLeft:
		if (btn == BTN_RIGHT) {
			_wayland->xdg_toplevel_show_window_menu(_toplevel, _display->seat->seat, serial,
					wl_fixed_to_int(decor->pointerX), wl_fixed_to_int(decor->pointerY));
			_appWindow->setReadyForNextFrame();
		}
		break;
	default: break;
	}
	uint32_t edges = 0;
	switch (decor->image) {
	case WindowLayerFlags::CursorResizeRight: edges = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT; break;
	case WindowLayerFlags::CursorResizeTopRight: edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT; break;
	case WindowLayerFlags::CursorResizeTop: edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP; break;
	case WindowLayerFlags::CursorResizeTopLeft: edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT; break;
	case WindowLayerFlags::CursorResizeBottomRight:
		edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
		break;
	case WindowLayerFlags::CursorResizeBottom: edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM; break;
	case WindowLayerFlags::CursorResizeBottomLeft:
		edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
		break;
	case WindowLayerFlags::CursorResizeLeft: edges = XDG_TOPLEVEL_RESIZE_EDGE_LEFT; break;
	case WindowLayerFlags::CursorArrow:
		if (released) {
			switchMaximized();
			_appWindow->setReadyForNextFrame();
			return;
		}
		break;
	default: break;
	}

	if (edges != 0) {
		_wayland->xdg_toplevel_resize(_toplevel, _display->seat->seat, serial, edges);
		_appWindow->setReadyForNextFrame();
	} else {
		_wayland->xdg_toplevel_move(_toplevel, _display->seat->seat, serial);
		_appWindow->setReadyForNextFrame();
	}
}

void WaylandWindow::setPreferredScale(int32_t scale) {
	if (_density != float(scale)) {
		_density = float(scale);
		_wayland->wl_surface_set_buffer_scale(_surface, scale);
		_controller->notifyWindowConstraintsChanged(this, false);
	}
}

void WaylandWindow::setPreferredTransform(uint32_t) { }

void WaylandWindow::dispatchPendingEvents() {
	if (!_shouldClose) {
		if (!_keys.empty()) {
			handleKeyRepeat();
		}
	}

	if (!_pendingEvents.empty()) {
		_controller->notifyWindowInputEvents(this, sp::move(_pendingEvents));
	}
	_pendingEvents.clear();

	bool surfacesDirty = false;
	for (auto &it : _decors) {
		if (it->commit()) {
			surfacesDirty = true;
		}
	}

	if (surfacesDirty) {
		_wayland->wl_surface_commit(_surface);
	}
}

WindowLayerFlags WaylandWindow::getCursor() const {
	auto layerCursor = _layerFlags & WindowLayerFlags::CursorMask;
	if (layerCursor == WindowLayerFlags::None) {
		return WindowLayerFlags::CursorArrow;
	}
	return layerCursor;
}

bool WaylandWindow::updateTextInput(const TextInputRequest &, TextInputFlags flags) { return true; }

void WaylandWindow::cancelTextInput() { }

void WaylandWindow::createDecorations() {
	if (!_display->viewporter || !_clientSizeDecoration) {
		return;
	}

	ShadowBuffers buf;
	if (!allocateDecorations(_wayland, _display->shm->shm, &buf, DecorWidth, DecorInset,
				Color::Grey_100, Color::Grey_200)) {
		return;
	}

	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.top), move(buf.topActive),
			WaylandDecorationName::TopSide));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.bottom),
			move(buf.bottomActive), WaylandDecorationName::BottomSide));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.left), move(buf.leftActive),
			WaylandDecorationName::LeftSide));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.right), move(buf.rightActive),
			WaylandDecorationName::RightSide));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.topLeft),
			move(buf.topLeftActive), WaylandDecorationName::TopLeftCorner));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.topRight),
			move(buf.topRightActive), WaylandDecorationName::TopRightCorner));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.bottomLeft),
			move(buf.bottomLeftActive), WaylandDecorationName::BottomLeftCorner));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.bottomRight),
			move(buf.bottomRightActive), WaylandDecorationName::BottomRightCorner));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.headerLeft),
			move(buf.headerLeftActive), WaylandDecorationName::HeaderLeft));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.headerRight),
			move(buf.headerRightActive), WaylandDecorationName::HeaderRight));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, Rc<WaylandBuffer>(buf.headerCenter),
			Rc<WaylandBuffer>(buf.headerCenterActive), WaylandDecorationName::HeaderCenter));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, Rc<WaylandBuffer>(buf.headerCenter),
			Rc<WaylandBuffer>(buf.headerCenterActive), WaylandDecorationName::HeaderBottom));
	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.iconClose),
			move(buf.iconCloseActive), WaylandDecorationName::IconClose));
	_iconMaximized =
			_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.iconMaximize),
					move(buf.iconMaximizeActive), WaylandDecorationName::IconMaximize));
	_iconMaximized->setAltBuffers(move(buf.iconRestore), move(buf.iconRestoreActive));

	_decors.emplace_back(Rc<WaylandDecoration>::create(this, move(buf.iconMinimize),
			move(buf.iconMinimizeActive), WaylandDecorationName::IconMinimize));
}

} // namespace stappler::xenolith::platform
