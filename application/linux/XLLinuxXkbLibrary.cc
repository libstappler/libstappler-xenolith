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

#include "XLLinuxXkbLibrary.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

XkbLibrary::~XkbLibrary() { close(); }

bool XkbLibrary::init() {
	_handle = Dso("libxkbcommon.so");
	if (!_handle) {
		log::error("XkbLibrary", "Fail to open libxkbcommon.so");
		return false;
	}

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
	XL_LOAD_PROTO(handle, xkb_context_new)
	XL_LOAD_PROTO(handle, xkb_context_ref)
	XL_LOAD_PROTO(handle, xkb_context_unref)
	XL_LOAD_PROTO(handle, xkb_keymap_unref)
	XL_LOAD_PROTO(handle, xkb_state_unref)
	XL_LOAD_PROTO(handle, xkb_keymap_new_from_string)
	XL_LOAD_PROTO(handle, xkb_state_new)
	XL_LOAD_PROTO(handle, xkb_state_update_mask)
	XL_LOAD_PROTO(handle, xkb_state_key_get_utf8)
	XL_LOAD_PROTO(handle, xkb_state_key_get_utf32)
	XL_LOAD_PROTO(handle, xkb_state_key_get_one_sym)
	XL_LOAD_PROTO(handle, xkb_state_mod_index_is_active)
	XL_LOAD_PROTO(handle, xkb_state_key_get_syms)
	XL_LOAD_PROTO(handle, xkb_state_get_keymap)
	XL_LOAD_PROTO(handle, xkb_keymap_key_for_each)
	XL_LOAD_PROTO(handle, xkb_keymap_key_get_name)
	XL_LOAD_PROTO(handle, xkb_keymap_mod_get_index)
	XL_LOAD_PROTO(handle, xkb_keymap_key_repeats)
	XL_LOAD_PROTO(handle, xkb_keysym_to_utf32)
	XL_LOAD_PROTO(handle, xkb_compose_table_new_from_locale)
	XL_LOAD_PROTO(handle, xkb_compose_table_unref)
	XL_LOAD_PROTO(handle, xkb_compose_state_new)
	XL_LOAD_PROTO(handle, xkb_compose_state_feed)
	XL_LOAD_PROTO(handle, xkb_compose_state_reset)
	XL_LOAD_PROTO(handle, xkb_compose_state_get_status)
	XL_LOAD_PROTO(handle, xkb_compose_state_get_one_sym)
	XL_LOAD_PROTO(handle, xkb_compose_state_unref)

	if (!validateFunctionList(&_xkb_first_fn, &_xkb_last_fn)) {
		log::error("XkbLibrary", "Fail to load libxkb");
		return false;
	}

	openAux();
	return true;
}

void XkbLibrary::openAux() {
	if (auto handle = Dso("libxkbcommon-x11.so")) {
		XL_LOAD_PROTO(handle, xkb_x11_setup_xkb_extension);
		XL_LOAD_PROTO(handle, xkb_x11_get_core_keyboard_device_id)
		XL_LOAD_PROTO(handle, xkb_x11_keymap_new_from_device)
		XL_LOAD_PROTO(handle, xkb_x11_state_new_from_device)

		if (!validateFunctionList(&_xkb_x11_first_fn, &_xkb_x11_last_fn)) {
			log::error("XcbLibrary", "Fail to load libxcb-randr function");
		} else {
			_x11 = move(handle);
		}
	}
}

} // namespace stappler::xenolith::platform
