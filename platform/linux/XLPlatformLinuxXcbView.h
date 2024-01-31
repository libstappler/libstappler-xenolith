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

#ifndef XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCBVIEW_H_
#define XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCBVIEW_H_

#include "XLPlatformLinuxXcb.h"
#include "XLPlatformLinuxXkb.h"
#include "XLPlatformLinuxView.h"

#if LINUX

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class XcbView : public LinuxViewInterface {
public:
	struct ScreenInfo {
		uint16_t width;
		uint16_t height;
		uint16_t mwidth;
		uint16_t mheight;
		Vector<uint16_t> rates;
	};

	struct ModeInfo {
		uint32_t id;
	    uint16_t width;
	    uint16_t height;
	    uint16_t rate;
	    String name;
	};

	struct CrtcInfo {
		xcb_randr_crtc_t crtc;
		int16_t x;
		int16_t y;
		uint16_t width;
		uint16_t height;
		xcb_randr_mode_t mode;
		uint16_t rotation;
		uint16_t rotations;
		Vector<xcb_randr_output_t> outputs;
		Vector<xcb_randr_output_t> possible;
	};

	struct OutputInfo {
		xcb_randr_output_t output;
	    xcb_randr_crtc_t crtc;
		Vector<xcb_randr_mode_t> modes;
		String name;
	};

	struct ScreenInfoData {
		Vector<xcb_randr_crtc_t> currentCrtcs;
		Vector<xcb_randr_output_t> currentOutputs;
		Vector<ModeInfo> currentModeInfo;
		Vector<ModeInfo> modeInfo;
		Vector<ScreenInfo> screenInfo;
		Vector<CrtcInfo> crtcInfo;

		OutputInfo primaryOutput;
		CrtcInfo primaryCrtc;
		ModeInfo primaryMode;
		xcb_timestamp_t config;
	};

	static void ReportError(int error);

	XcbView(XcbLibrary *, ViewInterface *, StringView, StringView bundleId, URect);
	virtual ~XcbView();

	bool valid() const;

	virtual bool poll(bool frameReady) override;

	virtual int getSocketFd() const override { return _socket; }

	virtual uint64_t getScreenFrameInterval() const override;

	virtual void mapWindow() override;

	xcb_connection_t *getConnection() const { return _connection; }
	uint32_t getWindow() const { return _window; }

protected:
	ScreenInfoData getScreenInfo() const;

	void initXkb();

	void updateXkbMapping();
	void updateKeysymMapping();
	xcb_keysym_t getKeysym(xcb_keycode_t code, uint16_t state, bool resolveMods = true);
	xkb_keysym_t composeSymbol(xkb_keysym_t sym, core::InputKeyComposeState &compose) const;

	void updateXkbKey(xcb_keycode_t code);

	// get code from keysym mapping table
	core::InputKeyCode getKeyCode(xcb_keycode_t code) const;

	// map keysym to InputKeyCode
	core::InputKeyCode getKeysymCode(xcb_keysym_t sym) const;

	Rc<XcbLibrary> _xcb;
	Rc<XkbLibrary> _xkb;
	ViewInterface *_view = nullptr;
	xcb_connection_t *_connection = nullptr;
	xcb_screen_t *_defaultScreen = nullptr;
	xcb_key_symbols_t *_keysyms = nullptr;
	uint32_t _window = 0;

	xcb_atom_t _atoms[sizeof(s_atomRequests) / sizeof(XcbAtomRequest)];

	uint16_t _width = 0;
	uint16_t _height = 0;
	uint16_t _rate = 60;

	int _socket = -1;

	uint16_t _numlock = 0;
	uint16_t _shiftlock = 0;
	uint16_t _capslock = 0;
	uint16_t _modeswitch = 0;

	bool _xcbSetup = false;
	bool _randrEnabled = true;
	int32_t _xkbDeviceId = 0;
	uint8_t _xkbFirstEvent = 0;
	uint8_t _xkbFirstError = 0;
	uint8_t _randrFirstEvent = 0;
	xkb_keymap *_xkbKeymap = nullptr;
	xkb_state *_xkbState = nullptr;
	xkb_compose_state *_xkbCompose = nullptr;
	core::InputKeyCode _keycodes[256] = { core::InputKeyCode::Unknown };

	String _wmClass;
	ScreenInfoData _screenInfo;
};

}

#endif

#endif /* XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCBVIEW_H_ */
