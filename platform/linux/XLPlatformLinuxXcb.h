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

#ifndef XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCB_H_
#define XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCB_H_

#include "XLCommon.h"
#include "SPDso.h"

#if LINUX

#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/sync.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_cursor.h>

#define explicit _explicit;
#include <xcb/xkb.h>
#undef explicit

#define XL_X11_DEBUG 1

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class XcbConnection;

#define XL_XCB_DEFINE_PROTO(name) decltype(&::name) name = nullptr;
#define XL_XCB_LOAD_PROTO(handle, name) this->name = handle.sym<decltype(this->name)>(#name);

SP_PUBLIC void _xl_xcb_null_fn();

class SP_PUBLIC XcbLibrary : public Ref {
public:
	static constexpr int RANDR_MAJOR_VERSION = XCB_RANDR_MAJOR_VERSION;
	static constexpr int RANDR_MINOR_VERSION = XCB_RANDR_MINOR_VERSION;

	static XcbLibrary *getInstance();

	XcbLibrary() { }

	virtual ~XcbLibrary();

	bool init();

	bool open(Dso &handle);
	void close();

	bool hasRandr() const;
	bool hasKeysyms() const;
	bool hasXkb() const;
	bool hasSync() const;

	decltype(&_xl_xcb_null_fn) _xcb_first_fn = &_xl_xcb_null_fn;
	XL_XCB_DEFINE_PROTO(xcb_connect)
	XL_XCB_DEFINE_PROTO(xcb_get_setup)
	XL_XCB_DEFINE_PROTO(xcb_setup_roots_iterator)
	XL_XCB_DEFINE_PROTO(xcb_screen_next)
	XL_XCB_DEFINE_PROTO(xcb_connection_has_error)
	XL_XCB_DEFINE_PROTO(xcb_get_file_descriptor)
	XL_XCB_DEFINE_PROTO(xcb_generate_id)
	XL_XCB_DEFINE_PROTO(xcb_flush)
	XL_XCB_DEFINE_PROTO(xcb_disconnect)
	XL_XCB_DEFINE_PROTO(xcb_poll_for_event)
	XL_XCB_DEFINE_PROTO(xcb_send_event)
	XL_XCB_DEFINE_PROTO(xcb_get_extension_data)
	XL_XCB_DEFINE_PROTO(xcb_map_window)
	XL_XCB_DEFINE_PROTO(xcb_create_window)
	XL_XCB_DEFINE_PROTO(xcb_configure_window)
	XL_XCB_DEFINE_PROTO(xcb_change_window_attributes)
	XL_XCB_DEFINE_PROTO(xcb_change_property)
	XL_XCB_DEFINE_PROTO(xcb_intern_atom)
	XL_XCB_DEFINE_PROTO(xcb_intern_atom_reply)
	XL_XCB_DEFINE_PROTO(xcb_get_property_reply)
	XL_XCB_DEFINE_PROTO(xcb_get_property)
	XL_XCB_DEFINE_PROTO(xcb_get_property_value)
	XL_XCB_DEFINE_PROTO(xcb_get_property_value_length)
	XL_XCB_DEFINE_PROTO(xcb_get_modifier_mapping_unchecked)
	XL_XCB_DEFINE_PROTO(xcb_get_modifier_mapping_reply)
	XL_XCB_DEFINE_PROTO(xcb_get_modifier_mapping_keycodes)
	XL_XCB_DEFINE_PROTO(xcb_convert_selection)
	XL_XCB_DEFINE_PROTO(xcb_set_selection_owner)
	XL_XCB_DEFINE_PROTO(xcb_get_selection_owner);
	XL_XCB_DEFINE_PROTO(xcb_get_selection_owner_reply)
	XL_XCB_DEFINE_PROTO(xcb_get_keyboard_mapping)
	XL_XCB_DEFINE_PROTO(xcb_get_keyboard_mapping_reply)
	XL_XCB_DEFINE_PROTO(xcb_request_check)
	XL_XCB_DEFINE_PROTO(xcb_open_font_checked)
	XL_XCB_DEFINE_PROTO(xcb_create_glyph_cursor)
	XL_XCB_DEFINE_PROTO(xcb_create_gc_checked)
	XL_XCB_DEFINE_PROTO(xcb_free_cursor)
	XL_XCB_DEFINE_PROTO(xcb_close_font_checked)

	// this function was not publicly exposed, but it's used in macros and inlines
	void *(*xcb_wait_for_reply)(xcb_connection_t *c, unsigned int request,
			xcb_generic_error_t **e) = nullptr;

	decltype(&_xl_xcb_null_fn) _xcb_last_fn = &_xl_xcb_null_fn;


	decltype(&_xl_xcb_null_fn) _xcb_randr_first_fn = &_xl_xcb_null_fn;
	XL_XCB_DEFINE_PROTO(xcb_randr_id)
	XL_XCB_DEFINE_PROTO(xcb_randr_query_version)
	XL_XCB_DEFINE_PROTO(xcb_randr_query_version_reply)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_info_unchecked)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_info_reply)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_info_sizes)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_info_sizes_length)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_info_sizes_iterator)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_info_rates_length)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_info_rates_iterator)
	XL_XCB_DEFINE_PROTO(xcb_randr_refresh_rates_next)
	XL_XCB_DEFINE_PROTO(xcb_randr_refresh_rates_end)
	XL_XCB_DEFINE_PROTO(xcb_randr_refresh_rates_rates)
	XL_XCB_DEFINE_PROTO(xcb_randr_refresh_rates_rates_length)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources_unchecked)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources_reply)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources_modes)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources_modes_length)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources_current)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources_current_unchecked)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources_current_reply)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources_current_outputs)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources_current_outputs_length)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources_current_modes)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources_current_modes_length)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources_current_names)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources_current_names_length)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources_current_crtcs)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_screen_resources_current_crtcs_length)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_output_primary)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_output_primary_unchecked)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_output_primary_reply)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_output_info)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_output_info_unchecked)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_output_info_reply)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_output_info_crtcs)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_output_info_crtcs_length)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_output_info_crtcs_end)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_output_info_modes)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_output_info_modes_length)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_output_info_name)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_output_info_name_length)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_crtc_info)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_crtc_info_unchecked)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_crtc_info_reply)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_crtc_info_outputs)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_crtc_info_outputs_length)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_crtc_info_possible)
	XL_XCB_DEFINE_PROTO(xcb_randr_get_crtc_info_possible_length)
	decltype(&_xl_xcb_null_fn) _xcb_randr_last_fn = &_xl_xcb_null_fn;

	decltype(&_xl_xcb_null_fn) _xcb_key_first_fn = &_xl_xcb_null_fn;
	XL_XCB_DEFINE_PROTO(xcb_key_symbols_alloc)
	XL_XCB_DEFINE_PROTO(xcb_key_symbols_free)
	XL_XCB_DEFINE_PROTO(xcb_key_symbols_get_keysym)
	XL_XCB_DEFINE_PROTO(xcb_key_symbols_get_keycode)
	XL_XCB_DEFINE_PROTO(xcb_key_press_lookup_keysym)
	XL_XCB_DEFINE_PROTO(xcb_key_release_lookup_keysym)
	XL_XCB_DEFINE_PROTO(xcb_refresh_keyboard_mapping)
	XL_XCB_DEFINE_PROTO(xcb_is_keypad_key)
	XL_XCB_DEFINE_PROTO(xcb_is_private_keypad_key)
	XL_XCB_DEFINE_PROTO(xcb_is_cursor_key)
	XL_XCB_DEFINE_PROTO(xcb_is_pf_key)
	XL_XCB_DEFINE_PROTO(xcb_is_function_key)
	XL_XCB_DEFINE_PROTO(xcb_is_misc_function_key)
	XL_XCB_DEFINE_PROTO(xcb_is_modifier_key)
	decltype(&_xl_xcb_null_fn) _xcb_key_last_fn = &_xl_xcb_null_fn;

	decltype(&_xl_xcb_null_fn) _xcb_xkb_first_fn = &_xl_xcb_null_fn;
	XL_XCB_DEFINE_PROTO(xcb_xkb_select_events)
	decltype(&_xl_xcb_null_fn) _xcb_xkb_last_fn = &_xl_xcb_null_fn;

	decltype(&_xl_xcb_null_fn) _xcb_sync_first_fn = &_xl_xcb_null_fn;
	XL_XCB_DEFINE_PROTO(xcb_sync_id)
	XL_XCB_DEFINE_PROTO(xcb_sync_create_counter)
	XL_XCB_DEFINE_PROTO(xcb_sync_create_counter_checked)
	XL_XCB_DEFINE_PROTO(xcb_sync_destroy_counter)
	XL_XCB_DEFINE_PROTO(xcb_sync_destroy_counter_checked)
	XL_XCB_DEFINE_PROTO(xcb_sync_set_counter)
	decltype(&_xl_xcb_null_fn) _xcb_sync_last_fn = &_xl_xcb_null_fn;

	decltype(&_xl_xcb_null_fn) _xcb_cursor_first_fn = &_xl_xcb_null_fn;
	XL_XCB_DEFINE_PROTO(xcb_cursor_context_new)
	XL_XCB_DEFINE_PROTO(xcb_cursor_load_cursor)
	XL_XCB_DEFINE_PROTO(xcb_cursor_context_free)
	decltype(&_xl_xcb_null_fn) _xcb_cursor_last_fn = &_xl_xcb_null_fn;

	XcbConnection *getCommonConnection();

	Rc<XcbConnection> acquireConnection();

protected:
	void openAux();

	Dso _handle;
	Dso _randr;
	Dso _keysyms;
	Dso _xkb;
	Dso _sync;
	Dso _cursor;

	std::mutex _connectionMutex;
	Rc<XcbConnection> _connection;
};

enum class XcbAtomIndex {
	WM_PROTOCOLS,
	WM_DELETE_WINDOW,
	WM_NAME,
	WM_ICON_NAME,
	_NET_WM_SYNC_REQUEST,
	_NET_WM_SYNC_REQUEST_COUNTER,
	SAVE_TARGETS,
	CLIPBOARD,
	PRIMARY,
	TARGETS,
	MULTIPLE,
	UTF8_STRING,
	XNULL,
	XENOLITH_CLIPBOARD
};

} // namespace stappler::xenolith::platform

#endif

#endif /* XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCB_H_ */
