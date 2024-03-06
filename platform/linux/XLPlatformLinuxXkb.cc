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

#include "XLPlatformLinuxXkb.h"

#if XL_LINK
extern "C" {
struct xkb_context * xkb_context_new(enum xkb_context_flags flags);
struct xkb_context * xkb_context_ref(struct xkb_context *context);
void xkb_context_unref (struct xkb_context *context);
void xkb_keymap_unref(struct xkb_keymap *keymap);
void xkb_state_unref(struct xkb_state *state);
struct xkb_keymap* xkb_keymap_new_from_string(struct xkb_context *context, const char *string,
		enum xkb_keymap_format format, enum xkb_keymap_compile_flags flags);
struct xkb_state * xkb_state_new(struct xkb_keymap *keymap);
enum xkb_state_component xkb_state_update_mask(struct xkb_state *, xkb_mod_mask_t depressed_mods,
		xkb_mod_mask_t latched_mods, xkb_mod_mask_t locked_mods, xkb_layout_index_t depressed_layout,
		xkb_layout_index_t latched_layout, xkb_layout_index_t locked_layout);
int xkb_state_key_get_utf8(struct xkb_state *state, xkb_keycode_t key, char *buffer, size_t size);
uint32_t xkb_state_key_get_utf32(struct xkb_state *state, xkb_keycode_t key);
xkb_keysym_t xkb_state_key_get_one_sym(struct xkb_state *state, xkb_keycode_t key);
int xkb_state_mod_index_is_active(struct xkb_state *state, xkb_mod_index_t idx, enum xkb_state_component type);
int xkb_state_key_get_syms(struct xkb_state *state, xkb_keycode_t key, const xkb_keysym_t **syms_out);
struct xkb_keymap * xkb_state_get_keymap(struct xkb_state *state);
void xkb_keymap_key_for_each(struct xkb_keymap *keymap, xkb_keymap_key_iter_t iter, void *data);
const char * xkb_keymap_key_get_name(struct xkb_keymap *keymap, xkb_keycode_t key);
xkb_mod_index_t xkb_keymap_mod_get_index(struct xkb_keymap *keymap, const char *name);
int xkb_keymap_key_repeats(struct xkb_keymap *keymap, xkb_keycode_t key);
uint32_t xkb_keysym_to_utf32(xkb_keysym_t keysym);

struct xkb_compose_table * xkb_compose_table_new_from_locale(struct xkb_context *context,
		const char *locale, enum xkb_compose_compile_flags flags);
void xkb_compose_table_unref(struct xkb_compose_table *table);
struct xkb_compose_state* xkb_compose_state_new(struct xkb_compose_table *table, enum xkb_compose_state_flags flags);
enum xkb_compose_feed_result xkb_compose_state_feed(struct xkb_compose_state *state, xkb_keysym_t keysym);
void xkb_compose_state_reset(struct xkb_compose_state *state);
enum xkb_compose_status xkb_compose_state_get_status(struct xkb_compose_state *state);
xkb_keysym_t xkb_compose_state_get_one_sym(struct xkb_compose_state *state);
void xkb_compose_state_unref(struct xkb_compose_state *state);

int xkb_x11_setup_xkb_extension(xcb_connection_t *connection, uint16_t major_xkb_version, uint16_t minor_xkb_version,
		enum xkb_x11_setup_xkb_extension_flags flags, uint16_t *major_xkb_version_out, uint16_t *minor_xkb_version_out,
		uint8_t *base_event_out, uint8_t *base_error_out);
int32_t xkb_x11_get_core_keyboard_device_id(xcb_connection_t *connection);
struct xkb_keymap * xkb_x11_keymap_new_from_device(struct xkb_context *context,
		xcb_connection_t *connection, int32_t device_id, enum xkb_keymap_compile_flags flags);
struct xkb_state * xkb_x11_state_new_from_device(struct xkb_keymap *keymap,
		xcb_connection_t *connection, int32_t device_id);
}
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

XkbLibrary *XkbLibrary::getInstance() {
	static Mutex s_mutex;
	static Rc<XkbLibrary> s_lib;

	std::unique_lock<Mutex> lock(s_mutex);
	if (!s_lib) {
		s_lib = Rc<XkbLibrary>::create();
	}

	return s_lib;
}

XkbLibrary::~XkbLibrary() {
	close();
}

bool XkbLibrary::init() {
#ifndef XL_LINK
	_handle = Dso("libxkbcommon.so");
	if (!_handle) {
		log::error("XkbLibrary", "Fail to open libxkbcommon.so");
		return false;
	}
#endif

	if (open(_handle)) {
		_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
		return true;
	} else {
		_handle = Dso();
	}
	return false;
}

void XkbLibrary::close() {
	if (_context) {
		xkb_context_unref(_context);
		_context = nullptr;
	}
}

bool XkbLibrary::open(Dso &handle) {
#if XL_LINK
	this->xkb_context_new = &::xkb_context_new;
	this->xkb_context_ref = &::xkb_context_ref;
	this->xkb_context_unref = &::xkb_context_unref;
	this->xkb_keymap_unref = &::xkb_keymap_unref;
	this->xkb_state_unref = &::xkb_state_unref;
	this->xkb_keymap_new_from_string = &::xkb_keymap_new_from_string;
	this->xkb_state_new = &::xkb_state_new;
	this->xkb_state_update_mask = &::xkb_state_update_mask;
	this->xkb_state_key_get_utf8 = &::xkb_state_key_get_utf8;
	this->xkb_state_key_get_utf32 = &::xkb_state_key_get_utf32;
	this->xkb_state_key_get_one_sym = &::xkb_state_key_get_one_sym;
	this->xkb_state_mod_index_is_active = &::xkb_state_mod_index_is_active;
	this->xkb_state_key_get_syms = &::xkb_state_key_get_syms;
	this->xkb_state_get_keymap = &::xkb_state_get_keymap;
	this->xkb_keymap_key_for_each = &::xkb_keymap_key_for_each;
	this->xkb_keymap_key_get_name = &::xkb_keymap_key_get_name;
	this->xkb_keymap_mod_get_index = &::xkb_keymap_mod_get_index;
	this->xkb_keymap_key_repeats = &::xkb_keymap_key_repeats;
	this->xkb_keysym_to_utf32 = &::xkb_keysym_to_utf32;
	this->xkb_compose_table_new_from_locale = &::xkb_compose_table_new_from_locale;
	this->xkb_compose_table_unref = &::xkb_compose_table_unref;
	this->xkb_compose_state_new = &::xkb_compose_state_new;
	this->xkb_compose_state_feed = &::xkb_compose_state_feed;
	this->xkb_compose_state_reset = &::xkb_compose_state_reset;
	this->xkb_compose_state_get_status = &::xkb_compose_state_get_status;
	this->xkb_compose_state_get_one_sym = &::xkb_compose_state_get_one_sym;
	this->xkb_compose_state_unref = &::xkb_compose_state_unref;
#else
	xkb_context_new = handle.sym<decltype(xkb_context_new)>("xkb_context_new");
	xkb_context_ref = handle.sym<decltype(xkb_context_ref)>("xkb_context_ref");
	xkb_context_unref = handle.sym<decltype(xkb_context_unref)>("xkb_context_unref");
	xkb_keymap_unref = handle.sym<decltype(xkb_keymap_unref)>("xkb_keymap_unref");
	xkb_state_unref = handle.sym<decltype(xkb_state_unref)>("xkb_state_unref");
	xkb_keymap_new_from_string = handle.sym<decltype(xkb_keymap_new_from_string)>("xkb_keymap_new_from_string");
	xkb_state_new = handle.sym<decltype(xkb_state_new)>("xkb_state_new");
	xkb_state_update_mask = handle.sym<decltype(xkb_state_update_mask)>("xkb_state_update_mask");
	xkb_state_key_get_utf8 = handle.sym<decltype(xkb_state_key_get_utf8)>("xkb_state_key_get_utf8");
	xkb_state_key_get_utf32 = handle.sym<decltype(xkb_state_key_get_utf32)>("xkb_state_key_get_utf32");
	xkb_state_key_get_one_sym = handle.sym<decltype(xkb_state_key_get_one_sym)>("xkb_state_key_get_one_sym");
	xkb_state_mod_index_is_active = handle.sym<decltype(xkb_state_mod_index_is_active)>("xkb_state_mod_index_is_active");
	xkb_state_key_get_syms = handle.sym<decltype(xkb_state_key_get_syms)>("xkb_state_key_get_syms");
	xkb_state_get_keymap = handle.sym<decltype(xkb_state_get_keymap)>("xkb_state_get_keymap");
	xkb_keymap_key_for_each = handle.sym<decltype(xkb_keymap_key_for_each)>("xkb_keymap_key_for_each");
	xkb_keymap_key_get_name = handle.sym<decltype(xkb_keymap_key_get_name)>("xkb_keymap_key_get_name");
	xkb_keymap_mod_get_index = handle.sym<decltype(xkb_keymap_mod_get_index)>("xkb_keymap_mod_get_index");
	xkb_keymap_key_repeats = handle.sym<decltype(xkb_keymap_key_repeats)>("xkb_keymap_key_repeats");
	xkb_keysym_to_utf32 = handle.sym<decltype(xkb_keysym_to_utf32)>("xkb_keysym_to_utf32");
	xkb_compose_table_new_from_locale = handle.sym<decltype(xkb_compose_table_new_from_locale)>("xkb_compose_table_new_from_locale");
	xkb_compose_table_unref = handle.sym<decltype(xkb_compose_table_unref)>("xkb_compose_table_unref");
	xkb_compose_state_new = handle.sym<decltype(xkb_compose_state_new)>("xkb_compose_state_new");
	xkb_compose_state_feed = handle.sym<decltype(xkb_compose_state_feed)>("xkb_compose_state_feed");
	xkb_compose_state_reset = handle.sym<decltype(xkb_compose_state_reset)>("xkb_compose_state_reset");
	xkb_compose_state_get_status = handle.sym<decltype(xkb_compose_state_get_status)>("xkb_compose_state_get_status");
	xkb_compose_state_get_one_sym = handle.sym<decltype(xkb_compose_state_get_one_sym)>("xkb_compose_state_get_one_sym");
	xkb_compose_state_unref = handle.sym<decltype(xkb_compose_state_unref)>("xkb_compose_state_unref");
#endif

	if (this->xkb_context_new
			&& this->xkb_context_ref
			&& this->xkb_context_unref
			&& this->xkb_keymap_unref
			&& this->xkb_state_unref
			&& this->xkb_keymap_new_from_string
			&& this->xkb_state_new
			&& this->xkb_state_update_mask
			&& this->xkb_state_key_get_utf8
			&& this->xkb_state_key_get_utf32
			&& this->xkb_state_key_get_one_sym
			&& this->xkb_state_mod_index_is_active
			&& this->xkb_state_key_get_syms
			&& this->xkb_state_get_keymap
			&& this->xkb_keymap_key_for_each
			&& this->xkb_keymap_key_get_name
			&& this->xkb_keymap_mod_get_index
			&& this->xkb_keymap_key_repeats
			&& this->xkb_keysym_to_utf32
			&& this->xkb_compose_table_new_from_locale
			&& this->xkb_compose_table_unref
			&& this->xkb_compose_state_new
			&& this->xkb_compose_state_feed
			&& this->xkb_compose_state_reset
			&& this->xkb_compose_state_get_status
			&& this->xkb_compose_state_get_one_sym
			&& this->xkb_compose_state_unref) {
		openAux();
		return true;
	}
	return false;
}

void XkbLibrary::openAux() {
#if XL_LINK
	this->xkb_x11_setup_xkb_extension = &::xkb_x11_setup_xkb_extension;
	this->xkb_x11_get_core_keyboard_device_id = &::xkb_x11_get_core_keyboard_device_id;
	this->xkb_x11_keymap_new_from_device = &::xkb_x11_keymap_new_from_device;
	this->xkb_x11_state_new_from_device = &::xkb_x11_state_new_from_device;
#else
	if (auto handle = Dso("libxkbcommon-x11.so")) {
		this->xkb_x11_setup_xkb_extension =
				handle.sym<decltype(this->xkb_x11_setup_xkb_extension)>("xkb_x11_setup_xkb_extension");
		this->xkb_x11_get_core_keyboard_device_id =
				handle.sym<decltype(this->xkb_x11_get_core_keyboard_device_id)>("xkb_x11_get_core_keyboard_device_id");
		this->xkb_x11_keymap_new_from_device =
				handle.sym<decltype(this->xkb_x11_keymap_new_from_device)>("xkb_x11_keymap_new_from_device");
		this->xkb_x11_state_new_from_device =
				handle.sym<decltype(this->xkb_x11_state_new_from_device)>("xkb_x11_state_new_from_device");

		if (this->xkb_x11_setup_xkb_extension
				&& xkb_x11_get_core_keyboard_device_id
				&& xkb_x11_keymap_new_from_device
				&& xkb_x11_state_new_from_device) {
			_x11 = move(handle);
		} else {
			this->xkb_x11_setup_xkb_extension = nullptr;
			this->xkb_x11_get_core_keyboard_device_id = nullptr;
			this->xkb_x11_keymap_new_from_device = nullptr;
			this->xkb_x11_state_new_from_device = nullptr;
		}
	}
#endif
}

}
