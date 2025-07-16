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

#include "XLContextController.h"
#include "XLContextNativeWindow.h"
#include "XLContext.h"

#if LINUX
#include "linux/XLLinuxContextController.h"
#endif

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkInstance.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

Rc<ContextController> ContextController::create(NotNull<Context> ctx, ContextConfig &&info) {
#if LINUX
	return Rc<LinuxContextController>::create(ctx, move(info));
#endif
}

void ContextController::acquireDefaultConfig(ContextConfig &config, NativeContextHandle *handle) {
#if LINUX
	LinuxContextController::acquireDefaultConfig(config, handle);
#endif
}

bool ContextController::init(NotNull<Context> ctx) {
	_context = ctx;
	return true;
}

int ContextController::run() { return _resultCode; }

void ContextController::notifyWindowResized(NotNull<ContextNativeWindow> w, bool liveResize) {
	_context->handleNativeWindowResized(w, liveResize);
}
void ContextController::notifyWindowInputEvents(NotNull<ContextNativeWindow> w,
		Vector<core::InputEventData> &&ev) {
	_context->handleNativeWindowInputEvents(w, sp::move(ev));
}

void ContextController::notifyWindowTextInput(NotNull<ContextNativeWindow> w,
		const core::TextInputState &state) {
	_context->handleNativeWindowTextInput(w, state);
}

bool ContextController::notifyWindowClosed(NotNull<ContextNativeWindow> w) {
	if (w->hasExitGuard()) {
		auto event = core::InputEventData::BoolEvent(core::InputEventName::CloseRequest, true);
		notifyWindowInputEvents(w, Vector<core::InputEventData>{event});
		return false;
	} else {
		_context->handleNativeWindowDestroyed(w);
		w->unmapWindow();
		return true;
	}
}

void ContextController::addNetworkCallback(Ref *key, Function<void(NetworkFlags)> &&cb) {
	auto it = _networkCallbacks.find(key);
	if (it != _networkCallbacks.end()) {
		it->second = sp::move(cb);
	} else {
		it = _networkCallbacks.emplace(key, sp::move(cb)).first;
	}

	it->second(_networkFlags);
}

void ContextController::removeNetworkCallback(Ref *key) { _networkCallbacks.erase(key); }

Rc<AppWindow> ContextController::makeAppWindow(NotNull<AppThread>, NotNull<ContextNativeWindow>) {
	return nullptr;
}

void ContextController::initializeComponent(NotNull<ContextComponent> comp) {
	switch (_state) {
	case ContextState::None: break;
	case ContextState::Created: break;
	case ContextState::Started: comp->handleStart(_context); break;
	case ContextState::Active: comp->handleResume(_context); break;
	}
}

Status ContextController::readFromClipboard(Rc<ClipboardRequest> &&) {
	return Status::ErrorNotImplemented;
}

Status ContextController::writeToClipboard(BytesView, StringView contentType) {
	return Status::ErrorNotImplemented;
}

Rc<ScreenInfo> ContextController::getScreenInfo() const { return Rc<ScreenInfo>::create(); }

void ContextController::handleStateChanged(ContextState prevState, ContextState newState) {
	if (prevState == newState) {
		return;
	}

	auto refId = retain();

	switch (newState) {
	case ContextState::None:
		handleContextWillDestroy();
		_networkCallbacks.clear();
		_state = newState;
		_networkFlags = NetworkFlags::None;

		_contextInfo = nullptr;
		_windowInfo = nullptr;
		_instanceInfo = nullptr;
		_loopInfo = nullptr;

		handleContextDidDestroy();
		_context = nullptr;
		_looper = nullptr;
		break;
	case ContextState::Created:
		if (prevState > newState) {
			handleContextWillStop();
			_state = newState;
			handleContextDidStop();
		} else {
			// should not happen - context controller should be created in this state
		}
		break;
	case ContextState::Started:
		if (prevState > newState) {
			handleContextWillPause();
			_state = newState;
			handleContextDidPause();
		} else {
			handleContextWillStart();
			_state = newState;
			handleContextDidStart();
		}
		break;
	case ContextState::Active:
		handleContextWillResume();
		_state = newState;
		handleContextDidResume();
		break;
	}

	release(refId);
}

void ContextController::handleContextWillDestroy() { _context->handleWillDestroy(); }
void ContextController::handleContextDidDestroy() { _context->handleDidDestroy(); }

void ContextController::handleContextWillStop() { _context->handleWillStop(); }
void ContextController::handleContextDidStop() { _context->handleDidStop(); }

void ContextController::handleContextWillPause() { _context->handleWillPause(); }
void ContextController::handleContextDidPause() { _context->handleDidPause(); }

void ContextController::handleContextWillResume() { _context->handleWillResume(); }
void ContextController::handleContextDidResume() { _context->handleDidResume(); }

void ContextController::handleContextWillStart() { _context->handleWillStart(); }
void ContextController::handleContextDidStart() { _context->handleDidStart(); }

bool ContextController::start() {
	switch (_state) {
	case ContextState::Created:
		handleStateChanged(_state, ContextState::Started);
		return true;
		break;
	case ContextState::None:
	case ContextState::Started:
	case ContextState::Active: break;
	}
	return false;
}

bool ContextController::resume() {
	switch (_state) {
	case ContextState::Created:
		if (start()) {
			handleStateChanged(_state, ContextState::Active);
			return true;
		}
		break;
	case ContextState::Started:
		handleStateChanged(_state, ContextState::Active);
		return true;
		break;
	case ContextState::None:
	case ContextState::Active: break;
	}
	return false;
}

bool ContextController::pause() {
	switch (_state) {
	case ContextState::Active:
		handleStateChanged(_state, ContextState::Started);
		return true;
		break;
	case ContextState::None:
	case ContextState::Started:
	case ContextState::Created: break;
	}
	return false;
}

bool ContextController::stop() {
	switch (_state) {
	case ContextState::Started:
		handleStateChanged(_state, ContextState::Created);
		return true;
		break;
	case ContextState::Active:
		if (pause()) {
			handleStateChanged(_state, ContextState::Created);
			return true;
		}
		break;
	case ContextState::None:
	case ContextState::Created: break;
	}
	return false;
}

bool ContextController::destroy() {
	if (_state == ContextState::None) {
		return false;
	}

	switch (_state) {
	case ContextState::Active:
		if (pause() && stop()) {
			handleStateChanged(_state, ContextState::None);
		}
		break;
	case ContextState::Started:
		if (stop()) {
			handleStateChanged(_state, ContextState::None);
		}
		break;
	case ContextState::Created: handleStateChanged(_state, ContextState::None); break;
	case ContextState::None: break;
	}

	return true;
}

Value ContextController::saveContextState() { return _context->saveState(); }

Rc<core::Loop> ContextController::makeLoop(NotNull<core::Instance> instance) {
#if MODULE_XENOLITH_BACKEND_VK
	if (instance->getApi() == core::InstanceApi::Vulkan) {
		if (!_loopInfo->backend) {
			auto isHeadless = hasFlag(_context->getInfo()->flags, ContextFlags::Headless);
			auto data = Rc<vk::LoopBackendInfo>::alloc();
			data->deviceSupportCallback = [isHeadless](const vk::DeviceInfo &dev) {
				if (isHeadless) {
					return true;
				} else {
					return dev.supportsPresentation()
							&& std::find(dev.availableExtensions.begin(),
									   dev.availableExtensions.end(),
									   String(VK_KHR_SWAPCHAIN_EXTENSION_NAME))
							!= dev.availableExtensions.end();
				}
			};
			data->deviceExtensionsCallback = [](const vk::DeviceInfo &dev) -> Vector<StringView> {
				Vector<StringView> ret;
				ret.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
				return ret;
			};
			_loopInfo->backend = data;
		}
	}
#endif

	auto loop = instance->makeLoop(_looper, move(_loopInfo));
	_loopInfo = nullptr;
	return loop;
}

void ContextController::updateNetworkFlags(NetworkFlags flags) {
	_networkFlags = flags;
	for (auto &it : _networkCallbacks) { it.second(flags); }
}

} // namespace stappler::xenolith::platform
