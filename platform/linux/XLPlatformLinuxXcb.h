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

#ifndef XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCB_H_
#define XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCB_H_

#include "XLCommon.h"
#include "SPDso.h"

#if LINUX

#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/xcb_keysyms.h>

#define explicit _explicit;
#include <xcb/xkb.h>
#undef explicit

#define XL_X11_DEBUG 0

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class SP_PUBLIC XcbLibrary : public Ref {
public:
	static constexpr int RANDR_MAJOR_VERSION = XCB_RANDR_MAJOR_VERSION;
	static constexpr int RANDR_MINOR_VERSION = XCB_RANDR_MINOR_VERSION;

	struct ConnectionData {
		int screen_nbr = -1;
		xcb_connection_t *connection = nullptr;
		const xcb_setup_t *setup = nullptr;
		xcb_screen_t *screen = nullptr;
	};

	static XcbLibrary *getInstance();

	XcbLibrary() { }

	virtual ~XcbLibrary();

	bool init();

	bool open(Dso &handle);
	void close();

	bool hasRandr() const;
	bool hasKeysyms() const;
	bool hasXkb() const;

	decltype(&::xcb_connect) xcb_connect = nullptr;
	decltype(&::xcb_get_setup) xcb_get_setup = nullptr;
	decltype(&::xcb_setup_roots_iterator) xcb_setup_roots_iterator = nullptr;
	decltype(&::xcb_screen_next) xcb_screen_next = nullptr;
	decltype(&::xcb_connection_has_error) xcb_connection_has_error = nullptr;
	decltype(&::xcb_get_file_descriptor) xcb_get_file_descriptor = nullptr;
	decltype(&::xcb_generate_id) xcb_generate_id = nullptr;
	decltype(&::xcb_flush) xcb_flush = nullptr;
	decltype(&::xcb_disconnect) xcb_disconnect = nullptr;
	decltype(&::xcb_poll_for_event) xcb_poll_for_event = nullptr;
	decltype(&::xcb_send_event) xcb_send_event = nullptr;
	decltype(&::xcb_get_extension_data) xcb_get_extension_data = nullptr;
	decltype(&::xcb_map_window) xcb_map_window = nullptr;
	decltype(&::xcb_create_window) xcb_create_window = nullptr;
	decltype(&::xcb_change_property) xcb_change_property = nullptr;
	decltype(&::xcb_intern_atom) xcb_intern_atom = nullptr;
	decltype(&::xcb_intern_atom_reply) xcb_intern_atom_reply = nullptr;
	decltype(&::xcb_get_property_reply) xcb_get_property_reply = nullptr;
	decltype(&::xcb_get_property) xcb_get_property = nullptr;
	decltype(&::xcb_get_property_value) xcb_get_property_value = nullptr;
	decltype(&::xcb_get_property_value_length) xcb_get_property_value_length = nullptr;

	void * (* xcb_wait_for_reply) (xcb_connection_t *c, unsigned int request, xcb_generic_error_t **e) = nullptr;

	decltype(&::xcb_get_modifier_mapping_unchecked) xcb_get_modifier_mapping_unchecked = nullptr;
	decltype(&::xcb_get_modifier_mapping_reply) xcb_get_modifier_mapping_reply = nullptr;
	decltype(&::xcb_get_modifier_mapping_keycodes) xcb_get_modifier_mapping_keycodes = nullptr;
	decltype(&::xcb_convert_selection) xcb_convert_selection = nullptr;
	decltype(&::xcb_set_selection_owner) xcb_set_selection_owner = nullptr;
	decltype(&::xcb_get_selection_owner) xcb_get_selection_owner = nullptr;
	decltype(&::xcb_get_selection_owner_reply) xcb_get_selection_owner_reply = nullptr;
	decltype(&::xcb_get_keyboard_mapping) xcb_get_keyboard_mapping = nullptr;
	decltype(&::xcb_get_keyboard_mapping_reply) xcb_get_keyboard_mapping_reply = nullptr;
	decltype(&::xcb_randr_id) xcb_randr_id = nullptr;
	decltype(&::xcb_randr_query_version) xcb_randr_query_version = nullptr;
	decltype(&::xcb_randr_query_version_reply) xcb_randr_query_version_reply = nullptr;
	decltype(&::xcb_randr_get_screen_info_unchecked) xcb_randr_get_screen_info_unchecked = nullptr;
	decltype(&::xcb_randr_get_screen_info_reply) xcb_randr_get_screen_info_reply = nullptr;
	decltype(&::xcb_randr_get_screen_info_sizes) xcb_randr_get_screen_info_sizes = nullptr;
	decltype(&::xcb_randr_get_screen_info_sizes_length) xcb_randr_get_screen_info_sizes_length = nullptr;
	decltype(&::xcb_randr_get_screen_info_sizes_iterator) xcb_randr_get_screen_info_sizes_iterator = nullptr;
	decltype(&::xcb_randr_get_screen_info_rates_length) xcb_randr_get_screen_info_rates_length = nullptr;
	decltype(&::xcb_randr_get_screen_info_rates_iterator) xcb_randr_get_screen_info_rates_iterator = nullptr;
	decltype(&::xcb_randr_refresh_rates_next) xcb_randr_refresh_rates_next = nullptr;
	decltype(&::xcb_randr_refresh_rates_end) xcb_randr_refresh_rates_end = nullptr;
	decltype(&::xcb_randr_refresh_rates_rates) xcb_randr_refresh_rates_rates = nullptr;
	decltype(&::xcb_randr_refresh_rates_rates_length) xcb_randr_refresh_rates_rates_length = nullptr;
	decltype(&::xcb_randr_get_screen_resources) xcb_randr_get_screen_resources = nullptr;
	decltype(&::xcb_randr_get_screen_resources_unchecked) xcb_randr_get_screen_resources_unchecked = nullptr;
	decltype(&::xcb_randr_get_screen_resources_reply) xcb_randr_get_screen_resources_reply = nullptr;
	decltype(&::xcb_randr_get_screen_resources_modes) xcb_randr_get_screen_resources_modes = nullptr;
	decltype(&::xcb_randr_get_screen_resources_modes_length) xcb_randr_get_screen_resources_modes_length = nullptr;
	decltype(&::xcb_randr_get_screen_resources_current) xcb_randr_get_screen_resources_current = nullptr;
	decltype(&::xcb_randr_get_screen_resources_current_unchecked) xcb_randr_get_screen_resources_current_unchecked = nullptr;
	decltype(&::xcb_randr_get_screen_resources_current_reply) xcb_randr_get_screen_resources_current_reply = nullptr;
	decltype(&::xcb_randr_get_screen_resources_current_outputs) xcb_randr_get_screen_resources_current_outputs = nullptr;
	decltype(&::xcb_randr_get_screen_resources_current_outputs_length) xcb_randr_get_screen_resources_current_outputs_length = nullptr;
	decltype(&::xcb_randr_get_screen_resources_current_modes) xcb_randr_get_screen_resources_current_modes = nullptr;
	decltype(&::xcb_randr_get_screen_resources_current_modes_length) xcb_randr_get_screen_resources_current_modes_length = nullptr;
	decltype(&::xcb_randr_get_screen_resources_current_names) xcb_randr_get_screen_resources_current_names = nullptr;
	decltype(&::xcb_randr_get_screen_resources_current_names_length) xcb_randr_get_screen_resources_current_names_length = nullptr;
	decltype(&::xcb_randr_get_screen_resources_current_crtcs) xcb_randr_get_screen_resources_current_crtcs = nullptr;
	decltype(&::xcb_randr_get_screen_resources_current_crtcs_length) xcb_randr_get_screen_resources_current_crtcs_length = nullptr;
	decltype(&::xcb_randr_get_output_primary) xcb_randr_get_output_primary = nullptr;
	decltype(&::xcb_randr_get_output_primary_unchecked) xcb_randr_get_output_primary_unchecked = nullptr;
	decltype(&::xcb_randr_get_output_primary_reply) xcb_randr_get_output_primary_reply = nullptr;
	decltype(&::xcb_randr_get_output_info) xcb_randr_get_output_info = nullptr;
	decltype(&::xcb_randr_get_output_info_unchecked) xcb_randr_get_output_info_unchecked = nullptr;
	decltype(&::xcb_randr_get_output_info_reply) xcb_randr_get_output_info_reply = nullptr;
	decltype(&::xcb_randr_get_output_info_crtcs) xcb_randr_get_output_info_crtcs = nullptr;
	decltype(&::xcb_randr_get_output_info_crtcs_length) xcb_randr_get_output_info_crtcs_length = nullptr;
	decltype(&::xcb_randr_get_output_info_crtcs_end) xcb_randr_get_output_info_crtcs_end = nullptr;
	decltype(&::xcb_randr_get_output_info_modes) xcb_randr_get_output_info_modes = nullptr;
	decltype(&::xcb_randr_get_output_info_modes_length) xcb_randr_get_output_info_modes_length = nullptr;
	decltype(&::xcb_randr_get_output_info_name) xcb_randr_get_output_info_name = nullptr;
	decltype(&::xcb_randr_get_output_info_name_length) xcb_randr_get_output_info_name_length = nullptr;
	decltype(&::xcb_randr_get_crtc_info) xcb_randr_get_crtc_info = nullptr;
	decltype(&::xcb_randr_get_crtc_info_unchecked) xcb_randr_get_crtc_info_unchecked = nullptr;
	decltype(&::xcb_randr_get_crtc_info_reply) xcb_randr_get_crtc_info_reply = nullptr;
	decltype(&::xcb_randr_get_crtc_info_outputs) xcb_randr_get_crtc_info_outputs = nullptr;
	decltype(&::xcb_randr_get_crtc_info_outputs_length) xcb_randr_get_crtc_info_outputs_length = nullptr;
	decltype(&::xcb_randr_get_crtc_info_possible) xcb_randr_get_crtc_info_possible = nullptr;
	decltype(&::xcb_randr_get_crtc_info_possible_length) xcb_randr_get_crtc_info_possible_length = nullptr;
	decltype(&::xcb_key_symbols_alloc) xcb_key_symbols_alloc = nullptr;
	decltype(&::xcb_key_symbols_free) xcb_key_symbols_free = nullptr;
	decltype(&::xcb_key_symbols_get_keysym) xcb_key_symbols_get_keysym = nullptr;
	decltype(&::xcb_key_symbols_get_keycode) xcb_key_symbols_get_keycode = nullptr;
	decltype(&::xcb_key_press_lookup_keysym) xcb_key_press_lookup_keysym = nullptr;
	decltype(&::xcb_key_release_lookup_keysym) xcb_key_release_lookup_keysym = nullptr;
	decltype(&::xcb_refresh_keyboard_mapping) xcb_refresh_keyboard_mapping = nullptr;
	decltype(&::xcb_is_keypad_key) xcb_is_keypad_key = nullptr;
	decltype(&::xcb_is_private_keypad_key) xcb_is_private_keypad_key = nullptr;
	decltype(&::xcb_is_cursor_key) xcb_is_cursor_key = nullptr;
	decltype(&::xcb_is_pf_key) xcb_is_pf_key = nullptr;
	decltype(&::xcb_is_function_key) xcb_is_function_key = nullptr;
	decltype(&::xcb_is_misc_function_key) xcb_is_misc_function_key = nullptr;
	decltype(&::xcb_is_modifier_key) xcb_is_modifier_key = nullptr;
	decltype(&::xcb_xkb_select_events) xcb_xkb_select_events = nullptr;

	ConnectionData acquireConnection();
	ConnectionData getActiveConnection() const;

protected:
	void openAux();
	void openConnection(ConnectionData &data);

	Dso _handle;
	Dso _randr;
	Dso _keysyms;
	Dso _xkb;

	ConnectionData _pending;
	ConnectionData _current;
};

enum class XcbAtomIndex {
	WM_PROTOCOLS,
	WM_DELETE_WINDOW,
	WM_NAME,
	WM_ICON_NAME,
	SAVE_TARGETS,
	CLIPBOARD,
	PRIMARY,
	TARGETS,
	MULTIPLE,
	STRING,
	XNULL,
	XENOLITH_CLIPBOARD
};

struct XcbAtomRequest {
	StringView name;
	bool onlyIfExists;
};

static XcbAtomRequest s_atomRequests[] = {
	{ "WM_PROTOCOLS", true },
	{ "WM_DELETE_WINDOW", false },
	{ "WM_NAME", false },
	{ "WM_ICON_NAME", false },
	{ "SAVE_TARGETS", false },
	{ "CLIPBOARD", false },
	{ "PRIMARY", false },
	{ "TARGETS", false },
	{ "MULTIPLE", false },
	{ "UTF8_STRING", false },
	{ "NULL", false },
	{ "XENOLITH_CLIPBOARD", false },
};

}

#endif

#endif /* XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCB_H_ */
