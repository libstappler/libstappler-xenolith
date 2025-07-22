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

#ifndef XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDLIBRARY_H_
#define XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDLIBRARY_H_

#include "XLContextInfo.h"

#if LINUX

#include "XLLinuxWaylandProtocol.h"
#include "linux/XLLinuxXkbLibrary.h"

#include <wayland-client.h>
#include <wayland-cursor.h>

#include <libdecor-0/libdecor.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class WaylandWindow;

struct WaylandShm;
struct WaylandSeat;
struct WaylandOutput;
struct WaylandDecoration;

struct XdgInterface;
struct ViewporterInterface;

class SP_PUBLIC WaylandLibrary : public Ref {
public:
	virtual ~WaylandLibrary();
	WaylandLibrary();

	bool init();
	void close();

	decltype(&_xl_null_fn) _wl_first_fn = &_xl_null_fn;

	XL_DEFINE_PROTO(wl_registry_interface)
	XL_DEFINE_PROTO(wl_compositor_interface)
	XL_DEFINE_PROTO(wl_output_interface)
	XL_DEFINE_PROTO(wl_seat_interface)
	XL_DEFINE_PROTO(wl_surface_interface)
	XL_DEFINE_PROTO(wl_region_interface)
	XL_DEFINE_PROTO(wl_callback_interface)
	XL_DEFINE_PROTO(wl_pointer_interface)
	XL_DEFINE_PROTO(wl_keyboard_interface)
	XL_DEFINE_PROTO(wl_touch_interface)
	XL_DEFINE_PROTO(wl_shm_interface)
	XL_DEFINE_PROTO(wl_subcompositor_interface)
	XL_DEFINE_PROTO(wl_subsurface_interface)
	XL_DEFINE_PROTO(wl_shm_pool_interface)
	XL_DEFINE_PROTO(wl_buffer_interface)

	XL_DEFINE_PROTO(wl_display_connect)
	XL_DEFINE_PROTO(wl_display_get_fd)
	XL_DEFINE_PROTO(wl_display_dispatch)
	XL_DEFINE_PROTO(wl_display_dispatch_pending)
	XL_DEFINE_PROTO(wl_display_prepare_read)
	XL_DEFINE_PROTO(wl_display_flush)
	XL_DEFINE_PROTO(wl_display_read_events)
	XL_DEFINE_PROTO(wl_display_disconnect)
	XL_DEFINE_PROTO(wl_proxy_marshal_flags)
	XL_DEFINE_PROTO(wl_proxy_get_version)
	XL_DEFINE_PROTO(wl_proxy_add_listener)
	XL_DEFINE_PROTO(wl_proxy_set_user_data)
	XL_DEFINE_PROTO(wl_proxy_get_user_data)
	XL_DEFINE_PROTO(wl_proxy_set_tag)
	XL_DEFINE_PROTO(wl_proxy_get_tag)
	XL_DEFINE_PROTO(wl_proxy_destroy)
	XL_DEFINE_PROTO(wl_display_roundtrip)

	decltype(&_xl_null_fn) _wl_last_fn = &_xl_null_fn;

	XL_DEFINE_PROTO(wp_viewporter_interface)
	XL_DEFINE_PROTO(wp_viewport_interface)
	XL_DEFINE_PROTO(xdg_wm_base_interface)
	XL_DEFINE_PROTO(xdg_positioner_interface)
	XL_DEFINE_PROTO(xdg_surface_interface)
	XL_DEFINE_PROTO(xdg_toplevel_interface)
	XL_DEFINE_PROTO(xdg_popup_interface)

	ViewporterInterface *viewporter = nullptr;
	XdgInterface *xdg = nullptr;

	bool ownsProxy(wl_proxy *);
	bool ownsProxy(wl_output *);
	bool ownsProxy(wl_surface *);

	struct wl_registry *wl_display_get_registry(struct wl_display *wl_display) {
		struct wl_proxy *registry;

		registry = wl_proxy_marshal_flags((struct wl_proxy *)wl_display, WL_DISPLAY_GET_REGISTRY,
				wl_registry_interface, wl_proxy_get_version((struct wl_proxy *)wl_display), 0,
				NULL);

		return (struct wl_registry *)registry;
	}

	void *wl_registry_bind(struct wl_registry *wl_registry, uint32_t name,
			const struct wl_interface *interface, uint32_t version) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy *)wl_registry, WL_REGISTRY_BIND, interface,
				version, 0, name, interface->name, version, NULL);

		return (void *)id;
	}

	int wl_registry_add_listener(struct wl_registry *wl_registry,
			const struct wl_registry_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy *)wl_registry, (void (**)(void))listener,
				data);
	}

	void wl_registry_destroy(struct wl_registry *wl_registry) {
		wl_proxy_destroy((struct wl_proxy *)wl_registry);
	}

	struct wl_shm_pool *wl_shm_create_pool(struct wl_shm *wl_shm, int32_t fd, int32_t size) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy *)wl_shm, WL_SHM_CREATE_POOL,
				wl_shm_pool_interface, wl_proxy_get_version((struct wl_proxy *)wl_shm), 0, NULL, fd,
				size);

		return (struct wl_shm_pool *)id;
	}

	int wl_shm_add_listener(struct wl_shm *wl_shm, const struct wl_shm_listener *listener,
			void *data) {
		return wl_proxy_add_listener((struct wl_proxy *)wl_shm, (void (**)(void))listener, data);
	}

	void wl_shm_set_user_data(struct wl_shm *wl_shm, void *user_data) {
		wl_proxy_set_user_data((struct wl_proxy *)wl_shm, user_data);
	}

	void *wl_shm_get_user_data(struct wl_shm *wl_shm) {
		return wl_proxy_get_user_data((struct wl_proxy *)wl_shm);
	}

	void wl_shm_destroy(struct wl_shm *wl_shm) { wl_proxy_destroy((struct wl_proxy *)wl_shm); }

	void wl_shm_pool_destroy(struct wl_shm_pool *wl_shm_pool) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_shm_pool, WL_SHM_POOL_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_shm_pool), WL_MARSHAL_FLAG_DESTROY);
	}

	struct wl_buffer *wl_shm_pool_create_buffer(struct wl_shm_pool *wl_shm_pool, int32_t offset,
			int32_t width, int32_t height, int32_t stride, uint32_t format) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy *)wl_shm_pool, WL_SHM_POOL_CREATE_BUFFER,
				wl_buffer_interface, wl_proxy_get_version((struct wl_proxy *)wl_shm_pool), 0, NULL,
				offset, width, height, stride, format);

		return (struct wl_buffer *)id;
	}

	void wl_buffer_destroy(struct wl_buffer *wl_buffer) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_buffer, WL_BUFFER_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_buffer), WL_MARSHAL_FLAG_DESTROY);
	}

	struct wl_surface *wl_compositor_create_surface(struct wl_compositor *wl_compositor) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy *)wl_compositor, WL_COMPOSITOR_CREATE_SURFACE,
				wl_surface_interface, wl_proxy_get_version((struct wl_proxy *)wl_compositor), 0,
				NULL);

		return (struct wl_surface *)id;
	}

	struct wl_region *wl_compositor_create_region(struct wl_compositor *wl_compositor) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy *)wl_compositor, WL_COMPOSITOR_CREATE_REGION,
				wl_region_interface, wl_proxy_get_version((struct wl_proxy *)wl_compositor), 0,
				NULL);

		return (struct wl_region *)id;
	}

	void wl_compositor_destroy(struct wl_compositor *wl_compositor) {
		wl_proxy_destroy((struct wl_proxy *)wl_compositor);
	}

	void wl_region_add(struct wl_region *wl_region, int32_t x, int32_t y, int32_t width,
			int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_region, WL_REGION_ADD, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_region), 0, x, y, width, height);
	}

	void wl_region_destroy(struct wl_region *wl_region) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_region, WL_REGION_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_region), WL_MARSHAL_FLAG_DESTROY);
	}

	int wl_surface_add_listener(struct wl_surface *wl_surface,
			const struct wl_surface_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy *)wl_surface, (void (**)(void))listener,
				data);
	}

	void wl_surface_attach(struct wl_surface *wl_surface, struct wl_buffer *buffer, int32_t x,
			int32_t y) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_surface, WL_SURFACE_ATTACH, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_surface), 0, buffer, x, y);
	}

	void wl_surface_set_buffer_scale(struct wl_surface *wl_surface, int32_t scale) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_surface, WL_SURFACE_SET_BUFFER_SCALE, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_surface), 0, scale);
	}

	void wl_surface_commit(struct wl_surface *wl_surface) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_surface, WL_SURFACE_COMMIT, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_surface), 0);
	}

	void wl_surface_damage(struct wl_surface *wl_surface, int32_t x, int32_t y, int32_t width,
			int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_surface, WL_SURFACE_DAMAGE, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_surface), 0, x, y, width, height);
	}

	void wl_surface_damage_buffer(struct wl_surface *wl_surface, int32_t x, int32_t y,
			int32_t width, int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_surface, WL_SURFACE_DAMAGE_BUFFER, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_surface), 0, x, y, width, height);
	}

	void wl_surface_set_opaque_region(struct wl_surface *wl_surface, struct wl_region *region) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_surface, WL_SURFACE_SET_OPAQUE_REGION, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_surface), 0, region);
	}

	void wl_surface_set_user_data(struct wl_surface *wl_surface, void *user_data) {
		wl_proxy_set_user_data((struct wl_proxy *)wl_surface, user_data);
	}

	void *wl_surface_get_user_data(struct wl_surface *wl_surface) {
		return wl_proxy_get_user_data((struct wl_proxy *)wl_surface);
	}

	void wl_surface_destroy(struct wl_surface *wl_surface) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_surface, WL_SURFACE_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_surface), WL_MARSHAL_FLAG_DESTROY);
	}

	struct wl_subsurface *wl_subcompositor_get_subsurface(struct wl_subcompositor *wl_subcompositor,
			struct wl_surface *surface, struct wl_surface *parent) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy *)wl_subcompositor,
				WL_SUBCOMPOSITOR_GET_SUBSURFACE, wl_subsurface_interface,
				wl_proxy_get_version((struct wl_proxy *)wl_subcompositor), 0, NULL, surface,
				parent);

		return (struct wl_subsurface *)id;
	}

	void wl_subcompositor_destroy(struct wl_subcompositor *wl_subcompositor) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_subcompositor, WL_SUBCOMPOSITOR_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_subcompositor), WL_MARSHAL_FLAG_DESTROY);
	}

	void wl_subsurface_set_position(struct wl_subsurface *wl_subsurface, int32_t x, int32_t y) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_subsurface, WL_SUBSURFACE_SET_POSITION, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_subsurface), 0, x, y);
	}

	void wl_subsurface_place_above(struct wl_subsurface *wl_subsurface,
			struct wl_surface *sibling) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_subsurface, WL_SUBSURFACE_PLACE_ABOVE, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_subsurface), 0, sibling);
	}

	void wl_subsurface_place_below(struct wl_subsurface *wl_subsurface,
			struct wl_surface *sibling) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_subsurface, WL_SUBSURFACE_PLACE_BELOW, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_subsurface), 0, sibling);
	}

	void wl_subsurface_set_sync(struct wl_subsurface *wl_subsurface) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_subsurface, WL_SUBSURFACE_SET_SYNC, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_subsurface), 0);
	}

	void wl_subsurface_set_desync(struct wl_subsurface *wl_subsurface) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_subsurface, WL_SUBSURFACE_SET_DESYNC, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_subsurface), 0);
	}

	void wl_subsurface_destroy(struct wl_subsurface *wl_subsurface) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_subsurface, WL_SUBSURFACE_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_subsurface), WL_MARSHAL_FLAG_DESTROY);
	}

	int wl_callback_add_listener(struct wl_callback *wl_callback,
			const struct wl_callback_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy *)wl_callback, (void (**)(void))listener,
				data);
	}

	void wl_callback_destroy(struct wl_callback *wl_callback) {
		wl_proxy_destroy((struct wl_proxy *)wl_callback);
	}

	int wl_output_add_listener(struct wl_output *wl_output,
			const struct wl_output_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy *)wl_output, (void (**)(void))listener, data);
	}

	struct wl_callback *wl_surface_frame(struct wl_surface *wl_surface) {
		struct wl_proxy *callback;

		callback = wl_proxy_marshal_flags((struct wl_proxy *)wl_surface, WL_SURFACE_FRAME,
				wl_callback_interface, wl_proxy_get_version((struct wl_proxy *)wl_surface), 0,
				NULL);

		return (struct wl_callback *)callback;
	}

	void wl_output_set_user_data(struct wl_output *wl_output, void *user_data) {
		wl_proxy_set_user_data((struct wl_proxy *)wl_output, user_data);
	}

	void *wl_output_get_user_data(struct wl_output *wl_output) {
		return wl_proxy_get_user_data((struct wl_proxy *)wl_output);
	}

	void wl_output_destroy(struct wl_output *wl_output) {
		wl_proxy_destroy((struct wl_proxy *)wl_output);
	}

	int wl_seat_add_listener(struct wl_seat *wl_seat, const struct wl_seat_listener *listener,
			void *data) {
		return wl_proxy_add_listener((struct wl_proxy *)wl_seat, (void (**)(void))listener, data);
	}

	void wl_seat_set_user_data(struct wl_seat *wl_seat, void *user_data) {
		wl_proxy_set_user_data((struct wl_proxy *)wl_seat, user_data);
	}

	void *wl_seat_get_user_data(struct wl_seat *wl_seat) {
		return wl_proxy_get_user_data((struct wl_proxy *)wl_seat);
	}

	struct wl_pointer *wl_seat_get_pointer(struct wl_seat *wl_seat) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy *)wl_seat, WL_SEAT_GET_POINTER,
				wl_pointer_interface, wl_proxy_get_version((struct wl_proxy *)wl_seat), 0, NULL);

		return (struct wl_pointer *)id;
	}

	struct wl_keyboard *wl_seat_get_keyboard(struct wl_seat *wl_seat) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy *)wl_seat, WL_SEAT_GET_KEYBOARD,
				wl_keyboard_interface, wl_proxy_get_version((struct wl_proxy *)wl_seat), 0, NULL);

		return (struct wl_keyboard *)id;
	}

	struct wl_touch *wl_seat_get_touch(struct wl_seat *wl_seat) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy *)wl_seat, WL_SEAT_GET_TOUCH,
				wl_touch_interface, wl_proxy_get_version((struct wl_proxy *)wl_seat), 0, NULL);

		return (struct wl_touch *)id;
	}

	void wl_seat_destroy(struct wl_seat *wl_seat) { wl_proxy_destroy((struct wl_proxy *)wl_seat); }

	int wl_pointer_add_listener(struct wl_pointer *wl_pointer,
			const struct wl_pointer_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy *)wl_pointer, (void (**)(void))listener,
				data);
	}

	void wl_pointer_set_cursor(struct wl_pointer *wl_pointer, uint32_t serial,
			struct wl_surface *surface, int32_t hotspot_x, int32_t hotspot_y) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_pointer, WL_POINTER_SET_CURSOR, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_pointer), 0, serial, surface, hotspot_x,
				hotspot_y);
	}

	void wl_pointer_release(struct wl_pointer *wl_pointer) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_pointer, WL_POINTER_RELEASE, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_pointer), WL_MARSHAL_FLAG_DESTROY);
	}

	int wl_keyboard_add_listener(struct wl_keyboard *wl_keyboard,
			const struct wl_keyboard_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy *)wl_keyboard, (void (**)(void))listener,
				data);
	}

	void wl_keyboard_release(struct wl_keyboard *wl_keyboard) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_keyboard, WL_KEYBOARD_RELEASE, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_keyboard), WL_MARSHAL_FLAG_DESTROY);
	}

	int wl_touch_add_listener(struct wl_touch *wl_touch, const struct wl_touch_listener *listener,
			void *data) {
		return wl_proxy_add_listener((struct wl_proxy *)wl_touch, (void (**)(void))listener, data);
	}

	void wl_touch_release(struct wl_touch *wl_touch) {
		wl_proxy_marshal_flags((struct wl_proxy *)wl_touch, WL_TOUCH_RELEASE, NULL,
				wl_proxy_get_version((struct wl_proxy *)wl_touch), WL_MARSHAL_FLAG_DESTROY);
	}

	struct wp_viewport *wp_viewporter_get_viewport(struct wp_viewporter *wp_viewporter,
			struct wl_surface *surface) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy *)wp_viewporter, WP_VIEWPORTER_GET_VIEWPORT,
				wp_viewport_interface, wl_proxy_get_version((struct wl_proxy *)wp_viewporter), 0,
				NULL, surface);

		return (struct wp_viewport *)id;
	}

	void wp_viewporter_destroy(struct wp_viewporter *wp_viewporter) {
		wl_proxy_marshal_flags((struct wl_proxy *)wp_viewporter, WP_VIEWPORTER_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy *)wp_viewporter), WL_MARSHAL_FLAG_DESTROY);
	}

	void wp_viewport_set_source(struct wp_viewport *wp_viewport, wl_fixed_t x, wl_fixed_t y,
			wl_fixed_t width, wl_fixed_t height) {
		wl_proxy_marshal_flags((struct wl_proxy *)wp_viewport, WP_VIEWPORT_SET_SOURCE, NULL,
				wl_proxy_get_version((struct wl_proxy *)wp_viewport), 0, x, y, width, height);
	}

	void wp_viewport_set_destination(struct wp_viewport *wp_viewport, int32_t width,
			int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy *)wp_viewport, WP_VIEWPORT_SET_DESTINATION, NULL,
				wl_proxy_get_version((struct wl_proxy *)wp_viewport), 0, width, height);
	}

	void wp_viewport_destroy(struct wp_viewport *wp_viewport) {
		wl_proxy_marshal_flags((struct wl_proxy *)wp_viewport, WP_VIEWPORT_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy *)wp_viewport), WL_MARSHAL_FLAG_DESTROY);
	}


	int xdg_wm_base_add_listener(struct xdg_wm_base *xdg_wm_base,
			const struct xdg_wm_base_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy *)xdg_wm_base, (void (**)(void))listener,
				data);
	}

	void xdg_wm_base_set_user_data(struct xdg_wm_base *xdg_wm_base, void *user_data) {
		wl_proxy_set_user_data((struct wl_proxy *)xdg_wm_base, user_data);
	}

	void *xdg_wm_base_get_user_data(struct xdg_wm_base *xdg_wm_base) {
		return wl_proxy_get_user_data((struct wl_proxy *)xdg_wm_base);
	}

	uint32_t xdg_wm_base_get_version(struct xdg_wm_base *xdg_wm_base) {
		return wl_proxy_get_version((struct wl_proxy *)xdg_wm_base);
	}

	void xdg_wm_base_destroy(struct xdg_wm_base *xdg_wm_base) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_wm_base, XDG_WM_BASE_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_wm_base), WL_MARSHAL_FLAG_DESTROY);
	}

	struct xdg_positioner *xdg_wm_base_create_positioner(struct xdg_wm_base *xdg_wm_base) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy *)xdg_wm_base, XDG_WM_BASE_CREATE_POSITIONER,
				xdg_positioner_interface, wl_proxy_get_version((struct wl_proxy *)xdg_wm_base), 0,
				NULL);

		return (struct xdg_positioner *)id;
	}

	struct xdg_surface *xdg_wm_base_get_xdg_surface(struct xdg_wm_base *xdg_wm_base,
			struct wl_surface *surface) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy *)xdg_wm_base, XDG_WM_BASE_GET_XDG_SURFACE,
				xdg_surface_interface, wl_proxy_get_version((struct wl_proxy *)xdg_wm_base), 0,
				NULL, surface);

		return (struct xdg_surface *)id;
	}

	void xdg_wm_base_pong(struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_wm_base, XDG_WM_BASE_PONG, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_wm_base), 0, serial);
	}

	void xdg_positioner_set_user_data(struct xdg_positioner *xdg_positioner, void *user_data) {
		wl_proxy_set_user_data((struct wl_proxy *)xdg_positioner, user_data);
	}

	void *xdg_positioner_get_user_data(struct xdg_positioner *xdg_positioner) {
		return wl_proxy_get_user_data((struct wl_proxy *)xdg_positioner);
	}

	uint32_t xdg_positioner_get_version(struct xdg_positioner *xdg_positioner) {
		return wl_proxy_get_version((struct wl_proxy *)xdg_positioner);
	}

	void xdg_positioner_destroy(struct xdg_positioner *xdg_positioner) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_positioner, XDG_POSITIONER_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_positioner), WL_MARSHAL_FLAG_DESTROY);
	}

	void xdg_positioner_set_size(struct xdg_positioner *xdg_positioner, int32_t width,
			int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_positioner, XDG_POSITIONER_SET_SIZE, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_positioner), 0, width, height);
	}

	void xdg_positioner_set_anchor_rect(struct xdg_positioner *xdg_positioner, int32_t x, int32_t y,
			int32_t width, int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_positioner, XDG_POSITIONER_SET_ANCHOR_RECT,
				NULL, wl_proxy_get_version((struct wl_proxy *)xdg_positioner), 0, x, y, width,
				height);
	}

	void xdg_positioner_set_anchor(struct xdg_positioner *xdg_positioner, uint32_t anchor) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_positioner, XDG_POSITIONER_SET_ANCHOR, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_positioner), 0, anchor);
	}

	void xdg_positioner_set_gravity(struct xdg_positioner *xdg_positioner, uint32_t gravity) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_positioner, XDG_POSITIONER_SET_GRAVITY, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_positioner), 0, gravity);
	}

	void xdg_positioner_set_constraint_adjustment(struct xdg_positioner *xdg_positioner,
			uint32_t constraint_adjustment) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_positioner,
				XDG_POSITIONER_SET_CONSTRAINT_ADJUSTMENT, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_positioner), 0, constraint_adjustment);
	}

	void xdg_positioner_set_offset(struct xdg_positioner *xdg_positioner, int32_t x, int32_t y) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_positioner, XDG_POSITIONER_SET_OFFSET, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_positioner), 0, x, y);
	}

	void xdg_positioner_set_reactive(struct xdg_positioner *xdg_positioner) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_positioner, XDG_POSITIONER_SET_REACTIVE, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_positioner), 0);
	}

	void xdg_positioner_set_parent_size(struct xdg_positioner *xdg_positioner, int32_t parent_width,
			int32_t parent_height) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_positioner, XDG_POSITIONER_SET_PARENT_SIZE,
				NULL, wl_proxy_get_version((struct wl_proxy *)xdg_positioner), 0, parent_width,
				parent_height);
	}

	void xdg_positioner_set_parent_configure(struct xdg_positioner *xdg_positioner,
			uint32_t serial) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_positioner,
				XDG_POSITIONER_SET_PARENT_CONFIGURE, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_positioner), 0, serial);
	}

	int xdg_surface_add_listener(struct xdg_surface *xdg_surface,
			const struct xdg_surface_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy *)xdg_surface, (void (**)(void))listener,
				data);
	}

	void xdg_surface_set_user_data(struct xdg_surface *xdg_surface, void *user_data) {
		wl_proxy_set_user_data((struct wl_proxy *)xdg_surface, user_data);
	}

	void *xdg_surface_get_user_data(struct xdg_surface *xdg_surface) {
		return wl_proxy_get_user_data((struct wl_proxy *)xdg_surface);
	}

	uint32_t xdg_surface_get_version(struct xdg_surface *xdg_surface) {
		return wl_proxy_get_version((struct wl_proxy *)xdg_surface);
	}

	void xdg_surface_destroy(struct xdg_surface *xdg_surface) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_surface, XDG_SURFACE_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_surface), WL_MARSHAL_FLAG_DESTROY);
	}

	struct xdg_toplevel *xdg_surface_get_toplevel(struct xdg_surface *xdg_surface) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy *)xdg_surface, XDG_SURFACE_GET_TOPLEVEL,
				xdg_toplevel_interface, wl_proxy_get_version((struct wl_proxy *)xdg_surface), 0,
				NULL);

		return (struct xdg_toplevel *)id;
	}

	struct xdg_popup *xdg_surface_get_popup(struct xdg_surface *xdg_surface,
			struct xdg_surface *parent, struct xdg_positioner *positioner) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy *)xdg_surface, XDG_SURFACE_GET_POPUP,
				xdg_popup_interface, wl_proxy_get_version((struct wl_proxy *)xdg_surface), 0, NULL,
				parent, positioner);

		return (struct xdg_popup *)id;
	}

	void xdg_surface_set_window_geometry(struct xdg_surface *xdg_surface, int32_t x, int32_t y,
			int32_t width, int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_surface, XDG_SURFACE_SET_WINDOW_GEOMETRY,
				NULL, wl_proxy_get_version((struct wl_proxy *)xdg_surface), 0, x, y, width, height);
	}

	void xdg_surface_ack_configure(struct xdg_surface *xdg_surface, uint32_t serial) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_surface, XDG_SURFACE_ACK_CONFIGURE, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_surface), 0, serial);
	}

	int xdg_toplevel_add_listener(struct xdg_toplevel *xdg_toplevel,
			const struct xdg_toplevel_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy *)xdg_toplevel, (void (**)(void))listener,
				data);
	}

	void xdg_toplevel_set_user_data(struct xdg_toplevel *xdg_toplevel, void *user_data) {
		wl_proxy_set_user_data((struct wl_proxy *)xdg_toplevel, user_data);
	}

	void *xdg_toplevel_get_user_data(struct xdg_toplevel *xdg_toplevel) {
		return wl_proxy_get_user_data((struct wl_proxy *)xdg_toplevel);
	}

	uint32_t xdg_toplevel_get_version(struct xdg_toplevel *xdg_toplevel) {
		return wl_proxy_get_version((struct wl_proxy *)xdg_toplevel);
	}

	void xdg_toplevel_destroy(struct xdg_toplevel *xdg_toplevel) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), WL_MARSHAL_FLAG_DESTROY);
	}

	void xdg_toplevel_set_parent(struct xdg_toplevel *xdg_toplevel, struct xdg_toplevel *parent) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_SET_PARENT, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0, parent);
	}

	void xdg_toplevel_set_title(struct xdg_toplevel *xdg_toplevel, const char *title) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_SET_TITLE, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0, title);
	}

	void xdg_toplevel_set_app_id(struct xdg_toplevel *xdg_toplevel, const char *app_id) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_SET_APP_ID, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0, app_id);
	}

	void xdg_toplevel_show_window_menu(struct xdg_toplevel *xdg_toplevel, struct wl_seat *seat,
			uint32_t serial, int32_t x, int32_t y) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_SHOW_WINDOW_MENU, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0, seat, serial, x, y);
	}

	void xdg_toplevel_move(struct xdg_toplevel *xdg_toplevel, struct wl_seat *seat,
			uint32_t serial) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_MOVE, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0, seat, serial);
	}

	void xdg_toplevel_resize(struct xdg_toplevel *xdg_toplevel, struct wl_seat *seat,
			uint32_t serial, uint32_t edges) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_RESIZE, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0, seat, serial, edges);
	}

	void xdg_toplevel_set_max_size(struct xdg_toplevel *xdg_toplevel, int32_t width,
			int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_SET_MAX_SIZE, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0, width, height);
	}

	void xdg_toplevel_set_min_size(struct xdg_toplevel *xdg_toplevel, int32_t width,
			int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_SET_MIN_SIZE, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0, width, height);
	}

	void xdg_toplevel_set_maximized(struct xdg_toplevel *xdg_toplevel) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_SET_MAXIMIZED, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0);
	}

	void xdg_toplevel_unset_maximized(struct xdg_toplevel *xdg_toplevel) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_UNSET_MAXIMIZED, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0);
	}

	void xdg_toplevel_set_fullscreen(struct xdg_toplevel *xdg_toplevel, struct wl_output *output) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_SET_FULLSCREEN, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0, output);
	}

	void xdg_toplevel_unset_fullscreen(struct xdg_toplevel *xdg_toplevel) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_UNSET_FULLSCREEN, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0);
	}

	void xdg_toplevel_set_minimized(struct xdg_toplevel *xdg_toplevel) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_toplevel, XDG_TOPLEVEL_SET_MINIMIZED, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_toplevel), 0);
	}

	int xdg_popup_add_listener(struct xdg_popup *xdg_popup,
			const struct xdg_popup_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy *)xdg_popup, (void (**)(void))listener, data);
	}

	void xdg_popup_set_user_data(struct xdg_popup *xdg_popup, void *user_data) {
		wl_proxy_set_user_data((struct wl_proxy *)xdg_popup, user_data);
	}

	void *xdg_popup_get_user_data(struct xdg_popup *xdg_popup) {
		return wl_proxy_get_user_data((struct wl_proxy *)xdg_popup);
	}

	uint32_t xdg_popup_get_version(struct xdg_popup *xdg_popup) {
		return wl_proxy_get_version((struct wl_proxy *)xdg_popup);
	}

	void xdg_popup_destroy(struct xdg_popup *xdg_popup) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_popup, XDG_POPUP_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_popup), WL_MARSHAL_FLAG_DESTROY);
	}

	void xdg_popup_grab(struct xdg_popup *xdg_popup, struct wl_seat *seat, uint32_t serial) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_popup, XDG_POPUP_GRAB, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_popup), 0, seat, serial);
	}

	void xdg_popup_reposition(struct xdg_popup *xdg_popup, struct xdg_positioner *positioner,
			uint32_t token) {
		wl_proxy_marshal_flags((struct wl_proxy *)xdg_popup, XDG_POPUP_REPOSITION, NULL,
				wl_proxy_get_version((struct wl_proxy *)xdg_popup), 0, positioner, token);
	}

	decltype(&_xl_null_fn) _wlcursor_first_fn = &_xl_null_fn;

	XL_DEFINE_PROTO(wl_cursor_theme_load)
	XL_DEFINE_PROTO(wl_cursor_theme_destroy)
	XL_DEFINE_PROTO(wl_cursor_theme_get_cursor)
	XL_DEFINE_PROTO(wl_cursor_image_get_buffer)

	decltype(&_xl_null_fn) _wlcursor_last_fn = &_xl_null_fn;


	decltype(&_xl_null_fn) _libdecor_first_fn = &_xl_null_fn;

	XL_DEFINE_PROTO(libdecor_unref)
	XL_DEFINE_PROTO(libdecor_new)
	XL_DEFINE_PROTO(libdecor_get_fd)
	XL_DEFINE_PROTO(libdecor_dispatch)
	XL_DEFINE_PROTO(libdecor_decorate)
	XL_DEFINE_PROTO(libdecor_frame_ref)
	XL_DEFINE_PROTO(libdecor_frame_unref)
	XL_DEFINE_PROTO(libdecor_frame_set_visibility)
	XL_DEFINE_PROTO(libdecor_frame_is_visible)
	XL_DEFINE_PROTO(libdecor_frame_set_parent)
	XL_DEFINE_PROTO(libdecor_frame_set_title)
	XL_DEFINE_PROTO(libdecor_frame_get_title)
	XL_DEFINE_PROTO(libdecor_frame_set_app_id)
	XL_DEFINE_PROTO(libdecor_frame_set_capabilities)
	XL_DEFINE_PROTO(libdecor_frame_unset_capabilities)
	XL_DEFINE_PROTO(libdecor_frame_has_capability)
	XL_DEFINE_PROTO(libdecor_frame_show_window_menu)
	XL_DEFINE_PROTO(libdecor_frame_popup_grab)
	XL_DEFINE_PROTO(libdecor_frame_popup_ungrab)
	XL_DEFINE_PROTO(libdecor_frame_translate_coordinate)
	XL_DEFINE_PROTO(libdecor_frame_set_min_content_size)
	XL_DEFINE_PROTO(libdecor_frame_set_max_content_size)
	XL_DEFINE_PROTO(libdecor_frame_get_min_content_size)
	XL_DEFINE_PROTO(libdecor_frame_get_max_content_size)
	XL_DEFINE_PROTO(libdecor_frame_resize)
	XL_DEFINE_PROTO(libdecor_frame_move)
	XL_DEFINE_PROTO(libdecor_frame_commit)
	XL_DEFINE_PROTO(libdecor_frame_set_minimized)
	XL_DEFINE_PROTO(libdecor_frame_set_maximized)
	XL_DEFINE_PROTO(libdecor_frame_unset_maximized)
	XL_DEFINE_PROTO(libdecor_frame_set_fullscreen)
	XL_DEFINE_PROTO(libdecor_frame_unset_fullscreen)
	XL_DEFINE_PROTO(libdecor_frame_is_floating)
	XL_DEFINE_PROTO(libdecor_frame_close)
	XL_DEFINE_PROTO(libdecor_frame_map)
	XL_DEFINE_PROTO(libdecor_frame_get_xdg_surface)
	XL_DEFINE_PROTO(libdecor_frame_get_xdg_toplevel)
	XL_DEFINE_PROTO(libdecor_state_new)
	XL_DEFINE_PROTO(libdecor_state_free)
	XL_DEFINE_PROTO(libdecor_configuration_get_content_size)
	XL_DEFINE_PROTO(libdecor_configuration_get_window_state)

	decltype(&_xl_null_fn) _libdecor_last_fn = &_xl_null_fn;

	bool hasWaylandCursor() const { return _cursor ? true : false; }
	bool hasDecor() const { return _decor ? true : false; }

protected:
	bool open(Dso &);
	bool openWaylandCursor(Dso &);
	bool openDecor(Dso &);

	Dso _handle;
	Dso _cursor;
	Dso _decor;
};

/*enum class WaylandCursorImage {
	LeftPtr,
	EResize,
	NEResize,
	NResize,
	NWResize,
	SEResize,
	SResize,
	SWResize,
	WResize,

	RightSide = EResize,
	TopRigntCorner = NEResize,
	TopSide = NResize,
	TopLeftCorner = NWResize,
	BottomRightCorner = SEResize,
	BottomSide = SResize,
	BottomLeftCorner = SWResize,
	LeftSide = WResize,

	Max
};*/

} // namespace stappler::xenolith::platform

#endif

#endif // XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDLIBRARY_H_
