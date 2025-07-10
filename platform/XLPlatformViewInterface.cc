/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#include "XLPlatformViewInterface.h"
#include "XLCoreInput.h"
#include "XLPlatformApplication.h"
#include "XLCoreLoop.h"
#include "SPEventLooper.h"
#include "XLPlatformTextInputInterface.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

XL_DECLARE_EVENT_CLASS(BasicWindow, onBackground);
XL_DECLARE_EVENT_CLASS(BasicWindow, onFocus);

bool BasicWindow::init(PlatformApplication &app, core::Loop &loop) {
	_application = &app;
	_loop = &loop;
	_textInput = Rc<TextInputInterface>::create(this);
	return true;
}

void BasicWindow::update(bool displayLink) { _presentationEngine->update(displayLink); }

void BasicWindow::handleInputEvent(const InputEventData &event) {
	if (!_presentationEngine) {
		return;
	}

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
				InputEventData ev = event;
				ev.event = InputEventName::KeyCanceled; // force-cancel processed key

				_application->performOnAppThread([this, ev]() mutable { propagateInputEvent(ev); },
						this);
				return;
			}
		}
		_application->performOnAppThread(
				[this, event = event]() mutable { propagateInputEvent(event); }, this);
		break;
	default:
		_application->performOnAppThread(
				[this, event = event]() mutable { propagateInputEvent(event); }, this);
		break;
	}

	setReadyForNextFrame();
}

void BasicWindow::handleInputEvents(Vector<InputEventData> &&events) {
	if (!_presentationEngine) {
		return;
	}

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

	_application->performOnAppThread([this, events = sp::move(events)]() mutable {
		for (auto &event : events) { propagateInputEvent(event); }
	}, this, true);
	setReadyForNextFrame();
}

const ViewLayer *BasicWindow::getTopLayer(Vec2 vec) const {
	auto it = _layers.crbegin();
	while (it != _layers.crend()) {
		if (it->rect.containsPoint(vec)) {
			return &(*it);
		}
		++it;
	}
	return nullptr;
}

const ViewLayer *BasicWindow::getBottomLayer(Vec2 vec) const {
	for (auto &it : _layers) {
		if (it.rect.containsPoint(vec)) {
			return &it;
		}
	}
	return nullptr;
}

bool BasicWindow::isOnThisThread() const { return _loop->isOnThisThread(); }

void BasicWindow::performOnThread(Function<void()> &&func, Ref *target, bool immediate,
		StringView tag) {
	if (immediate && isOnThisThread()) {
		func();
	} else {
		_loop->getLooper()->performOnThread(sp::move(func), target, immediate, tag);
	}
}

void BasicWindow::updateConfig() {
	_loop->performOnThread([this] { _presentationEngine->deprecateSwapchain(); }, this, true);
}

void BasicWindow::setReadyForNextFrame() {
	_loop->performOnThread([this] {
		if (_presentationEngine) {
			_presentationEngine->setReadyForNextFrame();
		}
	}, this, true);
}

void BasicWindow::setRenderOnDemand(bool value) {
	_loop->performOnThread([this, value] {
		if (_presentationEngine) {
			_presentationEngine->setRenderOnDemand(value);
		}
	}, this, true);
}

bool BasicWindow::isRenderOnDemand() const {
	return _presentationEngine ? _presentationEngine->isRenderOnDemand() : false;
}

void BasicWindow::setFrameInterval(uint64_t value) {
	_loop->performOnThread([this, value] {
		if (_presentationEngine) {
			_presentationEngine->setTargetFrameInterval(value);
		}
	}, this, true);
}

uint64_t BasicWindow::getFrameInterval() const {
	return _presentationEngine ? _presentationEngine->getTargetFrameInterval() : 0;
}

void BasicWindow::setContentPadding(const Padding &padding) {
	_loop->performOnThread([this, padding] { _presentationEngine->setContentPadding(padding); },
			this, true);
}

void BasicWindow::waitUntilFrame() {
	if (_presentationEngine) {
		_presentationEngine->waitUntilFramePresentation();
	}
}

void BasicWindow::acquireTextInput(TextInputRequest &&req) {
	performOnThread([this, data = move(req)]() { _textInput->run(data); }, this);
}

void BasicWindow::releaseTextInput() {
	performOnThread([this]() { _textInput->cancel(); }, this);
}

void BasicWindow::updateLayers(Vector<ViewLayer> &&layers) {
	performOnThread([this, layers = sp::move(layers)]() mutable { handleLayers(sp::move(layers)); },
			this);
}

void BasicWindow::propagateInputEvent(core::InputEventData &event) {
	if (event.isPointEvent()) {
		event.point.density = _presentationEngine
				? _presentationEngine->getFrameConstraints().density
				: getWindowInfo().density;
	}

	switch (event.event) {
	case InputEventName::Background:
		_inBackground = event.getValue();
		onBackground(this, _inBackground);
		break;
	case InputEventName::PointerEnter: _pointerInWindow = event.getValue(); break;
	case InputEventName::FocusGain:
		_hasFocus = event.getValue();
		onFocus(this, _hasFocus);
		break;
	default: break;
	}
}

void BasicWindow::propagateTextInput(TextInputState &state) { }

void BasicWindow::handleLayers(Vector<ViewLayer> &&layers) { _layers = sp::move(layers); }

void BasicWindow::handleMotionEvent(const core::InputEventData &event) {
	if (_handleLayerForMotion) {
		auto l = getTopLayer(Vec2(event.x, event.y));
		if (l) {
			if (*l != _currentLayer) {
				handleLayerUpdate(*l);
			}
		} else if (ViewLayer() != _currentLayer) {
			handleLayerUpdate(ViewLayer());
		}
	}
}

void BasicWindow::handleLayerUpdate(const ViewLayer &layer) { _currentLayer = layer; }

} // namespace stappler::xenolith::platform
