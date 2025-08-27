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

#include "XLContextNativeWindow.h"
#include "SPCore.h"
#include "SPStatus.h"
#include "XLContextController.h"
#include "XLAppWindow.h"
#include "XlCoreMonitorInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

NativeWindow::~NativeWindow() {
	if (_controller && _allocated) {
		_controller->notifyWindowDeallocated(this);
	}
	if (_appWindow) {
		_appWindow = nullptr;
	}
}

bool NativeWindow::init(NotNull<ContextController> c, Rc<WindowInfo> &&info,
		WindowCapabilities caps) {
	_controller = c;
	_info = move(info);
	_info->capabilities = caps;
	_textInput = Rc<TextInputProcessor>::create(core::TextInputInfo{
		.update = [this](const TextInputRequest &req) -> bool { return updateTextInput(req); },
		.propagate =
				[this](const TextInputState &state) {
		_controller->notifyWindowTextInput(this, state);
	},
		.cancel = [this]() { cancelTextInput(); },
	});

	_controller->notifyWindowAllocated(this);
	_allocated = true;

	return true;
}

core::FrameConstraints NativeWindow::exportConstraints(core::FrameConstraints &&c) const {
	if (hasFlag(_info->state, WindowState::Fullscreen)) {
		c.borderRadius = 0.0f;
		c.shadowRadius = 0.0f;
		c.shadowValue = 0.0f;
		c.shadowOffset = Vec2(0.0f, 0.0f);
	} else {
		c.borderRadius = _info->userDecorations.borderRadius;
		c.shadowRadius = _info->userDecorations.shadowWidth;
		c.shadowValue = _info->userDecorations.shadowCurrentValue;
		c.shadowOffset = _info->userDecorations.shadowOffset;
		c.viewConstraints = core::getViewConstraints(_info->state);
	}
	return move(c);
}

void NativeWindow::acquireTextInput(const TextInputRequest &req) { _textInput->run(req); }

void NativeWindow::releaseTextInput() { _textInput->cancel(); }

void NativeWindow::setAppWindow(Rc<AppWindow> &&w) {
	_appWindow = move(w);
	_appWindow->run();
}

AppWindow *NativeWindow::getAppWindow() const { return _appWindow; }

void NativeWindow::updateLayers(Vector<WindowLayer> &&layers) {
	if (_layers != layers) {
		_layers = sp::move(layers);
		if (_handleLayerForMotion) {
			handleMotionEvent(InputEventData{
				0,
				InputEventName::MouseMove,
				{{
					core::InputMouseButton::None,
					core::InputModifier::None,
					_layerLocation.x,
					_layerLocation.y,
				}},
			});
		}
	}
}

void NativeWindow::setFullscreen(FullscreenInfo &&info, Function<void(Status)> &&cb, Ref *ref) {
	if (!hasFlag(_info->capabilities, WindowCapabilities::Fullscreen)) {
		cb(Status::ErrorNotSupported);
		return;
	}

	if (_hasPendingFullscreenOp) {
		cb(Status::ErrorAgain);
		return;
	}

	auto hasModeSetting = false;
	auto hasSeamlessModeSetting = false;
	if (hasFlag(_info->capabilities, WindowCapabilities::FullscreenWithMode)) {
		hasModeSetting = true;
	}

	if (hasFlag(_info->capabilities, WindowCapabilities::FullscreenSeamlessModeSwitch)) {
		hasSeamlessModeSetting = true;
	}

	auto dcm = _controller->getDisplayConfigManager();

	if (info == FullscreenInfo::None) {
		// restore saved mode
		dcm->restoreMode(nullptr, this);

		// remove fullscreen state
		if (hasFlag(_info->state, WindowState::Fullscreen)) {
			cb(setFullscreenState(sp::move(info)));
		} else {
			// not in fullsreen
			cb(Status::Declined);
		}
	} else if (info == FullscreenInfo::Current) {
		if (!hasFlag(_info->state, WindowState::Fullscreen)) {
			cb(setFullscreenState(sp::move(info)));
		} else {
			cb(Status::Declined);
		}
	} else {
		auto config = dcm->getCurrentConfig();

		auto mon = config->getMonitor(info.id);
		if (!mon) {
			cb(Status::ErrorInvalidArguemnt);
			return;
		}

		auto m = mon->getMode(info.mode);
		auto &current = mon->getCurrent();
		if (!m) {
			cb(Status::ErrorInvalidArguemnt);
			return;
		}

		// update info with concrete parameters
		info.id = mon->id;
		info.mode = m->mode;

		if (hasFlag(_info->state, WindowState::Fullscreen)) {
			// we already in fullscreen mode
			if (_info->fullscreen.id != info.id) {
				// switch monitor

				if (info.mode == core::ModeInfo::Current || info.mode == current) {
					if (!dcm->hasSavedMode()) {
						// no saved mode - just switch to another monitor
						cb(setFullscreenState(sp::move(info)));
						return;
					} else {
						// with fullscreen engine, mode for other monitor should not be other then saved-current
						// so, restore saved mode, then fullscreen window on other monitor
						if (!hasModeSetting) {
							cb(Status::ErrorNotSupported);
							return;
						}
						dcm->restoreMode(
								[this, cb = sp::move(cb), info = sp::move(info),
										ref = Rc<Ref>(ref)](Status st) mutable {
							if (st == Status::Ok) {
								cb(setFullscreenState(sp::move(info)));
							} else {
								log::error("NativeWindow",
										"Fail to reset mode for fullscreen: ", st);
								cb(st);
							}
							ref = nullptr;
						},
								this);
						return;
					}
				} else {
					// requested fullscreen to another monitor with custom mode
					if (!hasModeSetting) {
						cb(Status::ErrorNotSupported);
						return;
					}
					// unset fullscreen, then set on other monitor with `setModeExclusive`
					setFullscreenState(FullscreenInfo(FullscreenInfo::None));
				}
			} else {
				// requested mode-switch for current monitor
				if (info.mode == core::ModeInfo::Current || info.mode == current) {
					// already on mede, decline
					cb(Status::Declined);
					return;
				} else {
					if (!hasModeSetting) {
						cb(Status::ErrorNotSupported);
						return;
					}

					if (!hasSeamlessModeSetting) {
						// exit from fullscreen before mode switch
						setFullscreenState(FullscreenInfo(FullscreenInfo::None));
					}
				}
			}
		} else {
			// not in fullscreen
			// check if requested mode is current
			if (info.mode == core::ModeInfo::Current || info.mode == current) {
				// if it is, - just set fullscreen flag
				cb(setFullscreenState(sp::move(info)));
				return;
			}
			// now - just set mode
		}

		if (!hasModeSetting) {
			cb(Status::ErrorNotSupported);
			return;
		}

		// set new mode for monitor
		dcm->setModeExclusive(mon->id, m->mode,
				[this, cb = sp::move(cb), info = sp::move(info), ref = Rc<Ref>(ref)](
						Status st) mutable {
			if (st == Status::Ok) {
				auto status = setFullscreenState(sp::move(info));
				if (status != Status::Ok && status != Status::Declined) {
					auto dcm = _controller->getDisplayConfigManager();
					dcm->restoreMode(nullptr, nullptr);
					cb(status);
				} else {
					cb(status);
				}
			} else {
				log::error("NativeWindow", "Fail to set mode for fullscreen: ", st);
				cb(st);
			}
			ref = nullptr;
		},
				this);
	}
}

void NativeWindow::handleInputEvents(Vector<InputEventData> &&events) {
	for (auto &event : events) {
		switch (event.event) {
		case InputEventName::MouseMove: handleMotionEvent(event); break;
		case InputEventName::KeyPressed:
		case InputEventName::KeyRepeated:
		case InputEventName::KeyReleased:
		case InputEventName::KeyCanceled:
			if (_handleTextInputFromKeyboard && isTextInputEnabled() && _textInput
					&& _textInput->canHandleInputEvent(event)) {
				// forward to text input
				if (_textInput->handleInputEvent(event)) {
					event.event = InputEventName::KeyCanceled; // force-cancel processed key
				}
			}
			break;
		default: break;
		}
	}

	_controller->notifyWindowInputEvents(this, sp::move(events));
}

void NativeWindow::dispatchPendingEvents() {
	if (!_pendingEvents.empty()) {
		handleInputEvents(sp::move(_pendingEvents));
	}
	_pendingEvents.clear();
}

void NativeWindow::handleMotionEvent(const InputEventData &event) {
	if (_handleLayerForMotion) {
		_layerLocation = event.getLocation();

		auto it = _currentLayers.begin();
		while (it != _currentLayers.end()) {
			if (!it->rect.containsPoint(_layerLocation)) {
				auto l = *it;
				it = _currentLayers.erase(it);
				handleLayerExit(l);
			} else {
				++it;
			}
		}

		for (auto &it : _layers) {
			if (it.rect.containsPoint(_layerLocation)) {
				bool found = false;
				for (auto &cit : _currentLayers) {
					if (cit == it) {
						found = true;
						break;
					}
				}
				if (!found) {
					handleLayerEnter(_currentLayers.emplace_back(it));
				}
			}
		}
	}
}

void NativeWindow::emitAppFrame() {
	if (_appWindow) {
		_appWindow->setReadyForNextFrame();
		_appWindow->update(core::PresentationUpdateFlags::DisplayLink);
	}
}

void NativeWindow::updateState(uint32_t id, WindowState state) {
	auto changes = state ^ _info->state;

	_info->state = state;
	// try to rewrite state in already pending event
	for (auto &it : _pendingEvents) {
		if (it.event == core::InputEventName::WindowState) {
			it.window.state = state;
			it.window.changes |= changes;
			return;
		}
	}

	// add new event
	core::InputEventData event{
		id,
		core::InputEventName::WindowState,
		{.input = {core::InputMouseButton::None, core::InputModifier::None, nan(), nan()}},
		{.window = {state, changes}},
	};

	_pendingEvents.emplace_back(sp::move(event));

	if (!_controller->isWithinPoll()) {
		dispatchPendingEvents();
	}
}

} // namespace stappler::xenolith::platform
