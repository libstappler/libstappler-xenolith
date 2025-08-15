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
	if (_appWindow) {
		_appWindow = nullptr;
	}
}

bool NativeWindow::init(NotNull<ContextController> c, Rc<WindowInfo> &&info,
		WindowCapabilities caps, bool isRootWindow) {
	_controller = c;
	_isRootWindow = isRootWindow;
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
	return true;
}

void NativeWindow::handleLayerUpdate(const WindowLayer &layer) { _currentLayer = layer; }

void NativeWindow::acquireTextInput(const TextInputRequest &req) { _textInput->run(req); }

void NativeWindow::releaseTextInput() { _textInput->cancel(); }

bool NativeWindow::findLayers(Vec2 pt, const Callback<bool(const WindowLayer &)> &cb) const {
	bool found = false;
	for (auto &it : _layers) {
		if (it.rect.containsPoint(pt)) {
			if (!cb(it)) {
				return found;
			}
		}
	}
	return found;
}

void NativeWindow::setAppWindow(Rc<AppWindow> &&w) {
	_appWindow = move(w);
	_appWindow->run();
}

AppWindow *NativeWindow::getAppWindow() const { return _appWindow; }

void NativeWindow::updateLayers(Vector<WindowLayer> &&layers) {
	_layers = sp::move(layers);
	if (_handleLayerForMotion) {
		handleMotionEvent(InputEventData{0, InputEventName::MouseMove, core::InputMouseButton::None,
			core::InputModifier::None, _layerLocation.x, _layerLocation.y});
	}
}

void NativeWindow::setFullscreen(FullscreenInfo &&info, Function<void(Status)> &&cb, Ref *ref) {
	if (!hasFlag(_info->capabilities, WindowCapabilities::Fullscreen)) {
		cb(Status::ErrorNotImplemented);
		return;
	}

	auto dcm = _controller->getDisplayConfigManager();

	if (info == FullscreenInfo::None) {
		// restore saved mode
		dcm->restoreMode(nullptr, this);

		// remove fullscreen state
		if (hasFlag(_state, NativeWindowStateFlags::Fullscreen)) {
			cb(setFullscreenState(sp::move(info)));
		} else {
			// not in fullsreen
			cb(Status::Declined);
		}
	} else if (info == FullscreenInfo::Current) {
		if (!hasFlag(_state, NativeWindowStateFlags::Fullscreen)) {
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

		if (hasFlag(_state, NativeWindowStateFlags::Fullscreen)) {
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
						dcm->restoreMode(
								[this, cb = sp::move(cb), info = sp::move(info),
										ref = Rc<Ref>(ref)](Status st) mutable {
							if (st == Status::Ok) {
								cb(setFullscreenState(sp::move(info)));
							} else {
								log::error("XcbWindow", "Fail to reset mode for fullscreen: ", st);
								cb(st);
							}
							ref = nullptr;
						},
								this);
						return;
					}
				} else {
					// requested fullscreen to another monitor with custom mode

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
					// exit from fullscreen before mode switch
					setFullscreenState(FullscreenInfo(FullscreenInfo::None));
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

		// set new mode for monitor
		dcm->setModeExclusive(mon->id, m->mode,
				[this, cb = sp::move(cb), info = sp::move(info), ref = Rc<Ref>(ref)](
						Status st) mutable {
			if (st == Status::Ok) {
				cb(setFullscreenState(sp::move(info)));
			} else {
				log::error("XcbWindow", "Fail to set mode for fullscreen: ", st);
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

void NativeWindow::handleMotionEvent(const InputEventData &event) {
	if (_handleLayerForMotion) {
		_layerLocation = Vec2(event.x, event.y);
		if (!findLayers(_layerLocation, [this](const WindowLayer &layer) {
			if (layer != _currentLayer) {
				handleLayerUpdate(layer);
			}
			return false;
		})) {
			if (WindowLayer() != _currentLayer) {
				handleLayerUpdate(WindowLayer());
			}
		}
	}
}

const CallbackStream &operator<<(const CallbackStream &out, NativeWindowStateFlags flags) {
	if (hasFlag(flags, NativeWindowStateFlags::Modal)) {
		out << " Modal";
	}
	if (hasFlag(flags, NativeWindowStateFlags::Sticky)) {
		out << " Sticky";
	}
	if (hasFlag(flags, NativeWindowStateFlags::MaximizedVert)) {
		out << " MaximizedVert";
	}
	if (hasFlag(flags, NativeWindowStateFlags::MaximizedHorz)) {
		out << " MaximizedHorz";
	}
	if (hasFlag(flags, NativeWindowStateFlags::Shaded)) {
		out << " Shaded";
	}
	if (hasFlag(flags, NativeWindowStateFlags::SkipTaskbar)) {
		out << " SkipTaskbar";
	}
	if (hasFlag(flags, NativeWindowStateFlags::SkipPager)) {
		out << " SkipPager";
	}
	if (hasFlag(flags, NativeWindowStateFlags::Hidden)) {
		out << " Hidden";
	}
	if (hasFlag(flags, NativeWindowStateFlags::Fullscreen)) {
		out << " Fullscreen";
	}
	if (hasFlag(flags, NativeWindowStateFlags::Above)) {
		out << " Above";
	}
	if (hasFlag(flags, NativeWindowStateFlags::Below)) {
		out << " Below";
	}
	if (hasFlag(flags, NativeWindowStateFlags::DemandsAttention)) {
		out << " DemandsAttention";
	}
	if (hasFlag(flags, NativeWindowStateFlags::Focused)) {
		out << " Focused";
	}
	return out;
}

} // namespace stappler::xenolith::platform
