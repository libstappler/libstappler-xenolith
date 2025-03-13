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
#include "XLPlatformLinuxXcbConnection.h"

#if LINUX

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class SP_PUBLIC XcbView : public LinuxViewInterface, public XcbWindowInterface {
public:
	virtual ~XcbView();

	XcbView(Rc<XcbConnection> &&, ViewInterface *, const WindowInfo &);

	virtual void handleConfigureNotify(xcb_configure_notify_event_t *) override;

	virtual void handleButtonPress(xcb_button_press_event_t *) override;
	virtual void handleButtonRelease(xcb_button_release_event_t *) override;
	virtual void handleMotionNotify(xcb_motion_notify_event_t *) override;
	virtual void handleEnterNotify(xcb_enter_notify_event_t *) override;
	virtual void handleLeaveNotify(xcb_leave_notify_event_t *) override;
	virtual void handleFocusIn(xcb_focus_in_event_t *) override;
	virtual void handleFocusOut(xcb_focus_out_event_t *) override;
	virtual void handleKeyPress(xcb_key_press_event_t *) override;
	virtual void handleKeyRelease(xcb_key_release_event_t *) override;

	virtual void handleSelectionNotify(xcb_selection_notify_event_t *) override;
	virtual void handleSelectionRequest(xcb_selection_request_event_t *e) override;

	virtual void handleSyncRequest(xcb_timestamp_t, xcb_sync_int64_t) override;
	virtual void handleCloseRequest() override;

	virtual void handleScreenChangeNotify(xcb_randr_screen_change_notify_event_t *) override;

	virtual void dispatchPendingEvents() override;

	virtual uint64_t getScreenFrameInterval() const override;

	virtual void mapWindow() override;

	xcb_window_t getWindow() const { return _info.window; }
	xcb_connection_t *getConnection() const;

	virtual void handleFramePresented() override;

	virtual void readFromClipboard(Function<void(BytesView, StringView)> &&, Ref *) override;
	virtual void writeToClipboard(BytesView, StringView contentType) override;

	virtual int getSocketFd() const override;
	virtual bool poll(bool frameReady) override;

	virtual core::FrameConstraints exportConstraints(core::FrameConstraints &&) const override;

protected:
	void notifyClipboard(BytesView);

	xcb_atom_t writeTargetToProperty(xcb_selection_request_event_t *request);

	void updateWindowAttributes();
	void configureWindow(xcb_rectangle_t r, uint16_t border_width);

	Rc<XcbConnection> _connection;
	XcbLibrary *_xcb = nullptr;
	XkbLibrary *_xkb = nullptr;

	ViewInterface *_view = nullptr;

	xcb_screen_t *_defaultScreen = nullptr;

	XcbWindowInfo _info;

	xcb_timestamp_t _lastInputTime = 0;
	xcb_timestamp_t _lastSyncTime = 0;
	Vector<core::InputEventData> _pendingEvents;
	bool _deprecateSwapchain = false;
	bool _shouldClose = false;

	uint16_t _borderWidth = 0;
	uint16_t _rate = 60;

	xkb_keymap *_xkbKeymap = nullptr;
	xkb_state *_xkbState = nullptr;
	xkb_compose_state *_xkbCompose = nullptr;

	String _wmClass;
	ScreenInfoData _screenInfo;

	Function<void(BytesView, StringView)> _clipboardCallback;
	Rc<Ref> _clipboardTarget;
	Bytes _clipboardSelection;
};

}

#endif

#endif /* XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCBVIEW_H_ */
