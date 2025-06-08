/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

#include "XLPlatformLinuxXcb.h"
#include "XLPlatformLinuxXcbConnection.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static XcbLibrary *s_XcbLibrary = nullptr;

// redirect xcb_wait_for_reply to libxcb
SP_EXTERN_C void *xcb_wait_for_reply(xcb_connection_t *c, unsigned int request,
		xcb_generic_error_t **e) {
	return s_XcbLibrary->xcb_wait_for_reply(c, request, e);
}

void _xl_xcb_null_fn() { }

XcbLibrary *XcbLibrary::getInstance() { return s_XcbLibrary; }

XcbLibrary::~XcbLibrary() { close(); }

bool XcbLibrary::init() {
	_handle = Dso("libxcb.so");
	if (!_handle) {
		return false;
	}

	if (open(_handle)) {
		s_XcbLibrary = this;
		_connection = Rc<XcbConnection>::alloc(this);
		return _connection != nullptr;
	} else {
		_handle = Dso();
	}
	return false;
}

template <typename T>
static bool XcbLibrary_clearFunctionList(T first, T last) {
	while (first != last) {
		*first = nullptr;
		++first;
	}
	return true;
}

template <typename T>
static bool XcbLibrary_validateFunctionList(T first, T last) {
	while (first != last) {
		if (*first == nullptr) {
			XcbLibrary_clearFunctionList(first, last);
			return false;
		}
		++first;
	}
	return true;
}

bool XcbLibrary::open(Dso &handle) {
	XL_XCB_LOAD_PROTO(handle, xcb_connect)
	XL_XCB_LOAD_PROTO(handle, xcb_get_setup)
	XL_XCB_LOAD_PROTO(handle, xcb_setup_roots_iterator)
	XL_XCB_LOAD_PROTO(handle, xcb_screen_next)
	XL_XCB_LOAD_PROTO(handle, xcb_connection_has_error)
	XL_XCB_LOAD_PROTO(handle, xcb_get_file_descriptor)
	XL_XCB_LOAD_PROTO(handle, xcb_generate_id)
	XL_XCB_LOAD_PROTO(handle, xcb_flush)
	XL_XCB_LOAD_PROTO(handle, xcb_disconnect)
	XL_XCB_LOAD_PROTO(handle, xcb_poll_for_event)
	XL_XCB_LOAD_PROTO(handle, xcb_send_event)
	XL_XCB_LOAD_PROTO(handle, xcb_get_extension_data)
	XL_XCB_LOAD_PROTO(handle, xcb_map_window)
	XL_XCB_LOAD_PROTO(handle, xcb_create_window)
	XL_XCB_LOAD_PROTO(handle, xcb_configure_window)
	XL_XCB_LOAD_PROTO(handle, xcb_change_window_attributes)
	XL_XCB_LOAD_PROTO(handle, xcb_change_property)
	XL_XCB_LOAD_PROTO(handle, xcb_intern_atom)
	XL_XCB_LOAD_PROTO(handle, xcb_intern_atom_reply)
	XL_XCB_LOAD_PROTO(handle, xcb_get_property_reply)
	XL_XCB_LOAD_PROTO(handle, xcb_get_property)
	XL_XCB_LOAD_PROTO(handle, xcb_get_property_value)
	XL_XCB_LOAD_PROTO(handle, xcb_get_property_value_length)
	XL_XCB_LOAD_PROTO(handle, xcb_request_check)
	XL_XCB_LOAD_PROTO(handle, xcb_open_font_checked)
	XL_XCB_LOAD_PROTO(handle, xcb_create_glyph_cursor)
	XL_XCB_LOAD_PROTO(handle, xcb_wait_for_reply)
	XL_XCB_LOAD_PROTO(handle, xcb_create_gc_checked)
	XL_XCB_LOAD_PROTO(handle, xcb_free_cursor)
	XL_XCB_LOAD_PROTO(handle, xcb_close_font_checked)
	XL_XCB_LOAD_PROTO(handle, xcb_get_modifier_mapping_unchecked)
	XL_XCB_LOAD_PROTO(handle, xcb_get_modifier_mapping_reply)
	XL_XCB_LOAD_PROTO(handle, xcb_get_modifier_mapping_keycodes)
	XL_XCB_LOAD_PROTO(handle, xcb_convert_selection)
	XL_XCB_LOAD_PROTO(handle, xcb_set_selection_owner)
	XL_XCB_LOAD_PROTO(handle, xcb_get_selection_owner);
	XL_XCB_LOAD_PROTO(handle, xcb_get_selection_owner_reply)
	XL_XCB_LOAD_PROTO(handle, xcb_get_keyboard_mapping)
	XL_XCB_LOAD_PROTO(handle, xcb_get_keyboard_mapping_reply)

	if (!XcbLibrary_validateFunctionList(&_xcb_first_fn, &_xcb_last_fn)) {
		log::error("XcbLibrary", "Fail to load xcb function");
		return false;
	}

	openAux();
	return true;
}

void XcbLibrary::close() {
	if (s_XcbLibrary == this) {
		s_XcbLibrary = nullptr;
	}
}

bool XcbLibrary::hasRandr() const { return _randr ? true : false; }

bool XcbLibrary::hasKeysyms() const { return _keysyms ? true : false; }

bool XcbLibrary::hasXkb() const { return _xkb ? true : false; }

bool XcbLibrary::hasSync() const { return _sync ? true : false; }

XcbConnection *XcbLibrary::getCommonConnection() {
	std::unique_lock lock(_connectionMutex);
	if (!_connection) {
		_connection = Rc<XcbConnection>::alloc(this);
	}
	return _connection;
}

Rc<XcbConnection> XcbLibrary::acquireConnection() {
	std::unique_lock lock(_connectionMutex);
	Rc<XcbConnection> ret;
	if (_connection) {
		ret = move(_connection);
		_connection = nullptr;
	} else {
		ret = Rc<XcbConnection>::alloc(this);
	}
	return ret;
}

void XcbLibrary::openAux() {
	if (auto randr = Dso("libxcb-randr.so")) {
		XL_XCB_LOAD_PROTO(randr, xcb_randr_id)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_query_version)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_query_version_reply)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_info_unchecked)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_info_reply)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_info_sizes)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_info_sizes_length)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_info_sizes_iterator)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_info_rates_length)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_info_rates_iterator)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_refresh_rates_next)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_refresh_rates_end)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_refresh_rates_rates)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_refresh_rates_rates_length)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources_unchecked)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources_reply)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources_modes)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources_modes_length)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_unchecked)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_reply)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_outputs)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_outputs_length)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_modes)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_modes_length)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_names)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_names_length)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_crtcs)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_crtcs_length)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_output_primary)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_output_primary_unchecked)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_output_primary_reply)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_output_info)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_output_info_unchecked)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_output_info_reply)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_output_info_crtcs)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_output_info_crtcs_length)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_output_info_crtcs_end)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_output_info_modes)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_output_info_modes_length)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_output_info_name)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_output_info_name_length)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_crtc_info)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_crtc_info_unchecked)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_crtc_info_reply)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_crtc_info_outputs)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_crtc_info_outputs_length)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_crtc_info_possible)
		XL_XCB_LOAD_PROTO(randr, xcb_randr_get_crtc_info_possible_length)

		if (!XcbLibrary_validateFunctionList(&_xcb_randr_first_fn, &_xcb_randr_last_fn)) {
			log::error("XcbLibrary", "Fail to load libxcb-randr function");
		} else {
			_randr = move(randr);
		}
	}

	if (auto keysyms = Dso("libxcb-keysyms.so")) {
		XL_XCB_LOAD_PROTO(keysyms, xcb_key_symbols_alloc)
		XL_XCB_LOAD_PROTO(keysyms, xcb_key_symbols_free)
		XL_XCB_LOAD_PROTO(keysyms, xcb_key_symbols_get_keysym)
		XL_XCB_LOAD_PROTO(keysyms, xcb_key_symbols_get_keycode)
		XL_XCB_LOAD_PROTO(keysyms, xcb_key_press_lookup_keysym)
		XL_XCB_LOAD_PROTO(keysyms, xcb_key_release_lookup_keysym)
		XL_XCB_LOAD_PROTO(keysyms, xcb_refresh_keyboard_mapping)
		XL_XCB_LOAD_PROTO(keysyms, xcb_is_keypad_key)
		XL_XCB_LOAD_PROTO(keysyms, xcb_is_private_keypad_key)
		XL_XCB_LOAD_PROTO(keysyms, xcb_is_cursor_key)
		XL_XCB_LOAD_PROTO(keysyms, xcb_is_pf_key)
		XL_XCB_LOAD_PROTO(keysyms, xcb_is_function_key)
		XL_XCB_LOAD_PROTO(keysyms, xcb_is_misc_function_key)
		XL_XCB_LOAD_PROTO(keysyms, xcb_is_modifier_key)

		if (!XcbLibrary_validateFunctionList(&_xcb_key_first_fn, &_xcb_key_last_fn)) {
			log::error("XcbLibrary", "Fail to load libxcb-randr function");
		} else {
			_keysyms = move(keysyms);
		}
	}

	if (auto xkb = Dso("libxcb-xkb.so")) {
		XL_XCB_LOAD_PROTO(xkb, xcb_xkb_select_events)

		if (!XcbLibrary_validateFunctionList(&_xcb_xkb_first_fn, &_xcb_xkb_last_fn)) {
			log::error("XcbLibrary", "Fail to load libxcb-xkb function");
		} else {
			_xkb = move(xkb);
		}
	}

	if (auto sync = Dso("libxcb-sync.so")) {
		XL_XCB_LOAD_PROTO(sync, xcb_sync_id)
		XL_XCB_LOAD_PROTO(sync, xcb_sync_create_counter)
		XL_XCB_LOAD_PROTO(sync, xcb_sync_create_counter_checked)
		XL_XCB_LOAD_PROTO(sync, xcb_sync_destroy_counter)
		XL_XCB_LOAD_PROTO(sync, xcb_sync_destroy_counter_checked)
		XL_XCB_LOAD_PROTO(sync, xcb_sync_set_counter)

		if (!XcbLibrary_validateFunctionList(&_xcb_sync_first_fn, &_xcb_sync_last_fn)) {
			log::error("XcbLibrary", "Fail to load libxcb-sync function");
		} else {
			_sync = move(sync);
		}
	}

	if (auto cursor = Dso("libxcb-cursor.so")) {
		XL_XCB_LOAD_PROTO(cursor, xcb_cursor_context_new)
		XL_XCB_LOAD_PROTO(cursor, xcb_cursor_load_cursor)
		XL_XCB_LOAD_PROTO(cursor, xcb_cursor_context_free)

		if (!XcbLibrary_validateFunctionList(&_xcb_cursor_first_fn, &_xcb_cursor_last_fn)) {
			log::error("XcbLibrary", "Fail to load libxcb-cursor function");
		} else {
			_cursor = move(cursor);
		}
	}
}

} // namespace stappler::xenolith::platform
