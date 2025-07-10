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

#ifndef XENOLITH_APPLICATION_LINUX_XLLINUXCONTEXTCONTROLLER_H_
#define XENOLITH_APPLICATION_LINUX_XLLINUXCONTEXTCONTROLLER_H_

#include "platform/XLContextController.h"

#include "XLLinuxXcbConnection.h"
#include "XLLinuxDBusLibrary.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class ContextNativeWindow;

class LinuxContextController : public ContextController {
public:
	static void acquireDefaultConfig(ContextConfig &, NativeContextHandle *);

	virtual ~LinuxContextController() = default;

	virtual bool init(NotNull<Context>, ContextConfig &&);

	virtual int run() override;

	XcbConnection *getXcbConnection() const { return _xcbConnection; }

	void notifyWindowResized(NotNull<ContextNativeWindow>) override;
	bool notifyWindowClosed(NotNull<ContextNativeWindow>) override;

	virtual Rc<AppWindow> makeAppWindow(NotNull<AppThread>, NotNull<ContextNativeWindow>) override;

protected:
	dbus_bool_t handleDbusEvent(dbus::Connection *c, const dbus::Event &);

	void tryStart();

	void updateNetworkState();
	bool loadInstance();
	bool loadWindow();

	Rc<XcbLibrary> _xcb;
	Rc<XkbLibrary> _xkb;
	Rc<dbus::Library> _dbus;

	Rc<XcbConnection> _xcbConnection;
	Rc<event::PollHandle> _xcbPollHandle;

	Rc<dbus::Connection> _sessionBus;
	Rc<dbus::Connection> _systemBus;
	Rc<dbus::BusFilter> _networkConnectionFilter;

	NetworkState _networkState;

	Vector<ContextNativeWindow *> _resizedWindows;
	Vector<ContextNativeWindow *> _closedWindows;
};

} // namespace stappler::xenolith::platform

#endif /* XENOLITH_APPLICATION_LINUX_XLLINUXCONTEXTCONTROLLER_H_ */
