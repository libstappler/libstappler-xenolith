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

#include "XLLinuxWaylandDisplay.h"
#include "SPString.h"
#include "SPStringDetail.h"
#include "XLLinuxWaylandWindow.h"
#include "XLContextInfo.h"
#include "SPSharedModule.h"
#include "linux/XLLinux.h"
#include <wayland-client-protocol.h>

#if MODULE_XENOLITH_FONT
#include "XLFontLocale.h"
#endif

#include <wayland-cursor.h>
#include <sys/mman.h>
#include <linux/input.h>

#ifndef XL_WAYLAND_DEBUG
#define XL_WAYLAND_DEBUG 1
#endif

#ifndef XL_WAYLAND_LOG
#if XL_WAYLAND_DEBUG
#define XL_WAYLAND_LOG(...) log::debug("Wayland", __VA_ARGS__)
#else
#define XL_WAYLAND_LOG(...)
#endif
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

// clang-format off
static const xdg_wm_base_listener s_XdgWmBaseListener{
	.ping = [](void *data, xdg_wm_base *xdg_wm_base, uint32_t serial) {
		((WaylandDisplay *)data)->wayland->xdg_wm_base_pong(xdg_wm_base, serial);
	}
};

static const wl_output_listener s_WaylandOutputListener{
	.geometry = [](void *data, wl_output *wl_output, int32_t x, int32_t y, int32_t mmWidth, int32_t mmHeight,
			int32_t subpixel, char const *make, char const *model, int32_t transform) {

		auto out = reinterpret_cast<WaylandOutput *>(data);
		auto geom = WaylandOutput::Geometry{
			IVec2{x, y},
			Extent2(static_cast<uint32_t>(mmWidth), static_cast<uint32_t>(mmHeight)),
			subpixel,
			transform,
			make,
			model
		};

		if (geom != out->geometry) {
			out->geometry = sp::move(geom);
			out->state = WaylandUpdateState::Updating;
		}

		if (out->name.empty()) {
			out->name = toString(make, " ", model);
		}
	},

	.mode = [](void *data, wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
		auto out = reinterpret_cast<WaylandOutput *>(data);

		auto mode = WaylandOutput::Mode{
			Extent2(static_cast<uint32_t>(width), static_cast<uint32_t>(height)),
			static_cast<uint32_t>(refresh),
			flags
		};

		out->newModes.emplace_back(mode);
		out->state = WaylandUpdateState::Updating;
	},

	.done = [](void *data, wl_output *output) {
		auto out = reinterpret_cast<WaylandOutput *>(data);
		if (out->state == WaylandUpdateState::Updating) {
			if (!out->newModes.empty()) {
				out->availableModes = sp::move(out->newModes);
				out->newModes.clear();

				out->currentMode = out->availableModes.front();
				out->preferredMode = out->availableModes.front();
			}

			for (auto &it : out->availableModes) {
				if (hasFlag(it.flags, uint32_t(WL_OUTPUT_MODE_CURRENT))) {
					out->currentMode = it;
				}
				if (hasFlag(it.flags, uint32_t(WL_OUTPUT_MODE_PREFERRED))) {
					out->preferredMode = it;
				}
			}

			out->state = WaylandUpdateState::Done;
		}
	},

	.scale = [](void *data, wl_output *output, int32_t factor) {
		auto out = reinterpret_cast<WaylandOutput *>(data);
		if (out->scale != static_cast<float>(factor)) {
			out->scale = static_cast<float>(factor);
			out->state = WaylandUpdateState::Updating;
		}
	},

	.name = [](void *data, struct wl_output *wl_output, const char *name) {
		auto out = reinterpret_cast<WaylandOutput *>(data);
		out->name = name;
	},

	.description = [](void *data, struct wl_output *wl_output, const char *name) {
		auto out = reinterpret_cast<WaylandOutput *>(data);
		out->desc = name;
	}
};

static struct wl_pointer_listener s_WaylandPointerListener{
	.enter = [](void *data, wl_pointer *wl_pointer, uint32_t serial, wl_surface *surface, wl_fixed_t x, wl_fixed_t y) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		seat->pointerFocus = surface;
		seat->serial = serial;

		if (seat->root->isDecoration(surface)) {
			if (auto decor =
							(WaylandDecoration *)seat->wayland->wl_surface_get_user_data(surface)) {
				if (decor->image != seat->cursorImage) {
					if (seat->cursorTheme) {
						seat->cursorImage = decor->image;
						seat->cursorTheme->setCursor(seat);
					}
				}
				seat->pointerDecorations.emplace(decor);
				decor->onEnter();
			}
			return;
		}

		auto window = reinterpret_cast<WaylandWindow *>(seat->wayland->wl_surface_get_user_data(surface));
		if (window) {
			seat->pointerViews.emplace(window);
			if (window->getCursor() != seat->cursorImage) {
				if (seat->cursorTheme) {
					seat->cursorImage = window->getCursor();
					seat->cursorTheme->setCursor(seat);
				}
			}
			window->handlePointerEnter(x, y);
		}
	},

	.leave = [](void *data, wl_pointer *wl_pointer, uint32_t serial, wl_surface *surface) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);

		if (seat->root->isDecoration(surface)) {
			if (auto decor = (WaylandDecoration *)seat->wayland->wl_surface_get_user_data(surface)) {
				decor->waitForMove = false;
				seat->pointerDecorations.erase(decor);
				decor->onLeave();
			}
		} else if (seat->root->ownsSurface(surface)) {
			auto window = reinterpret_cast<WaylandWindow *>(seat->wayland->wl_surface_get_user_data(surface));
			if (window) {
				window->handlePointerLeave();
				seat->pointerViews.erase(window);
			}
		}

		if (seat->pointerFocus == surface) {
			seat->pointerFocus = NULL;
			seat->cursorImage = WindowLayerFlags::None;
		}
	},

	.motion = [](void *data, wl_pointer *wl_pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerMotion(time, x, y); }
		for (auto &it : seat->pointerDecorations) { it->handleMotion(x, y); }
	},

	.button = [](void *data, wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerButton(serial, time, button, state); }
		for (auto &it : seat->pointerDecorations) { it->handlePress(seat, serial, button, state); }
	},

	.axis = [](void *data, wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerAxis(time, axis, wl_fixed_to_double(value)); }
	},

	.frame = [](void *data, wl_pointer *wl_pointer) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerFrame(); }
	},

	.axis_source = [](void *data, wl_pointer *wl_pointer, uint32_t axis_source) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerAxisSource(axis_source); }
	},

	.axis_stop = [](void *data, wl_pointer *wl_pointer, uint32_t time, uint32_t axis) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerAxisStop(time, axis); }
	},

	.axis_discrete = [](void *data, wl_pointer *wl_pointer, uint32_t axis, int32_t discrete) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerAxisDiscrete(axis, discrete * 120); }
	},

	.axis_value120 = [](void *data, wl_pointer *wl_pointer, uint32_t axis, int32_t value120) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerAxisDiscrete(axis, value120); }
	},

	.axis_relative_direction = [](void *data, struct wl_pointer *wl_pointer, uint32_t axis, uint32_t direction) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerAxisRelativeDirection(axis, direction); }
	}
};

static struct wl_keyboard_listener s_WaylandKeyboardListener{// keymap
	.keymap = [](void *data, wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		if (seat->root->xkb) {
			auto map_shm = ::mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
			if (map_shm != MAP_FAILED) {
				if (seat->state) {
					seat->root->xkb->xkb_state_unref(seat->state);
					seat->state = nullptr;
				}

				if (seat->compose) {
					seat->root->xkb->xkb_compose_state_unref(seat->compose);
					seat->compose = nullptr;
				}

				auto keymap = seat->root->xkb->xkb_keymap_new_from_string(seat->root->xkb->getContext(),
						(const char *)map_shm, xkb_keymap_format(format), XKB_KEYMAP_COMPILE_NO_FLAGS);
				if (keymap) {
					seat->state = seat->root->xkb->xkb_state_new(keymap);
					seat->keyState.controlIndex =
							seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Control");
					seat->keyState.altIndex = seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Mod1");
					seat->keyState.shiftIndex =
							seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Shift");
					seat->keyState.superIndex =
							seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Mod4");
					seat->keyState.capsLockIndex =
							seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Lock");
					seat->keyState.numLockIndex =
							seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Mod2");
					seat->root->xkb->xkb_keymap_unref(keymap);
				}

				String posixLocale;

#if MODULE_XENOLITH_FONT
				auto getLocaleInfo = SharedModule::acquireTypedSymbol<decltype(&locale::getLocaleInfo)>(
						buildconfig::MODULE_XENOLITH_FONT_NAME, "locale::getLocaleInfo");
				if (getLocaleInfo) {
					auto info = getLocaleInfo();
					posixLocale = info.id.getPosixName<Interface>();
				}
#endif
				if (posixLocale.empty()) {
					posixLocale = stappler::platform::getOsLocale().str<Interface>();
				}

				auto composeTable = seat->root->xkb->xkb_compose_table_new_from_locale(
						seat->root->xkb->getContext(), posixLocale.empty() ? "C" : posixLocale.data(),
						XKB_COMPOSE_COMPILE_NO_FLAGS);
				if (composeTable) {
					seat->compose = seat->root->xkb->xkb_compose_state_new(composeTable,
							XKB_COMPOSE_STATE_NO_FLAGS);
					seat->root->xkb->xkb_compose_table_unref(composeTable);
				}

				::munmap(map_shm, size);
			}
		}
		::close(fd);
	},

	.enter = [](void *data, wl_keyboard *wl_keyboard, uint32_t serial, wl_surface *surface,
			struct wl_array *keys) {
		auto seat = (WaylandSeat *)data;
		if (seat->root->ownsSurface(surface)) {
			if (auto view = (WaylandWindow *)seat->wayland->wl_surface_get_user_data(surface)) {
				Vector<uint32_t> keysVec;
				for (uint32_t *it = (uint32_t *)keys->data;
						(const char *)it < ((const char *)keys->data + keys->size); ++it) {
					keysVec.emplace_back(*it);
				}

				seat->keyboardViews.emplace(view);
				view->handleKeyboardEnter(sp::move(keysVec), seat->keyState.modsDepressed,
						seat->keyState.modsLatched, seat->keyState.modsLocked);
			}
		}
	},

	.leave = [](void *data, wl_keyboard *wl_keyboard, uint32_t serial, wl_surface *surface) {
		auto seat = (WaylandSeat *)data;
		if (seat->root->ownsSurface(surface)) {
			if (auto view = (WaylandWindow *)seat->wayland->wl_surface_get_user_data(surface)) {
				view->handleKeyboardLeave();
				seat->keyboardViews.erase(view);
			}
		}
	},

	.key = [](void *data, wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key,
			uint32_t state) {
		auto seat = (WaylandSeat *)data;
		for (auto &it : seat->keyboardViews) { it->handleKey(time, key, state); }
	},

	.modifiers = [](void *data, wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed,
			uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
		auto seat = (WaylandSeat *)data;
		if (seat->state) {
			seat->root->xkb->xkb_state_update_mask(seat->state, mods_depressed, mods_latched,
					mods_locked, 0, 0, group);
			seat->keyState.modsDepressed = mods_depressed;
			seat->keyState.modsLatched = mods_latched;
			seat->keyState.modsLocked = mods_locked;
			for (auto &it : seat->keyboardViews) {
				it->handleKeyModifiers(mods_depressed, mods_latched, mods_locked);
			}
		}
	},

	.repeat_info = [](void *data, wl_keyboard *wl_keyboard, int32_t rate, int32_t delay) {
		auto seat = (WaylandSeat *)data;
		seat->keyState.keyRepeatRate = rate;
		seat->keyState.keyRepeatDelay = delay;
		seat->keyState.keyRepeatInterval = 1'000'000 / rate;
	}
};

static const wl_registry_listener s_WaylandRegistryListener{
	.global = [](void *data, struct wl_registry *registry, uint32_t name, const char *interface,
			uint32_t version) {
		auto display = (WaylandDisplay *)data;
		auto wayland = display->wayland.get();

		StringView iname = StringView(interface);

		if (iname == StringView(wayland->wl_compositor_interface->name)) {
			display->compositor = static_cast<wl_compositor *>(
				wayland->wl_registry_bind(registry, name,
					wayland->wl_compositor_interface,
					std::min(version, uint32_t(wayland->wl_compositor_interface->version))));

		} else if (iname == StringView(wayland->wl_subcompositor_interface->name)) {
			display->subcompositor = static_cast<wl_subcompositor *>(
				wayland->wl_registry_bind(registry, name,
					wayland->wl_subcompositor_interface,
					std::min(version, uint32_t(wayland->wl_subcompositor_interface->version))));

		} else if (iname == StringView(wayland->wl_output_interface->name)) {
			auto out = Rc<WaylandOutput>::create(wayland, registry, name, version);
			display->outputs.emplace_back(move(out));

		} else if (iname == StringView(wayland->wp_viewporter_interface->name)) {
			display->viewporter = static_cast<struct wp_viewporter *>(
					wayland->wl_registry_bind(registry, name,
						wayland->wp_viewporter_interface,
						std::min(version, uint32_t(wayland->wp_viewporter_interface->version))));

		} else if (iname == StringView(wayland->xdg_wm_base_interface->name)) {
			display->xdgWmBase = static_cast<struct xdg_wm_base *>(
				wayland->wl_registry_bind(registry, name,
					wayland->xdg_wm_base_interface,
					std::min(int(version), wayland->xdg_wm_base_interface->version)));

			wayland->xdg_wm_base_add_listener(display->xdgWmBase, &s_XdgWmBaseListener, display);
		} else if (iname == StringView(wayland->wl_shm_interface->name)) {
			display->shm = Rc<WaylandShm>::create(wayland, registry, name, version);

		} else if (iname == StringView(wayland->wl_seat_interface->name)) {
			display->seat = Rc<WaylandSeat>::create(wayland, display, registry, name, version);

		} else {
			XL_WAYLAND_LOG("handleGlobal: '", interface, "', version: ", version, ", name: ", name);
		}
	},
	.global_remove = [](void *data, struct wl_registry *registry, uint32_t name) {
	//auto display = (WaylandDisplay *)data;
}};

static struct wl_shm_listener s_WaylandShmListener = {
	.format = [](void *data, wl_shm *shm, uint32_t format) {
		reinterpret_cast<WaylandShm *>(data)->format = format;
	}
};

static struct wl_surface_listener cursor_surface_listener = {
	.enter = [](void *data, wl_surface *surface, wl_output *output) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		if (!seat->wayland->ownsProxy(output)) {
			return;
		}

		auto out = (WaylandOutput *)seat->wayland->wl_output_get_user_data(output);
		seat->pointerOutputs.emplace(out);
		seat->tryUpdateCursor();
	},
	.leave = [](void *data, struct wl_surface *wl_surface, struct wl_output *output) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		if (!seat->wayland->ownsProxy(output)) {
			return;
		}

		auto out = (WaylandOutput *)seat->wayland->wl_output_get_user_data(output);
		seat->pointerOutputs.erase(out);
	},
	.preferred_buffer_scale = [](void *data, struct wl_surface *wl_surface, int32_t factor) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		seat->pointerScale = float(factor);
	},
	.preferred_buffer_transform = [](void *data, struct wl_surface *wl_surface, uint32_t transform) { }
};

// clang-format on

WaylandDisplay::~WaylandDisplay() {
	if (xdgWmBase) {
		wayland->xdg_wm_base_destroy(xdgWmBase);
		xdgWmBase = nullptr;
	}
	if (compositor) {
		wayland->wl_compositor_destroy(compositor);
		compositor = nullptr;
	}
	if (subcompositor) {
		wayland->wl_subcompositor_destroy(subcompositor);
		subcompositor = nullptr;
	}
	if (viewporter) {
		wayland->wp_viewporter_destroy(viewporter);
		viewporter = nullptr;
	}

	shm = nullptr;
	seat = nullptr;
	outputs.clear();

	if (display) {
		wayland->wl_display_disconnect(display);
		display = nullptr;
	}
}

bool WaylandDisplay::init(NotNull<WaylandLibrary> lib, NotNull<XkbLibrary> xkbLib, StringView d) {
	wayland = lib;

	display = wayland->wl_display_connect(
			d.empty() ? nullptr : (d.terminated() ? d.data() : d.str<Interface>().data()));

	struct wl_registry *registry = wayland->wl_display_get_registry(display);

	wayland->wl_registry_add_listener(registry, &s_WaylandRegistryListener, this);
	wayland->wl_display_roundtrip(display); // registry
	wayland->wl_display_roundtrip(display); // seats and outputs
	wayland->wl_registry_destroy(registry);

	xkb = xkbLib;
	return true;
}

wl_surface *WaylandDisplay::createSurface(WaylandWindow *view) {
	auto surface = wayland->wl_compositor_create_surface(compositor);
	wayland->wl_surface_set_user_data(surface, view);
	wayland->wl_proxy_set_tag((struct wl_proxy *)surface, &s_XenolithWaylandTag);
	surfaces.emplace(surface);
	windows.emplace(view);
	return surface;
}

void WaylandDisplay::destroySurface(WaylandWindow *window) {
	seat->clearWindow(window);

	auto surface = window->getSurface();
	surfaces.erase(window->getSurface());
	windows.erase(window);
	wayland->wl_surface_destroy(surface);
}

wl_surface *WaylandDisplay::createDecorationSurface(WaylandDecoration *decor) {
	auto surface = wayland->wl_compositor_create_surface(compositor);
	wayland->wl_surface_set_user_data(surface, decor);
	wayland->wl_proxy_set_tag((struct wl_proxy *)surface, &s_XenolithWaylandTag);
	decorations.emplace(surface);
	return surface;
}

void WaylandDisplay::destroyDecorationSurface(wl_surface *surface) {
	decorations.erase(surface);
	wayland->wl_surface_destroy(surface);
}

bool WaylandDisplay::ownsSurface(wl_surface *surface) const {
	return surfaces.find(surface) != surfaces.end();
}

bool WaylandDisplay::isDecoration(wl_surface *surface) const {
	return decorations.find(surface) != decorations.end();
}

bool WaylandDisplay::flush() { return wayland->wl_display_flush(display) != -1; }

bool WaylandDisplay::poll() {
	if (seatDirty) {
		seat->update();
	}

	while (wayland->wl_display_prepare_read(display) != 0) {
		wayland->wl_display_dispatch_pending(display);
	}

	wayland->wl_display_read_events(display);
	wayland->wl_display_dispatch_pending(display);

	for (auto &it : windows) { it->dispatchPendingEvents(); }

	flush();

	return true;
}

int WaylandDisplay::getFd() const { return wayland->wl_display_get_fd(display); }

void WaylandDisplay::updateThemeInfo(const ThemeInfo &theme) {
	if (seat) {
		if ((seat->capabilities & WL_SEAT_CAPABILITY_POINTER) != 0) {
			seat->setCursors(theme.cursorTheme, theme.cursorSize);
			seat->doubleClickInterval = theme.doubleClickInterval;
		}
	}
}

void WaylandDisplay::notifyScreenChange() {
	for (auto &it : windows) { it->notifyScreenChange(); }
}

WaylandCursorTheme::~WaylandCursorTheme() {
	if (cursorTheme) {
		wayland->wl_cursor_theme_destroy(cursorTheme);
		cursorTheme = nullptr;
	}
}

bool WaylandCursorTheme::init(WaylandDisplay *display, StringView name, int size) {
	wayland = display->wayland;
	cursorSize = size;
	cursorName = name.str<Interface>();
	cursorTheme = wayland->wl_cursor_theme_load(name.data(), size, display->shm->shm);

	if (cursorTheme) {
		for (uint32_t i = 0; i < toInt(WindowLayerFlags::CursorMask); ++i) {
			wl_cursor *c = nullptr;
			auto names = getCursorNames(WindowLayerFlags(i));
			for (auto &it : names) {
				c = wayland->wl_cursor_theme_get_cursor(cursorTheme, it.data());
				if (c) {
					cursors.emplace_back(c);
					break;
				}
			}
			if (!c) {
				cursors.emplace_back(nullptr);
			}
		}
		return true;
	}

	return false;
}

void WaylandCursorTheme::setCursor(WaylandSeat *seat) {
	setCursor(seat->pointer, seat->cursorSurface, seat->serial, seat->cursorImage,
			seat->pointerScale);
}

void WaylandCursorTheme::setCursor(wl_pointer *pointer, wl_surface *cursorSurface, uint32_t serial,
		WindowLayerFlags img, int scale) {
	auto cursorIndex = toInt(img & WindowLayerFlags::CursorMask);
	if (!cursorTheme || cursors.size() <= size_t(cursorIndex)) {
		return;
	}

	auto cursor = cursors.at(cursorIndex);
	if (!cursor) {
		cursor = cursors[1]; // default arrow
	}

	auto image = cursor->images[0];
	auto buffer = wayland->wl_cursor_image_get_buffer(image);
	wayland->wl_pointer_set_cursor(pointer, serial, cursorSurface, image->hotspot_x / scale,
			image->hotspot_y / scale);
	wayland->wl_surface_attach(cursorSurface, buffer, 0, 0);
	wayland->wl_surface_set_buffer_scale(cursorSurface, scale);
	wayland->wl_surface_damage_buffer(cursorSurface, 0, 0, image->width, image->height);
	wayland->wl_surface_commit(cursorSurface);
}

WaylandShm::~WaylandShm() {
	if (shm) {
		wayland->wl_shm_destroy(shm);
		shm = nullptr;
	}
}

bool WaylandShm::init(const Rc<WaylandLibrary> &lib, wl_registry *registry, uint32_t name,
		uint32_t version) {
	wayland = lib;
	id = name;
	shm = static_cast<wl_shm *>(wayland->wl_registry_bind(registry, name, wayland->wl_shm_interface,
			std::min(version, uint32_t(wayland->wl_shm_interface->version))));
	wayland->wl_shm_set_user_data(shm, this);
	wayland->wl_shm_add_listener(shm, &s_WaylandShmListener, this);
	wayland->wl_proxy_set_tag((struct wl_proxy *)shm, &s_XenolithWaylandTag);
	return true;
}

WaylandOutput::~WaylandOutput() {
	if (output) {
		wayland->wl_output_destroy(output);
		output = nullptr;
	}
}

bool WaylandOutput::init(const Rc<WaylandLibrary> &lib, wl_registry *registry, uint32_t name,
		uint32_t version) {
	wayland = lib;
	id = name;
	output = static_cast<wl_output *>(
			wayland->wl_registry_bind(registry, name, wayland->wl_output_interface,
					std::min(version, uint32_t(wayland->wl_output_interface->version))));
	wayland->wl_output_set_user_data(output, this);
	wayland->wl_output_add_listener(output, &s_WaylandOutputListener, this);
	wayland->wl_proxy_set_tag((struct wl_proxy *)output, &s_XenolithWaylandTag);
	return true;
}

String WaylandOutput::description() const {
	StringStream stream;
	stream << geometry.make << " " << geometry.model << ": " << currentMode.size.width << "x"
		   << currentMode.size.height << "@" << currentMode.rate / 1'000 << "Hz (x" << scale
		   << ");";
	if (currentMode.flags & WL_OUTPUT_MODE_CURRENT) {
		stream << " Current;";
	}
	if (currentMode.flags & WL_OUTPUT_MODE_PREFERRED) {
		stream << " Preferred;";
	}
	if (!desc.empty()) {
		stream << " " << desc << ";";
	}
	return stream.str();
}

static struct wl_touch_listener s_WaylandTouchListener{// down
	[](void *data, wl_touch *touch, uint32_t serial, uint32_t time, wl_surface *surface, int32_t id,
			wl_fixed_t x, wl_fixed_t y) {

},

	// up
	[](void *data, wl_touch *touch, uint32_t serial, uint32_t time, int32_t id) {

},

	// motion
	[](void *data, wl_touch *touch, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y) {

},

	// frame
	[](void *data, wl_touch *touch) {

},

	// cancel
	[](void *data, wl_touch *touch) {

},

	// shape
	[](void *data, wl_touch *touch, int32_t id, wl_fixed_t major, wl_fixed_t minor) {

},

	// orientation
	[](void *data, wl_touch *touch, int32_t id, wl_fixed_t orientation) {

}};

static struct wl_seat_listener s_WaylandSeatListener{
	[](void *data, struct wl_seat *wl_seat, uint32_t capabilities) {
	auto seat = (WaylandSeat *)data;
	seat->capabilities = capabilities;
	seat->root->seatDirty = true;

	seat->update();
}, [](void *data, struct wl_seat *wl_seat, const char *name) {
	auto seat = (WaylandSeat *)data;
	seat->name = name;
}};

WaylandSeat::~WaylandSeat() {
	if (state) {
		root->xkb->xkb_state_unref(state);
		state = nullptr;
	}
	if (compose) {
		root->xkb->xkb_compose_state_unref(compose);
		compose = nullptr;
	}
	if (seat) {
		wayland->wl_seat_destroy(seat);
		seat = nullptr;
	}
}

bool WaylandSeat::init(NotNull<WaylandLibrary> lib, NotNull<WaylandDisplay> view,
		wl_registry *registry, uint32_t name, uint32_t version) {
	wayland = lib;
	root = view;
	id = name;
	if (version >= 5U) {
		hasPointerFrames = true;
	}
	seat = static_cast<wl_seat *>(
			wayland->wl_registry_bind(registry, name, wayland->wl_seat_interface,
					std::min(version, uint32_t(wayland->wl_seat_interface->version))));

	wayland->wl_seat_set_user_data(seat, this);
	wayland->wl_seat_add_listener(seat, &s_WaylandSeatListener, this);
	wayland->wl_proxy_set_tag((struct wl_proxy *)seat, &s_XenolithWaylandTag);

	return true;
}

void WaylandSeat::setCursors(StringView theme, int32_t size) {
	size *= pointerScale;

	if (!cursorTheme || cursorTheme->cursorSize != size || cursorTheme->cursorName != theme) {
		cursorTheme = Rc<WaylandCursorTheme>::create(root, theme, size);
	}

	if (!cursorSurface) {
		cursorSurface = wayland->wl_compositor_create_surface(root->compositor);
		wayland->wl_surface_add_listener(cursorSurface, &cursor_surface_listener, this);
	}
}

void WaylandSeat::tryUpdateCursor() {
	if (cursorTheme) {
		setCursors(cursorTheme->cursorName, cursorTheme->cursorSize);
	}
	if (cursorTheme) {
		cursorTheme->setCursor(this);
	}
}

void WaylandSeat::update() {
	if (!root->seatDirty) {
		return;
	}

	root->seatDirty = false;

	if ((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0 && !pointer) {
		pointer = wayland->wl_seat_get_pointer(seat);
		wayland->wl_pointer_add_listener(pointer, &s_WaylandPointerListener, this);
		pointerScale = 1;
		if (cursorTheme) {
			setCursors(cursorTheme->cursorName, cursorTheme->cursorSize);
		}
	} else if ((capabilities & WL_SEAT_CAPABILITY_POINTER) == 0 && pointer) {
		wayland->wl_pointer_release(pointer);
		pointer = NULL;
	}

	if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0 && !keyboard) {
		keyboard = wayland->wl_seat_get_keyboard(seat);
		wayland->wl_keyboard_add_listener(keyboard, &s_WaylandKeyboardListener, this);
	} else if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) == 0 && keyboard) {
		wayland->wl_keyboard_release(keyboard);
		keyboard = NULL;
	}

	if ((capabilities & WL_SEAT_CAPABILITY_TOUCH) != 0 && !touch) {
		touch = wayland->wl_seat_get_touch(seat);
		wayland->wl_touch_add_listener(touch, &s_WaylandTouchListener, this);
	} else if ((capabilities & WL_SEAT_CAPABILITY_TOUCH) == 0 && touch) {
		wayland->wl_touch_release(touch);
		touch = NULL;
	}

	// wayland->wl_display_roundtrip(root->display);
}

void WaylandSeat::clearWindow(WaylandWindow *window) {
	pointerViews.erase(window);
	keyboardViews.erase(window);
}

core::InputKeyCode WaylandSeat::translateKey(uint32_t scancode) const {
	if (scancode < sizeof(keyState.keycodes) / sizeof(keyState.keycodes[0])) {
		return keyState.keycodes[scancode];
	}

	return core::InputKeyCode::Unknown;
}

xkb_keysym_t WaylandSeat::composeSymbol(xkb_keysym_t sym,
		core::InputKeyComposeState &composeState) const {
	if (sym == XKB_KEY_NoSymbol || !compose) {
		return sym;
	}
	if (root->xkb->xkb_compose_state_feed(compose, sym) != XKB_COMPOSE_FEED_ACCEPTED) {
		return sym;
	}
	switch (root->xkb->xkb_compose_state_get_status(compose)) {
	case XKB_COMPOSE_COMPOSED:
		composeState = core::InputKeyComposeState::Composed;
		return root->xkb->xkb_compose_state_get_one_sym(compose);
	case XKB_COMPOSE_COMPOSING: composeState = core::InputKeyComposeState::Composing; return sym;
	case XKB_COMPOSE_CANCELLED: return sym;
	case XKB_COMPOSE_NOTHING:
	default: return sym;
	}
}

WaylandDecoration::~WaylandDecoration() {
	if (viewport) {
		wayland->wp_viewport_destroy(viewport);
		viewport = nullptr;
	}
	if (subsurface) {
		wayland->wl_subsurface_destroy(subsurface);
		subsurface = nullptr;
	}
	if (surface) {
		display->destroyDecorationSurface(surface);
		surface = nullptr;
	}
}

bool WaylandDecoration::init(WaylandWindow *view, Rc<WaylandBuffer> &&b, Rc<WaylandBuffer> &&a,
		WaylandDecorationName n) {
	root = view;
	display = view->getDisplay();
	wayland = display->wayland;
	surface = display->createDecorationSurface(this);
	name = n;
	switch (n) {
	case WaylandDecorationName::RightSide: image = WindowLayerFlags::CursorResizeRight; break;
	case WaylandDecorationName::TopRightCorner:
		image = WindowLayerFlags::CursorResizeTopRight;
		break;
	case WaylandDecorationName::TopSide: image = WindowLayerFlags::CursorResizeTop; break;
	case WaylandDecorationName::TopLeftCorner: image = WindowLayerFlags::CursorResizeTopLeft; break;
	case WaylandDecorationName::BottomRightCorner:
		image = WindowLayerFlags::CursorResizeBottomRight;
		break;
	case WaylandDecorationName::BottomSide: image = WindowLayerFlags::CursorResizeBottom; break;
	case WaylandDecorationName::BottomLeftCorner:
		image = WindowLayerFlags::CursorResizeBottomLeft;
		break;
	case WaylandDecorationName::LeftSide: image = WindowLayerFlags::CursorResizeLeft; break;
	default: image = WindowLayerFlags::CursorArrow; break;
	}
	buffer = move(b);
	if (a) {
		active = move(a);
	}

	auto parent = root->getSurface();

	subsurface = wayland->wl_subcompositor_get_subsurface(display->subcompositor, surface, parent);
	wayland->wl_subsurface_place_below(subsurface, parent);
	wayland->wl_subsurface_set_sync(subsurface);

	viewport = wayland->wp_viewporter_get_viewport(display->viewporter, surface);
	wayland->wl_surface_attach(surface, buffer->buffer, 0, 0);

	dirty = true;

	return true;
}

bool WaylandDecoration::init(WaylandWindow *view, Rc<WaylandBuffer> &&b, WaylandDecorationName n) {
	return init(view, move(b), nullptr, n);
}

void WaylandDecoration::setAltBuffers(Rc<WaylandBuffer> &&b, Rc<WaylandBuffer> &&a) {
	altBuffer = move(b);
	altActive = move(a);
}

void WaylandDecoration::handlePress(WaylandSeat *seat, uint32_t s, uint32_t button,
		uint32_t state) {
	serial = s;
	waitForMove = false;
	if (isTouchable()) {
		if (state == WL_POINTER_BUTTON_STATE_RELEASED && button == BTN_LEFT) {
			root->handleDecorationPress(this, serial, button);
		}
	} else if (image == WindowLayerFlags::CursorArrow) {
		if (state == WL_POINTER_BUTTON_STATE_PRESSED && button == BTN_RIGHT) {
			root->handleDecorationPress(this, serial, button, false);
		} else if (state == WL_POINTER_BUTTON_STATE_RELEASED && button == BTN_LEFT) {
			auto n = Time::now().toMicros();
			if (n - lastTouch < seat->doubleClickInterval) {
				root->handleDecorationPress(this, serial, button, true);
			}
			lastTouch = n;
		} else if (state == WL_POINTER_BUTTON_STATE_PRESSED && button == BTN_LEFT) {
			waitForMove = true;
			//root->handleDecorationPress(this, serial, button);
		}
	} else {
		if (state == WL_POINTER_BUTTON_STATE_PRESSED && button == BTN_LEFT) {
			root->handleDecorationPress(this, serial, button);
		}
	}
}

void WaylandDecoration::handleMotion(wl_fixed_t x, wl_fixed_t y) {
	pointerX = x;
	pointerY = y;
	if (waitForMove) {
		root->handleDecorationPress(this, serial, 0);
	}
}

void WaylandDecoration::onEnter() {
	if (isTouchable() && !isActive) {
		isActive = true;
		auto &b = (active && isActive) ? active : buffer;
		wayland->wl_surface_attach(surface, b->buffer, 0, 0);
		wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
		dirty = true;
	}
}

void WaylandDecoration::onLeave() {
	if (isTouchable() && isActive) {
		isActive = false;
		auto &b = (active && isActive) ? active : buffer;
		wayland->wl_surface_attach(surface, b->buffer, 0, 0);
		wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
		dirty = true;
	}
}

void WaylandDecoration::setActive(bool val) {
	if (!isTouchable()) {
		if (val != isActive) {
			isActive = val;
			auto &b = (active && isActive) ? active : buffer;
			wayland->wl_surface_attach(surface, b->buffer, 0, 0);
			wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
			dirty = true;
		}
	}
}

void WaylandDecoration::setVisible(bool val) {
	if (val != visible) {
		visible = val;
		if (visible) {
			auto &b = (active && isActive) ? active : buffer;
			wayland->wl_surface_attach(surface, b->buffer, 0, 0);
			wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
		} else {
			wayland->wl_surface_attach(surface, nullptr, 0, 0);
		}
		dirty = true;
	}
}

void WaylandDecoration::setAlternative(bool val) {
	if (!altBuffer || !altActive) {
		return;
	}

	if (alternative != val) {
		alternative = val;
		auto tmpA = move(altBuffer);
		auto tmpB = move(altActive);

		altBuffer = move(buffer);
		altActive = move(active);

		buffer = move(tmpA);
		active = move(tmpB);

		auto &b = (active && isActive) ? active : buffer;
		wayland->wl_surface_attach(surface, b->buffer, 0, 0);
		wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
		dirty = true;
	}
}

void WaylandDecoration::setGeometry(int32_t x, int32_t y, int32_t width, int32_t height) {
	if (_x == x && _y == y && _width == width && _height == height) {
		return;
	}

	_x = x;
	_y = y;
	_width = width;
	_height = height;

	wayland->wl_subsurface_set_position(subsurface, _x, _y);
	wayland->wp_viewport_set_destination(viewport, _width, _height);

	auto &b = (active && isActive) ? active : buffer;
	wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
	dirty = true;
}

bool WaylandDecoration::commit() {
	if (dirty) {
		wayland->wl_surface_commit(surface);
		dirty = false;
		return true;
	}
	return false;
}

bool WaylandDecoration::isTouchable() const {
	switch (name) {
	case WaylandDecorationName::IconClose:
	case WaylandDecorationName::IconMaximize:
	case WaylandDecorationName::IconMinimize:
	case WaylandDecorationName::IconRestore: return true; break;
	default: break;
	}
	return false;
}

} // namespace stappler::xenolith::platform
