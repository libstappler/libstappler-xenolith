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

#include "XLPlatformLinuxXcb.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static XcbLibrary *s_XcbLibrary = nullptr;

SP_EXTERN_C void * xcb_wait_for_reply(xcb_connection_t *c, unsigned int request, xcb_generic_error_t **e) {
	return s_XcbLibrary->xcb_wait_for_reply(c, request, e);
}

XcbLibrary *XcbLibrary::getInstance() {
	return s_XcbLibrary;
}

XcbLibrary::~XcbLibrary() {
	close();
}

bool XcbLibrary::init() {
	if (auto d = Dso("libxcb.so")) {
		if (open(d)) {
			s_XcbLibrary = this;
			openConnection(_pending);
			return _pending.connection != nullptr;
		}
	}
	return false;
}

bool XcbLibrary::open(Dso &handle) {
	this->xcb_connect = handle.sym<decltype(this->xcb_connect)>("xcb_connect");
	this->xcb_get_setup = handle.sym<decltype(this->xcb_get_setup)>("xcb_get_setup");
	this->xcb_setup_roots_iterator =
			handle.sym<decltype(this->xcb_setup_roots_iterator)>("xcb_setup_roots_iterator");
	this->xcb_screen_next =
			handle.sym<decltype(this->xcb_screen_next)>("xcb_screen_next");
	this->xcb_connection_has_error =
			handle.sym<decltype(this->xcb_connection_has_error)>("xcb_connection_has_error");
	this->xcb_get_file_descriptor =
			handle.sym<decltype(this->xcb_get_file_descriptor)>("xcb_get_file_descriptor");
	this->xcb_generate_id = handle.sym<decltype(this->xcb_generate_id)>("xcb_generate_id");
	this->xcb_flush = handle.sym<decltype(this->xcb_flush)>("xcb_flush");
	this->xcb_disconnect = handle.sym<decltype(this->xcb_disconnect)>("xcb_disconnect");
	this->xcb_poll_for_event = handle.sym<decltype(this->xcb_poll_for_event)>("xcb_poll_for_event");
	this->xcb_get_extension_data = handle.sym<decltype(this->xcb_get_extension_data)>("xcb_get_extension_data");
	this->xcb_map_window = handle.sym<decltype(this->xcb_map_window)>("xcb_map_window");
	this->xcb_create_window = handle.sym<decltype(this->xcb_create_window)>("xcb_create_window");
	this->xcb_change_property = handle.sym<decltype(this->xcb_change_property)>("xcb_change_property");
	this->xcb_intern_atom = handle.sym<decltype(this->xcb_intern_atom)>("xcb_intern_atom");
	this->xcb_intern_atom_reply = handle.sym<decltype(this->xcb_intern_atom_reply)>("xcb_intern_atom_reply");
	this->xcb_wait_for_reply = handle.sym<decltype(this->xcb_wait_for_reply)>("xcb_wait_for_reply");
	this->xcb_get_modifier_mapping_unchecked =
			handle.sym<decltype(this->xcb_get_modifier_mapping_unchecked)>("xcb_get_modifier_mapping_unchecked");
	this->xcb_get_modifier_mapping_reply =
			handle.sym<decltype(this->xcb_get_modifier_mapping_reply)>("xcb_get_modifier_mapping_reply");
	this->xcb_get_modifier_mapping_keycodes =
			handle.sym<decltype(this->xcb_get_modifier_mapping_keycodes)>("xcb_get_modifier_mapping_keycodes");

	this->xcb_get_keyboard_mapping =
			handle.sym<decltype(this->xcb_get_keyboard_mapping)>("xcb_get_keyboard_mapping");
	this->xcb_get_keyboard_mapping_reply =
			handle.sym<decltype(this->xcb_get_keyboard_mapping_reply)>("xcb_get_keyboard_mapping_reply");

	if (this->xcb_connect
			&& this->xcb_get_setup
			&& this->xcb_setup_roots_iterator
			&& this->xcb_screen_next
			&& this->xcb_connection_has_error
			&& this->xcb_get_file_descriptor
			&& this->xcb_generate_id
			&& this->xcb_flush
			&& this->xcb_disconnect
			&& this->xcb_poll_for_event
			&& this->xcb_get_extension_data
			&& this->xcb_map_window
			&& this->xcb_create_window
			&& this->xcb_change_property
			&& this->xcb_intern_atom
			&& this->xcb_intern_atom_reply
			&& this->xcb_wait_for_reply
			&& this->xcb_get_modifier_mapping_unchecked
			&& this->xcb_get_modifier_mapping_reply
			&& this->xcb_get_modifier_mapping_keycodes
			&& this->xcb_get_keyboard_mapping
			&& this->xcb_get_keyboard_mapping_reply) {
		_handle = move(handle);
		openAux();
		return true;
	}
	return false;
}

void XcbLibrary::close() {
	if (_pending.connection) {
		xcb_disconnect(_pending.connection);
	}

	if (s_XcbLibrary == this) {
		s_XcbLibrary = nullptr;
	}
}

XcbLibrary::ConnectionData XcbLibrary::acquireConnection() {
	if (_pending.connection) {
		_current = _pending;
		_pending = ConnectionData({-1, nullptr, nullptr, nullptr});
		return _current;
	} else {
		openConnection(_current);
		return _current;
	}
}

XcbLibrary::ConnectionData XcbLibrary::getActiveConnection() const {
	if (_pending.connection) {
		return _pending;
	} else {
		return _current;
	}
}

void XcbLibrary::openAux() {
	if (auto randr = Dso("libxcb-randr.so")) {
		this->xcb_randr_id =
				randr.sym<decltype(this->xcb_randr_id)>("xcb_randr_id");

		this->xcb_randr_query_version =
				randr.sym<decltype(this->xcb_randr_query_version)>("xcb_randr_query_version");
		this->xcb_randr_query_version_reply =
				randr.sym<decltype(this->xcb_randr_query_version_reply)>("xcb_randr_query_version_reply");
		this->xcb_randr_get_screen_info_unchecked =
				randr.sym<decltype(this->xcb_randr_get_screen_info_unchecked)>("xcb_randr_get_screen_info_unchecked");
		this->xcb_randr_get_screen_info_reply =
				randr.sym<decltype(this->xcb_randr_get_screen_info_reply)>("xcb_randr_get_screen_info_reply");

		this->xcb_randr_get_screen_info_sizes =
				randr.sym<decltype(this->xcb_randr_get_screen_info_sizes)>("xcb_randr_get_screen_info_sizes");;
		this->xcb_randr_get_screen_info_sizes_length =
				randr.sym<decltype(this->xcb_randr_get_screen_info_sizes_length)>("xcb_randr_get_screen_info_sizes_length");
		this->xcb_randr_get_screen_info_sizes_iterator =
				randr.sym<decltype(this->xcb_randr_get_screen_info_sizes_iterator)>("xcb_randr_get_screen_info_sizes_iterator");
		this->xcb_randr_get_screen_info_rates_length =
				randr.sym<decltype(this->xcb_randr_get_screen_info_rates_length)>("xcb_randr_get_screen_info_rates_length");
		this->xcb_randr_get_screen_info_rates_iterator =
				randr.sym<decltype(this->xcb_randr_get_screen_info_rates_iterator)>("xcb_randr_get_screen_info_rates_iterator");
		this->xcb_randr_refresh_rates_next =
				randr.sym<decltype(this->xcb_randr_refresh_rates_next)>("xcb_randr_refresh_rates_next");
		this->xcb_randr_refresh_rates_end =
				randr.sym<decltype(this->xcb_randr_refresh_rates_end)>("xcb_randr_refresh_rates_end");
		this->xcb_randr_refresh_rates_rates =
				randr.sym<decltype(this->xcb_randr_refresh_rates_rates)>("xcb_randr_refresh_rates_rates");
		this->xcb_randr_refresh_rates_rates_length =
				randr.sym<decltype(this->xcb_randr_refresh_rates_rates_length)>("xcb_randr_refresh_rates_rates_length");

		this->xcb_randr_get_screen_resources =
				randr.sym<decltype(this->xcb_randr_get_screen_resources)>("xcb_randr_get_screen_resources");
		this->xcb_randr_get_screen_resources_unchecked =
				randr.sym<decltype(this->xcb_randr_get_screen_resources_unchecked)>("xcb_randr_get_screen_resources_unchecked");
		this->xcb_randr_get_screen_resources_reply =
				randr.sym<decltype(this->xcb_randr_get_screen_resources_reply)>("xcb_randr_get_screen_resources_reply");
		this->xcb_randr_get_screen_resources_modes =
					randr.sym<decltype(this->xcb_randr_get_screen_resources_modes)>("xcb_randr_get_screen_resources_modes");
		this->xcb_randr_get_screen_resources_modes_length =
					randr.sym<decltype(this->xcb_randr_get_screen_resources_modes_length)>("xcb_randr_get_screen_resources_modes_length");

		this->xcb_randr_get_screen_resources_current =
				randr.sym<decltype(this->xcb_randr_get_screen_resources_current)>("xcb_randr_get_screen_resources_current");
		this->xcb_randr_get_screen_resources_current_unchecked =
				randr.sym<decltype(this->xcb_randr_get_screen_resources_current_unchecked)>("xcb_randr_get_screen_resources_current_unchecked");
		this->xcb_randr_get_screen_resources_current_reply =
				randr.sym<decltype(this->xcb_randr_get_screen_resources_current_reply)>("xcb_randr_get_screen_resources_current_reply");
		this->xcb_randr_get_screen_resources_current_outputs =
				randr.sym<decltype(this->xcb_randr_get_screen_resources_current_outputs)>("xcb_randr_get_screen_resources_current_outputs");
		this->xcb_randr_get_screen_resources_current_outputs_length =
				randr.sym<decltype(this->xcb_randr_get_screen_resources_current_outputs_length)>("xcb_randr_get_screen_resources_current_outputs_length");
		this->xcb_randr_get_screen_resources_current_modes =
				randr.sym<decltype(this->xcb_randr_get_screen_resources_current_modes)>("xcb_randr_get_screen_resources_current_modes");
		this->xcb_randr_get_screen_resources_current_modes_length =
				randr.sym<decltype(this->xcb_randr_get_screen_resources_current_modes_length)>("xcb_randr_get_screen_resources_current_modes_length");
		this->xcb_randr_get_screen_resources_current_names =
				randr.sym<decltype(this->xcb_randr_get_screen_resources_current_names)>("xcb_randr_get_screen_resources_current_names");
		this->xcb_randr_get_screen_resources_current_names_length =
				randr.sym<decltype(this->xcb_randr_get_screen_resources_current_names_length)>("xcb_randr_get_screen_resources_current_names_length");
		this->xcb_randr_get_screen_resources_current_crtcs =
				randr.sym<decltype(this->xcb_randr_get_screen_resources_current_crtcs)>("xcb_randr_get_screen_resources_current_crtcs");
		this->xcb_randr_get_screen_resources_current_crtcs_length =
				randr.sym<decltype(this->xcb_randr_get_screen_resources_current_crtcs_length)>("xcb_randr_get_screen_resources_current_crtcs_length");

		this->xcb_randr_get_output_primary =
				randr.sym<decltype(this->xcb_randr_get_output_primary)>("xcb_randr_get_output_primary");
		this->xcb_randr_get_output_primary_unchecked =
				randr.sym<decltype(this->xcb_randr_get_output_primary_unchecked)>("xcb_randr_get_output_primary_unchecked");
		this->xcb_randr_get_output_primary_reply =
				randr.sym<decltype(this->xcb_randr_get_output_primary_reply)>("xcb_randr_get_output_primary_reply");

		this->xcb_randr_get_output_info =
				randr.sym<decltype(this->xcb_randr_get_output_info)>("xcb_randr_get_output_info");
		this->xcb_randr_get_output_info_unchecked =
				randr.sym<decltype(this->xcb_randr_get_output_info_unchecked)>("xcb_randr_get_output_info_unchecked");
		this->xcb_randr_get_output_info_reply =
				randr.sym<decltype(this->xcb_randr_get_output_info_reply)>("xcb_randr_get_output_info_reply");
		this->xcb_randr_get_output_info_crtcs =
				randr.sym<decltype(this->xcb_randr_get_output_info_crtcs)>("xcb_randr_get_output_info_crtcs");
		this->xcb_randr_get_output_info_crtcs_length =
				randr.sym<decltype(this->xcb_randr_get_output_info_crtcs_length)>("xcb_randr_get_output_info_crtcs_length");
		this->xcb_randr_get_output_info_crtcs_end =
				randr.sym<decltype(this->xcb_randr_get_output_info_crtcs_end)>("xcb_randr_get_output_info_crtcs_end");
		this->xcb_randr_get_output_info_modes =
				randr.sym<decltype(this->xcb_randr_get_output_info_modes)>("xcb_randr_get_output_info_modes");
		this->xcb_randr_get_output_info_modes_length =
				randr.sym<decltype(this->xcb_randr_get_output_info_modes_length)>("xcb_randr_get_output_info_modes_length");
		this->xcb_randr_get_output_info_name =
				randr.sym<decltype(this->xcb_randr_get_output_info_name)>("xcb_randr_get_output_info_name");
		this->xcb_randr_get_output_info_name_length =
				randr.sym<decltype(this->xcb_randr_get_output_info_name_length)>("xcb_randr_get_output_info_name_length");

		this->xcb_randr_get_crtc_info =
				randr.sym<decltype(this->xcb_randr_get_crtc_info)>("xcb_randr_get_crtc_info");
		this->xcb_randr_get_crtc_info_unchecked =
				randr.sym<decltype(this->xcb_randr_get_crtc_info_unchecked)>("xcb_randr_get_crtc_info_unchecked");
		this->xcb_randr_get_crtc_info_reply =
				randr.sym<decltype(this->xcb_randr_get_crtc_info_reply)>("xcb_randr_get_crtc_info_reply");
		this->xcb_randr_get_crtc_info_outputs =
				randr.sym<decltype(this->xcb_randr_get_crtc_info_outputs)>("xcb_randr_get_crtc_info_outputs");
		this->xcb_randr_get_crtc_info_outputs_length =
				randr.sym<decltype(this->xcb_randr_get_crtc_info_outputs_length)>("xcb_randr_get_crtc_info_outputs_length");
		this->xcb_randr_get_crtc_info_possible =
				randr.sym<decltype(this->xcb_randr_get_crtc_info_possible)>("xcb_randr_get_crtc_info_possible");
		this->xcb_randr_get_crtc_info_possible_length =
				randr.sym<decltype(this->xcb_randr_get_crtc_info_possible_length)>("xcb_randr_get_crtc_info_possible_length");

		if (this->xcb_randr_id
				&&this->xcb_randr_query_version
				&& this->xcb_randr_query_version_reply
				&& this->xcb_randr_get_screen_info_unchecked
				&& this->xcb_randr_get_screen_info_reply
				&& this->xcb_randr_get_screen_info_sizes
				&& this->xcb_randr_get_screen_info_sizes_length
				&& this->xcb_randr_get_screen_info_sizes_iterator
				&& this->xcb_randr_get_screen_info_rates_length
				&& this->xcb_randr_get_screen_info_rates_iterator
				&& this->xcb_randr_refresh_rates_next
				&& this->xcb_randr_refresh_rates_end
				&& this->xcb_randr_refresh_rates_rates
				&& this->xcb_randr_refresh_rates_rates_length
				&& this->xcb_randr_get_screen_resources
				&& this->xcb_randr_get_screen_resources_unchecked
				&& this->xcb_randr_get_screen_resources_reply
				&& this->xcb_randr_get_screen_resources_modes
				&& this->xcb_randr_get_screen_resources_modes_length
				&& this->xcb_randr_get_screen_resources_current
				&& this->xcb_randr_get_screen_resources_current_unchecked
				&& this->xcb_randr_get_screen_resources_current_reply
				&& this->xcb_randr_get_screen_resources_current_outputs
				&& this->xcb_randr_get_screen_resources_current_outputs_length
				&& this->xcb_randr_get_screen_resources_current_modes
				&& this->xcb_randr_get_screen_resources_current_modes_length
				&& this->xcb_randr_get_screen_resources_current_names
				&& this->xcb_randr_get_screen_resources_current_names_length
				&& this->xcb_randr_get_screen_resources_current_crtcs
				&& this->xcb_randr_get_screen_resources_current_crtcs_length
				&& this->xcb_randr_get_output_primary
				&& this->xcb_randr_get_output_primary_unchecked
				&& this->xcb_randr_get_output_primary_reply
				&& this->xcb_randr_get_output_info
				&& this->xcb_randr_get_output_info_unchecked
				&& this->xcb_randr_get_output_info_reply
				&& this->xcb_randr_get_output_info_crtcs
				&& this->xcb_randr_get_output_info_crtcs_length
				&& this->xcb_randr_get_output_info_crtcs_end
				&& this->xcb_randr_get_output_info_modes
				&& this->xcb_randr_get_output_info_modes_length
				&& this->xcb_randr_get_output_info_name
				&& this->xcb_randr_get_output_info_name_length
				&& this->xcb_randr_get_crtc_info
				&& this->xcb_randr_get_crtc_info_unchecked
				&& this->xcb_randr_get_crtc_info_reply
				&& this->xcb_randr_get_crtc_info_outputs
				&& this->xcb_randr_get_crtc_info_outputs_length
				&& this->xcb_randr_get_crtc_info_possible
				&& this->xcb_randr_get_crtc_info_possible_length) {
			_randr = move(randr);
		} else {
			this->xcb_randr_query_version = nullptr;
			this->xcb_randr_query_version_reply = nullptr;
			this->xcb_randr_get_screen_info_unchecked = nullptr;
			this->xcb_randr_get_screen_info_reply = nullptr;
			this->xcb_randr_get_screen_info_sizes = nullptr;
			this->xcb_randr_get_screen_info_sizes_length = nullptr;
			this->xcb_randr_get_screen_info_sizes_iterator = nullptr;
			this->xcb_randr_get_screen_info_rates_length = nullptr;
			this->xcb_randr_get_screen_info_rates_iterator = nullptr;
			this->xcb_randr_refresh_rates_next = nullptr;
			this->xcb_randr_refresh_rates_rates = nullptr;
			this->xcb_randr_refresh_rates_rates_length = nullptr;
			this->xcb_randr_get_screen_resources = nullptr;
			this->xcb_randr_get_screen_resources_unchecked = nullptr;
			this->xcb_randr_get_screen_resources_reply = nullptr;
			this->xcb_randr_get_screen_resources_modes = nullptr;
			this->xcb_randr_get_screen_resources_modes_length = nullptr;
			this->xcb_randr_get_screen_resources_current = nullptr;
			this->xcb_randr_get_screen_resources_current_unchecked = nullptr;
			this->xcb_randr_get_screen_resources_current_reply = nullptr;
		}
	}

	if (auto keysyms = Dso("libxcb-keysyms.so")) {
		this->xcb_key_symbols_alloc = keysyms.sym<decltype(xcb_key_symbols_alloc)>("xcb_key_symbols_alloc");
		this->xcb_key_symbols_free = keysyms.sym<decltype(xcb_key_symbols_free)>("xcb_key_symbols_free");
		this->xcb_key_symbols_get_keysym = keysyms.sym<decltype(xcb_key_symbols_get_keysym)>("xcb_key_symbols_get_keysym");
		this->xcb_key_symbols_get_keycode = keysyms.sym<decltype(xcb_key_symbols_get_keycode)>("xcb_key_symbols_get_keycode");
		this->xcb_key_press_lookup_keysym = keysyms.sym<decltype(xcb_key_press_lookup_keysym)>("xcb_key_press_lookup_keysym");
		this->xcb_key_release_lookup_keysym = keysyms.sym<decltype(xcb_key_release_lookup_keysym)>("xcb_key_release_lookup_keysym");
		this->xcb_refresh_keyboard_mapping = keysyms.sym<decltype(xcb_refresh_keyboard_mapping)>("xcb_refresh_keyboard_mapping");
		this->xcb_is_keypad_key = keysyms.sym<decltype(xcb_is_keypad_key)>("xcb_is_keypad_key");
		this->xcb_is_private_keypad_key = keysyms.sym<decltype(xcb_is_private_keypad_key)>("xcb_is_private_keypad_key");
		this->xcb_is_cursor_key = keysyms.sym<decltype(xcb_is_cursor_key)>("xcb_is_cursor_key");
		this->xcb_is_pf_key = keysyms.sym<decltype(xcb_is_pf_key)>("xcb_is_pf_key");
		this->xcb_is_function_key = keysyms.sym<decltype(xcb_is_function_key)>("xcb_is_function_key");
		this->xcb_is_misc_function_key = keysyms.sym<decltype(xcb_is_misc_function_key)>("xcb_is_misc_function_key");
		this->xcb_is_modifier_key = keysyms.sym<decltype(xcb_is_modifier_key)>("xcb_is_modifier_key");

		if (this->xcb_key_symbols_alloc
				&& this->xcb_key_symbols_free
				&& this->xcb_key_symbols_get_keysym
				&& this->xcb_key_symbols_get_keycode
				&& this->xcb_key_press_lookup_keysym
				&& this->xcb_key_release_lookup_keysym
				&& this->xcb_refresh_keyboard_mapping
				&& this->xcb_is_keypad_key
				&& this->xcb_is_private_keypad_key
				&& this->xcb_is_cursor_key
				&& this->xcb_is_pf_key
				&& this->xcb_is_function_key
				&& this->xcb_is_misc_function_key
				&& this->xcb_is_modifier_key) {
			_keysyms = move(keysyms);
		} else {
			this->xcb_key_symbols_alloc = nullptr;
			this->xcb_key_symbols_free = nullptr;
			this->xcb_key_symbols_get_keysym = nullptr;
			this->xcb_key_symbols_get_keycode = nullptr;
			this->xcb_key_press_lookup_keysym = nullptr;
			this->xcb_key_release_lookup_keysym = nullptr;
			this->xcb_refresh_keyboard_mapping = nullptr;
			this->xcb_is_keypad_key = nullptr;
			this->xcb_is_private_keypad_key = nullptr;
			this->xcb_is_cursor_key = nullptr;
			this->xcb_is_pf_key = nullptr;
			this->xcb_is_function_key = nullptr;
			this->xcb_is_misc_function_key = nullptr;
			this->xcb_is_modifier_key = nullptr;
		}
	}

	if (auto xkb = Dso("libxcb-xkb.so")) {
		this->xcb_xkb_select_events = xkb.sym<decltype(xcb_xkb_select_events)>("xcb_xkb_select_events");

		if (this->xcb_xkb_select_events) {
			_xkb = move(xkb);
		} else {
			this->xcb_xkb_select_events = nullptr;
		}
	}
}

void XcbLibrary::openConnection(ConnectionData &data) {
	data.setup = nullptr;
	data.screen = nullptr;
	data.connection = this->xcb_connect(NULL, &data.screen_nbr); // always not null
	int screen_nbr = data.screen_nbr;
	data.setup = this->xcb_get_setup(data.connection);
	auto iter = this->xcb_setup_roots_iterator(data.setup);
	for (; iter.rem; --screen_nbr, this->xcb_screen_next(&iter)) {
		if (screen_nbr == 0) {
			data.screen = iter.data;
			break;
		}
	}
}

}
