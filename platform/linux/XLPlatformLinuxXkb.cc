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
	if (auto d = Dso("libxkbcommon.so")) {
		if (open(d)) {
			_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
			return true;
		}
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
		_handle = move(handle);
		openAux();
		return true;
	}
	return false;
}

void XkbLibrary::openAux() {
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
}

}
