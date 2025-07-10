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

#include "XLLinuxContextController.h"
#include "XLContext.h"
#include "XLCoreInstance.h"
#include "SPEventLooper.h"
#include "SPEventPollHandle.h"
#include "SPEventTimerHandle.h"

#if MODULE_XENOLITH_BACKEND_VK_GUI
#include "XLVkInstance.h"
#include "XLVkView.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

#if MODULE_XENOLITH_BACKEND_VK_GUI
static vk::SurfaceBackendMask checkPresentationSupport(LinuxContextController *c,
		const vk::Instance *instance, VkPhysicalDevice device, uint32_t queueIdx) {

	vk::SurfaceBackendMask ret;
#if XL_ENABLE_WAYLAND
	if ((instanceData->surfaceType & SurfaceType::Wayland) != SurfaceType::None) {
		auto display =
				xenolith::platform::WaylandLibrary::getInstance()->getActiveConnection().display;
		std::cout << "Check if " << (void *)device << " [" << queueIdx << "] supports wayland on "
				  << (void *)display << ": ";
		auto supports = instance->vkGetPhysicalDeviceWaylandPresentationSupportKHR(device, queueIdx,
				display);
		if (supports) {
			ret |= toInt(SurfaceType::Wayland);
			std::cout << "yes\n";
		} else {
			std::cout << "no\n";
		}
	}
#endif
	if (instance->getSurfaceBackends().test(toInt(vk::SurfaceBackend::Xcb))) {
		if (auto conn = c->getXcbConnection()) {
			auto xcbConn = conn->getConnection();
			auto screen = conn->getDefaultScreen();
			auto supports = instance->vkGetPhysicalDeviceXcbPresentationSupportKHR(device, queueIdx,
					xcbConn, screen->root_visual);
			if (supports) {
				ret.set(toInt(vk::SurfaceBackend::Xcb));
			}
		}
	}
	return ret;
}
#endif

void LinuxContextController::acquireDefaultConfig(ContextConfig &config,
		NativeContextHandle *handle) {
	if (config.instance->api == core::InstanceApi::None) {
		config.instance->api = core::InstanceApi::Vulkan;
	}

	if (config.loop) {
		config.loop->defaultFormat = core::ImageFormat::B8G8R8A8_UNORM;
	}
}

bool LinuxContextController::init(NotNull<Context> ctx, ContextConfig &&config) {
	if (!ContextController::init(ctx)) {
		return false;
	}

	_contextInfo = move(config.context);
	_windowInfo = move(config.window);
	_instanceInfo = move(config.instance);
	_loopInfo = move(config.loop);

	_xcb = Rc<XcbLibrary>::create();
	_xkb = Rc<XkbLibrary>::create();
	_dbus = Rc<dbus::Library>::create();

	_looper = event::Looper::acquire(
			event::LooperInfo{.workersCount = _contextInfo->mainThreadsCount});

	return true;
}

int LinuxContextController::run() {
	_context->handleConfigurationChanged(move(_contextInfo));

	_contextInfo = nullptr;

	_sessionBus = Rc<dbus::Connection>::alloc(_dbus,
			[this](dbus::Connection *c, const dbus::Event &ev) -> dbus_bool_t {
		return handleDbusEvent(c, ev);
	}, DBUS_BUS_SESSION);

	_systemBus = Rc<dbus::Connection>::alloc(_dbus,
			[this](dbus::Connection *c, const dbus::Event &ev) -> dbus_bool_t {
		return handleDbusEvent(c, ev);
	}, DBUS_BUS_SYSTEM);

	_looper->performOnThread([this] {
		_sessionBus->dispatchAll();
		_systemBus->dispatchAll();

		_sessionBus->setup();
		_systemBus->setup();

		if (_xcb && _xkb) {
			_xcbConnection = Rc<XcbConnection>::create(_xcb, _xkb);
		}

		if (_xcbConnection) {
			_xcbPollHandle = _looper->listenPollableHandle(_xcbConnection->getSocket(),
					event::PollFlags::In, [this](int fd, event::PollFlags) {
				_xcbConnection->poll();

				for (auto &it : _resizedWindows) { _context->handleNativeWindowRedrawNeeded(it); }

				return Status::Ok;
			}, this);
		}
	}, this);

	_looper->run();

	return ContextController::run();
}

void LinuxContextController::notifyWindowResized(NotNull<ContextNativeWindow> w) {
	_resizedWindows.emplace_back(w);
}

bool LinuxContextController::notifyWindowClosed(NotNull<ContextNativeWindow> w) {
	_resizedWindows.emplace_back(w);
	return false;
}

Rc<AppWindow> LinuxContextController::makeAppWindow(NotNull<AppThread> app,
		NotNull<ContextNativeWindow> w) {
	return Rc<AppWindow>::create(_context, app, w);
}

dbus_bool_t LinuxContextController::handleDbusEvent(dbus::Connection *c, const dbus::Event &ev) {
	switch (ev.type) {
	case dbus::Event::None: break;
	case dbus::Event::AddWatch: {
		auto handle = _looper->listenPollableHandle(_dbus->dbus_watch_get_unix_fd(ev.watch),
				dbus::getPollFlags(_dbus->dbus_watch_get_flags(ev.watch)),
				event::CompletionHandle<event::PollHandle>::create<DBusWatch>(ev.watch,
						[](DBusWatch *watch, event::PollHandle *handle, uint32_t flags,
								Status status) {
			if (status::isErrno(status)) {
				return;
			}

			auto c = static_cast<dbus::Connection *>(handle->getUserdata());
			if (!c->handle(handle, dbus::Event{dbus::Event::TriggerWatch, {watch}},
						event::PollFlags(flags))) {
				handle->cancel();
			}
		}),
				c);

		handle->retain(0);
		_dbus->dbus_watch_set_data(ev.watch, handle.get(),
				[](void *ptr) { reinterpret_cast<event::PollHandle *>(ptr)->release(0); });

		if (!_dbus->dbus_watch_get_enabled(ev.watch)) {
			handle->pause();
		}

		return 1;
		break;
	}
	case dbus::Event::ToggleWatch: {
		auto handle = reinterpret_cast<event::PollHandle *>(_dbus->dbus_watch_get_data(ev.watch));
		if (!_dbus->dbus_watch_get_enabled(ev.watch)) {
			if (handle->getStatus() != Status::Declined) {
				handle->pause();
			}
		} else {
			if (handle->getStatus() == Status::Declined) {
				handle->reset(dbus::getPollFlags(_dbus->dbus_watch_get_flags(ev.watch)));
				handle->resume();
			}
		}
		return 1;
		break;
	}
	case dbus::Event::RemoveWatch: {
		auto handle = reinterpret_cast<event::PollHandle *>(_dbus->dbus_watch_get_data(ev.watch));
		handle->cancel(Status::Done);
		return 1;
		break;
	}
	case dbus::Event::TriggerWatch: break;
	case dbus::Event::AddTimeout: {
		if (auto d = _dbus->dbus_timeout_get_data(ev.timeout)) {
			auto handle = reinterpret_cast<event::TimerHandle *>(d);
			handle->reset(event::TimerInfo{
				.timeout = TimeInterval::milliseconds(_dbus->dbus_timeout_get_interval(ev.timeout)),
				.count = 1,
			});
			return 1;
		}
		auto handle = _looper->scheduleTimer(
				event::TimerInfo{
					.completion = event::CompletionHandle<event::TimerHandle>::create<DBusTimeout>(
							ev.timeout,
							[](DBusTimeout *timeout, event::TimerHandle *handle, uint32_t flags,
									Status status) {
			if (status::isErrno(status)) {
				return;
			}

			auto c = static_cast<dbus::Connection *>(handle->getUserdata());
			if (!c->handle(handle, dbus::Event{dbus::Event::TriggerTimeout, {.timeout = timeout}},
						event::PollFlags(flags))) {
				handle->cancel();
			} else if (c->lib->dbus_timeout_get_enabled(timeout)) {
				auto ival = TimeInterval::milliseconds(c->lib->dbus_timeout_get_interval(timeout));
				if (ival) {
					handle->reset(event::TimerInfo{
						.timeout = TimeInterval::milliseconds(
								c->lib->dbus_timeout_get_interval(timeout)),
						.count = 1,
					});
				}
			}
		}),
					.timeout = TimeInterval::milliseconds(
							_dbus->dbus_timeout_get_interval(ev.timeout)),
					.count = 1,
				},
				c);
		handle->retain(0);
		_dbus->dbus_timeout_set_data(ev.timeout, handle.get(),
				[](void *ptr) { reinterpret_cast<event::PollHandle *>(ptr)->release(0); });

		if (!_dbus->dbus_timeout_get_enabled(ev.timeout)) {
			handle->pause();
		}
		return 1;
		break;
	}
	case dbus::Event::ToggleTimeout: {
		auto handle =
				reinterpret_cast<event::TimerHandle *>(_dbus->dbus_timeout_get_data(ev.timeout));
		if (!_dbus->dbus_timeout_get_enabled(ev.timeout)) {
			if (handle->getStatus() != Status::Declined) {
				handle->pause();
			}
		} else {
			if (handle->getStatus() == Status::Declined) {
				handle->reset(event::TimerInfo{
					.timeout = TimeInterval::milliseconds(
							_dbus->dbus_timeout_get_interval(ev.timeout)),
					.count = 1,
				});
				handle->resume();
			}
		}
		return 1;
		break;
	}
	case dbus::Event::RemoveTimeout: {
		auto handle =
				reinterpret_cast<event::TimerHandle *>(_dbus->dbus_timeout_get_data(ev.timeout));
		handle->cancel(Status::ErrorCancelled);
		return 1;
		break;
	}
	case dbus::Event::TriggerTimeout: break;
	case dbus::Event::Dispatch: _looper->performOnThread([c]() { c->dispatchAll(); }, c); break;
	case dbus::Event::Wakeup: _looper->performOnThread([c]() { c->flush(); }, c); break;
	case dbus::Event::Connected:
		if (c == _systemBus) {
			if (c->services.find(NM_SERVICE_NAME) != c->services.end()) {
				_networkConnectionFilter =
						Rc<dbus::BusFilter>::alloc(c, NM_SERVICE_CONNECTION_FILTER);
				//_networkVpnFilter = Rc<dbus::BusFilter>::alloc(c, NM_SERVICE_VPN_FILTER);

				updateNetworkState();
			}
		}
		tryStart();
		return 1;
		break;
	case dbus::Event::Message: {
		//dbus::describe(_dbus, ev.message, [](StringView str) { std::cout << str; });
		if (_dbus->dbus_message_is_signal(ev.message, NM_SERVICE_CONNECTION_NAME, "StateChanged")) {
			//std::cout << "Connection: StateChanged\n";
			updateNetworkState();
			return DBUS_HANDLER_RESULT_HANDLED;
		}
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		break;
	}
	}

	return 0;
}

void LinuxContextController::tryStart() {
	if (_sessionBus->connected && _systemBus->connected && _xcbConnection) {
		if (!loadInstance()) {
			log::error("LinuxContextController", "Fail to load gAPI instance");
			return;
		}

		if (!resume()) {
			log::error("LinuxContextController", "Fail to resume Context");
		}

		loadWindow();
	}
}

void LinuxContextController::updateNetworkState() {
	_systemBus->callMethod(NM_SERVICE_NAME, NM_SERVICE_PATH, "org.freedesktop.DBus.Properties",
			"GetAll", [this](DBusMessage *msg) {
		_dbus->dbus_message_append_args(msg, DBUS_TYPE_STRING, &NM_SERVICE_NAME, DBUS_TYPE_INVALID);
	}, [this](NotNull<dbus::Connection> c, DBusMessage *reply) {
		_networkState = NetworkState(_dbus, reply);


		//describe(c->lib, reply, [](StringView str) { std::cout << str; });

		std::cout << "NetworkState: ";
		_networkState.description([](StringView str) { std::cout << str; });
		std::cout << "\n";

		// Do we need to check active connection's props?
		/*_systemBus->callMethod(NM_SERVICE_NAME,
				"/org/freedesktop/NetworkManager/ActiveConnection/66",
				"org.freedesktop.DBus.Properties", "GetAll", [this](DBusMessage *msg) {
			_dbus->dbus_message_append_args(msg, DBUS_TYPE_STRING, &NM_SERVICE_CONNECTION_NAME,
					DBUS_TYPE_INVALID);
		}, [this](NotNull<dbus::Connection> c, DBusMessage *reply) {
			std::cout << "Connection: \n";
			describe(c->lib, reply, [](StringView str) { std::cout << str; });
		}, this);*/
	}, this);
}

bool LinuxContextController::loadInstance() {
	if (!_xcbConnection) {
		return false;
	}

#if MODULE_XENOLITH_BACKEND_VK_GUI
	auto instanceInfo = move(_instanceInfo);
	_instanceInfo = nullptr;

	auto instanceBackendInfo = Rc<vk::InstanceBackendInfo>::create();
	instanceBackendInfo->setup = [this](vk::InstanceData &data, const vk::InstanceInfo &info) {
		auto ctxInfo = _context->getInfo();

		if (_xcbConnection) {
			if (info.availableBackends.test(toInt(vk::SurfaceBackend::Xcb))) {
				data.enableBackends.set(toInt(vk::SurfaceBackend::Xcb));
			}
		}
#if XL_ENABLE_WAYLAND
		if (info.availableBackends.test(toInt(vk::SurfaceBackend::Wayland))) {
			data.enableBackends.set(toInt(vk::SurfaceBackend::Wayland));
		}
#endif

		data.applicationName = ctxInfo->appName;
		data.applicationVersion = ctxInfo->appVersion;
		data.checkPresentationSupport = [this](const vk::Instance *inst, VkPhysicalDevice device,
												uint32_t queueIdx) {
			return checkPresentationSupport(this, inst, device, queueIdx);
		};
		return true;
	};

	instanceInfo->backend = move(instanceBackendInfo);

	auto instance = core::Instance::create(move(instanceInfo));
	if (instance) {
		if (auto loop = makeLoop(instance)) {
			_context->handleGraphicsLoaded(loop);
			return true;
		}
	}
#else
	log::error("LinuxContextController", "No available GAPI backends found");
#endif
	return false;
}

bool LinuxContextController::loadWindow() {
	if (_xcbConnection) { }

	return false;
}

} // namespace stappler::xenolith::platform
