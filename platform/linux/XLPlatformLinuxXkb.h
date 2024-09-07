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

#ifndef XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXKB_H_
#define XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXKB_H_

#include "XLCommon.h"
#include "SPDso.h"

#if LINUX

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-names.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class SP_PUBLIC XkbLibrary : public Ref {
public:
	static XkbLibrary *getInstance();

	XkbLibrary() { }

	virtual ~XkbLibrary();

	bool init();
	void close();

	bool hasX11() const { return _x11 ? true : false; }

	struct xkb_context *getContext() const { return _context; }

	decltype(&::xkb_context_new) xkb_context_new = nullptr;
	decltype(&::xkb_context_ref) xkb_context_ref = nullptr;
	decltype(&::xkb_context_unref) xkb_context_unref = nullptr;
	decltype(&::xkb_keymap_unref) xkb_keymap_unref = nullptr;
	decltype(&::xkb_state_unref) xkb_state_unref = nullptr;
	decltype(&::xkb_keymap_new_from_string) xkb_keymap_new_from_string = nullptr;
	decltype(&::xkb_state_new) xkb_state_new = nullptr;
	decltype(&::xkb_state_update_mask) xkb_state_update_mask = nullptr;
	decltype(&::xkb_state_key_get_utf8) xkb_state_key_get_utf8 = nullptr;
	decltype(&::xkb_state_key_get_utf32) xkb_state_key_get_utf32 = nullptr;
	decltype(&::xkb_state_key_get_one_sym) xkb_state_key_get_one_sym = nullptr;
	decltype(&::xkb_state_mod_index_is_active) xkb_state_mod_index_is_active = nullptr;
	decltype(&::xkb_state_key_get_syms) xkb_state_key_get_syms = nullptr;
	decltype(&::xkb_state_get_keymap) xkb_state_get_keymap = nullptr;
	decltype(&::xkb_keymap_key_for_each) xkb_keymap_key_for_each = nullptr;
	decltype(&::xkb_keymap_key_get_name) xkb_keymap_key_get_name = nullptr;
	decltype(&::xkb_keymap_mod_get_index) xkb_keymap_mod_get_index = nullptr;
	decltype(&::xkb_keymap_key_repeats) xkb_keymap_key_repeats = nullptr;
	decltype(&::xkb_keysym_to_utf32) xkb_keysym_to_utf32 = nullptr;
	decltype(&::xkb_compose_table_new_from_locale) xkb_compose_table_new_from_locale = nullptr;
	decltype(&::xkb_compose_table_unref) xkb_compose_table_unref = nullptr;
	decltype(&::xkb_compose_state_new) xkb_compose_state_new = nullptr;
	decltype(&::xkb_compose_state_feed) xkb_compose_state_feed = nullptr;
	decltype(&::xkb_compose_state_reset) xkb_compose_state_reset = nullptr;
	decltype(&::xkb_compose_state_get_status) xkb_compose_state_get_status = nullptr;
	decltype(&::xkb_compose_state_get_one_sym) xkb_compose_state_get_one_sym = nullptr;
	decltype(&::xkb_compose_state_unref) xkb_compose_state_unref = nullptr;
	decltype(&::xkb_x11_setup_xkb_extension) xkb_x11_setup_xkb_extension = nullptr;
	decltype(&::xkb_x11_get_core_keyboard_device_id) xkb_x11_get_core_keyboard_device_id = nullptr;
	decltype(&::xkb_x11_keymap_new_from_device) xkb_x11_keymap_new_from_device = nullptr;
	decltype(&::xkb_x11_state_new_from_device) xkb_x11_state_new_from_device = nullptr;

protected:
	bool open(Dso &);
	void openAux();

	Dso _handle;
	Dso _x11;
	struct xkb_context *_context = nullptr;
};

}

#endif

#endif /* XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXKB_H_ */
