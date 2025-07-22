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
#include "XLContextController.h"
#include "XLAppWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

ContextNativeWindow::~ContextNativeWindow() {
	if (_appWindow) {
		_appWindow = nullptr;
	}
}

bool ContextNativeWindow::init(NotNull<ContextController> c, Rc<WindowInfo> &&info,
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
	return true;
}

void ContextNativeWindow::acquireTextInput(const TextInputRequest &req) { _textInput->run(req); }

void ContextNativeWindow::releaseTextInput() { _textInput->cancel(); }

bool ContextNativeWindow::findLayers(Vec2 pt, const Callback<bool(const WindowLayer &)> &cb) const {
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

void ContextNativeWindow::setAppWindow(Rc<AppWindow> &&w) {
	_appWindow = move(w);
	_appWindow->run();
}

AppWindow *ContextNativeWindow::getAppWindow() const { return _appWindow; }

void ContextNativeWindow::updateLayers(Vector<WindowLayer> &&layers) {
	_layers = sp::move(layers);
	if (_handleLayerForMotion) {
		handleMotionEvent(InputEventData{0, InputEventName::MouseMove, core::InputMouseButton::None,
			core::InputModifier::None, _layerLocation.x, _layerLocation.y});
	}
}

void ContextNativeWindow::handleInputEvents(Vector<InputEventData> &&events) {
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

void ContextNativeWindow::handleMotionEvent(const InputEventData &event) {
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

} // namespace stappler::xenolith::platform
