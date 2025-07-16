/**
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

#ifndef XENOLITH_APPLICATION_LINUX_XLLINUXXCBWINDOW_H_
#define XENOLITH_APPLICATION_LINUX_XLLINUXXCBWINDOW_H_

#include "platform/XLContextNativeWindow.h"
#include "XLLinuxXcbConnection.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class LinuxContextController;

class XcbWindow : public ContextNativeWindow {
public:
	virtual ~XcbWindow();

	XcbWindow();

	virtual bool init(NotNull<XcbConnection>, Rc<WindowInfo> &&, NotNull<const ContextInfo>,
			NotNull<LinuxContextController>);

	virtual void handleConfigureNotify(xcb_configure_notify_event_t *);

	virtual void handleButtonPress(xcb_button_press_event_t *);
	virtual void handleButtonRelease(xcb_button_release_event_t *);
	virtual void handleMotionNotify(xcb_motion_notify_event_t *);
	virtual void handleEnterNotify(xcb_enter_notify_event_t *);
	virtual void handleLeaveNotify(xcb_leave_notify_event_t *);
	virtual void handleFocusIn(xcb_focus_in_event_t *);
	virtual void handleFocusOut(xcb_focus_out_event_t *);
	virtual void handleKeyPress(xcb_key_press_event_t *);
	virtual void handleKeyRelease(xcb_key_release_event_t *);

	virtual void handleSyncRequest(xcb_timestamp_t, xcb_sync_int64_t);
	virtual void handleCloseRequest();

	virtual void handleScreenChangeNotify(xcb_randr_screen_change_notify_event_t *);

	virtual void dispatchPendingEvents();

	xcb_window_t getWindow() const { return _xinfo.window; }
	xcb_connection_t *getConnection() const;

	virtual uint64_t getScreenFrameInterval() const override;

	virtual void mapWindow() override;
	virtual void unmapWindow() override;

	virtual void handleFramePresented(NotNull<core::PresentationFrame>) override;

	virtual core::FrameConstraints exportConstraints(core::FrameConstraints &&) const override;

	virtual void handleLayerUpdate(const WindowLayer &) override;

	virtual Extent2 getExtent() const override;

	virtual Rc<core::Surface> makeSurface(NotNull<core::Instance>) override;

	virtual bool close() override;

	virtual Status setFullscreen(const MonitorId &, const xenolith::ModeInfo &) override;

protected:
	virtual bool updateTextInput(const TextInputRequest &,
			TextInputFlags flags = TextInputFlags::RunIfDisabled) override;

	virtual void cancelTextInput() override;

	void updateWindowAttributes();
	void configureWindow(xcb_rectangle_t r, uint16_t border_width);

	Rc<XcbConnection> _connection;

	XcbLibrary *_xcb = nullptr;

	xcb_screen_t *_defaultScreen = nullptr;

	XcbWindowInfo _xinfo;

	xcb_timestamp_t _lastInputTime = 0;
	xcb_timestamp_t _lastSyncTime = 0;
	Vector<core::InputEventData> _pendingEvents;

	uint16_t _borderWidth = 0;
	uint16_t _rate = 60;

	String _wmClass;
	ScreenInfoData _screenInfo;
};

} // namespace stappler::xenolith::platform

#endif /* XENOLITH_APPLICATION_LINUX_XLLINUXXCBWINDOW_H_ */
