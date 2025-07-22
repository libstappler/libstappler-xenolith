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

#include "XLInputListener.h"
#include "XLComponent.h"
#include "XLDirector.h"
#include "XLInputDispatcher.h"
#include "XLGestureRecognizer.h"
#include "XLAppWindow.h"
#include "XLScene.h"
#include "XLFrameContext.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

InputListener::EventMask InputListener::EventMaskTouch = InputListener::makeEventMask({
	InputEventName::Begin,
	InputEventName::Move,
	InputEventName::End,
	InputEventName::Cancel,
	InputEventName::Scroll,
});

InputListener::EventMask InputListener::EventMaskKeyboard = InputListener::makeEventMask({
	InputEventName::KeyPressed,
	InputEventName::KeyRepeated,
	InputEventName::KeyReleased,
	InputEventName::KeyCanceled,
});

InputListener::ButtonMask InputListener::makeButtonMask(
		std::initializer_list<InputMouseButton> &&il) {
	ButtonMask ret;
	for (auto &it : il) { ret.set(toInt(it)); }
	return ret;
}

InputListener::EventMask InputListener::makeEventMask(std::initializer_list<InputEventName> &&il) {
	EventMask ret;
	for (auto &it : il) { ret.set(toInt(it)); }
	return ret;
}

InputListener::KeyMask InputListener::makeKeyMask(std::initializer_list<InputKeyCode> &&il) {
	KeyMask ret;
	for (auto &it : il) { ret.set(toInt(it)); }
	return ret;
}

bool InputListener::init(int32_t priority) {
	if (!Component::init()) {
		return false;
	}

	_priority = priority;
	return true;
}

void InputListener::handleEnter(Scene *scene) {
	Component::handleEnter(scene);

	_scene = scene;

	for (auto &it : _recognizers) { it->onEnter(this); }
}

void InputListener::handleExit() {
	for (auto &it : _recognizers) { it->onExit(); }

	_scene = nullptr;

	Component::handleExit();
}

void InputListener::handleVisitSelf(FrameInfo &info, Node *node, NodeFlags flags) {
	Component::handleVisitSelf(info, node, flags);

	if (_enabled) {
		info.input->addListener(this, _dedicatedFocus == 0 ? info.focusValue : _dedicatedFocus,
				_layerFlags,
				_layerFlags != WindowLayerFlags::None
						? TransformRect(Rect(Vec2(0, 0), node->getContentSize()),
								  info.modelTransformStack.back())
						: Rect());
		info.input->updateFocus(info.focusValue);
	}
}

void InputListener::update(const UpdateTime &dt) {
	for (auto &it : _recognizers) { it->update(dt.delta); }
}

void InputListener::setViewLayerFlags(WindowLayerFlags flags) { _layerFlags = flags; }

void InputListener::setOwner(Node *owner) { _owner = owner; }

void InputListener::setPriority(int32_t p) { _priority = p; }

void InputListener::setDedicatedFocus(uint32_t v) { _dedicatedFocus = v; }

void InputListener::setExclusive() {
	if (_scene) {
		_scene->getDirector()->getInputDispatcher()->setListenerExclusive(this);
	}
}

void InputListener::setExclusiveForTouch(uint32_t eventId) {
	if (_scene) {
		_scene->getDirector()->getInputDispatcher()->setListenerExclusiveForTouch(this, eventId);
	}
}

void InputListener::setSwallowEvents(EventMask &&mask) {
	if (_swallowEvents.any()) {
		_swallowEvents |= mask;
	} else {
		_swallowEvents = sp::move(mask);
	}
}

void InputListener::setSwallowEvents(const EventMask &mask) {
	if (_swallowEvents.any()) {
		_swallowEvents |= mask;
	} else {
		_swallowEvents = mask;
	}
}

void InputListener::setSwallowAllEvents() { _swallowEvents.set(); }

void InputListener::setSwallowEvent(InputEventName event) { _swallowEvents.set(toInt(event)); }

void InputListener::clearSwallowAllEvents() { _swallowEvents.reset(); }

void InputListener::clearSwallowEvent(InputEventName event) { _swallowEvents.reset(toInt(event)); }

void InputListener::clearSwallowEvents(const EventMask &event) { _swallowEvents &= ~event; }

bool InputListener::isSwallowAllEvents() const { return _swallowEvents.all(); }

bool InputListener::isSwallowAllEvents(const EventMask &event) const {
	return (_swallowEvents & event) == event;
}

bool InputListener::isSwallowAnyEvents(const EventMask &event) const {
	return (_swallowEvents & event).any();
}

bool InputListener::isSwallowEvent(InputEventName name) const {
	return _swallowEvents.test(toInt(name));
}

void InputListener::setTouchFilter(const EventFilter &filter) { _eventFilter = filter; }

bool InputListener::shouldSwallowEvent(const InputEvent &event) const {
	return _swallowEvents.test(toInt(event.data.event));
}

bool InputListener::canHandleEvent(const InputEvent &event) const {
	if (!_running || !_owner) {
		return false;
	}

	if (_eventMask.test(toInt(event.data.event)) && shouldProcessEvent(event)) {
		auto it = _callbacks.find(event.data.event);
		if (it != _callbacks.end()) {
			return true;
		}
		for (auto &it : _recognizers) {
			if (!_running || !_owner) {
				break;
			}

			if (it->canHandleEvent(event)) {
				return true;
			}
		}
	}
	return false;
}

InputEventState InputListener::handleEvent(const InputEvent &event) {
	InputEventState ret = InputEventState::Declined;
	auto it = _callbacks.find(event.data.event);
	if (it != _callbacks.end()) {
		ret = std::max(it->second(event.data.getValue()) ? InputEventState::Processed
														 : InputEventState::Declined,
				ret);
	}
	for (auto &it : _recognizers) {
		if (!_running || !_owner) {
			break;
		}

		ret = std::max(it->handleInputEvent(event, _owner->getInputDensity()), ret);
	}
	return ret;
}

GestureRecognizer *InputListener::addTouchRecognizer(InputCallback<GestureData> &&cb,
		ButtonMask &&buttonMask) {
	return addRecognizer(Rc<GestureTouchRecognizer>::create(sp::move(cb), sp::move(buttonMask)));
}

GestureRecognizer *InputListener::addTapRecognizer(InputCallback<GestureTap> &&cb,
		ButtonMask &&buttonMask, uint32_t maxTapCount) {
	return addRecognizer(
			Rc<GestureTapRecognizer>::create(sp::move(cb), sp::move(buttonMask), maxTapCount));
}

GestureRecognizer *InputListener::addScrollRecognizer(InputCallback<GestureScroll> &&cb) {
	return addRecognizer(Rc<GestureScrollRecognizer>::create(sp::move(cb)));
}

GestureRecognizer *InputListener::addPressRecognizer(InputCallback<GesturePress> &&cb,
		TimeInterval interval, bool continuous, ButtonMask &&mask) {
	return addRecognizer(
			Rc<GesturePressRecognizer>::create(sp::move(cb), interval, continuous, sp::move(mask)));
}

GestureRecognizer *InputListener::addSwipeRecognizer(InputCallback<GestureSwipe> &&cb,
		float threshold, bool sendThreshold, ButtonMask &&mask) {
	return addRecognizer(Rc<GestureSwipeRecognizer>::create(sp::move(cb), threshold, sendThreshold,
			sp::move(mask)));
}

GestureRecognizer *InputListener::addPinchRecognizer(InputCallback<GesturePinch> &&cb,
		ButtonMask &&mask) {
	return addRecognizer(Rc<GesturePinchRecognizer>::create(sp::move(cb), sp::move(mask)));
}

GestureRecognizer *InputListener::addMoveRecognizer(InputCallback<GestureData> &&cb,
		bool withinNode) {
	return addRecognizer(Rc<GestureMoveRecognizer>::create(sp::move(cb), withinNode));
}

GestureRecognizer *InputListener::addMouseOverRecognizer(InputCallback<GestureData> &&cb,
		float padding) {
	return addRecognizer(Rc<GestureMouseOverRecognizer>::create(sp::move(cb)));
}

GestureKeyRecognizer *InputListener::addKeyRecognizer(InputCallback<GestureData> &&cb,
		KeyMask &&keys) {
	return (GestureKeyRecognizer *)addRecognizer(
			Rc<GestureKeyRecognizer>::create(sp::move(cb), sp::move(keys)));
}

void InputListener::setPointerEnterCallback(Function<bool(bool)> &&cb) {
	if (cb) {
		_callbacks.insert_or_assign(InputEventName::PointerEnter, sp::move(cb));
		_eventMask.set(toInt(InputEventName::PointerEnter));
	} else {
		_callbacks.erase(InputEventName::PointerEnter);
		_eventMask.reset(toInt(InputEventName::PointerEnter));
	}
}

void InputListener::setBackgroudCallback(Function<bool(bool)> &&cb) {
	if (cb) {
		_callbacks.insert_or_assign(InputEventName::Background, sp::move(cb));
		_eventMask.set(toInt(InputEventName::Background));
	} else {
		_callbacks.erase(InputEventName::Background);
		_eventMask.reset(toInt(InputEventName::Background));
	}
}

void InputListener::setFocusCallback(Function<bool(bool)> &&cb) {
	if (cb) {
		_callbacks.insert_or_assign(InputEventName::FocusGain, sp::move(cb));
		_eventMask.set(toInt(InputEventName::FocusGain));
	} else {
		_callbacks.erase(InputEventName::FocusGain);
		_eventMask.reset(toInt(InputEventName::FocusGain));
	}
}

void InputListener::setCloseRequestCallback(Function<bool(bool)> &&cb) {
	if (cb) {
		_callbacks.insert_or_assign(InputEventName::CloseRequest, sp::move(cb));
		_eventMask.set(toInt(InputEventName::CloseRequest));
	} else {
		_callbacks.erase(InputEventName::CloseRequest);
		_eventMask.reset(toInt(InputEventName::CloseRequest));
	}
}

void InputListener::setScreenUpdateCallback(Function<bool(bool)> &&cb) {
	if (cb) {
		_callbacks.insert_or_assign(InputEventName::ScreenUpdate, sp::move(cb));
		_eventMask.set(toInt(InputEventName::ScreenUpdate));
	} else {
		_callbacks.erase(InputEventName::ScreenUpdate);
		_eventMask.reset(toInt(InputEventName::ScreenUpdate));
	}
}

void InputListener::setFullscreenCallback(Function<bool(bool)> &&cb) {
	if (cb) {
		_callbacks.insert_or_assign(InputEventName::Fullscreen, sp::move(cb));
		_eventMask.set(toInt(InputEventName::Fullscreen));
	} else {
		_callbacks.erase(InputEventName::Fullscreen);
		_eventMask.reset(toInt(InputEventName::Fullscreen));
	}
}

void InputListener::clear() {
	_eventMask.reset();
	_recognizers.clear();
}

bool InputListener::shouldProcessEvent(const InputEvent &event) const {
	if (!_eventFilter) {
		return _shouldProcessEvent(event);
	} else {
		return _eventFilter(event, std::bind(&InputListener::_shouldProcessEvent, this, event));
	}
}

bool InputListener::_shouldProcessEvent(const InputEvent &event) const {
	auto node = getOwner();
	if (node && _running) {
		bool visible = node->isVisible();
		auto p = node->getParent();
		while (visible && p) {
			visible = p->isVisible();
			p = p->getParent();
		}
		if (visible
				&& (!event.data.hasLocation() || event.data.event == InputEventName::MouseMove
						|| node->isTouched(event.currentLocation, _touchPadding))
				&& node->getOpacity() >= _opacityFilter) {
			return true;
		}
	}
	return false;
}

void InputListener::addEventMask(const EventMask &mask) {
	for (size_t i = 0; i < mask.size(); ++i) {
		if (mask.test(i)) {
			_eventMask.set(i);
		}
	}
}

GestureRecognizer *InputListener::addRecognizer(GestureRecognizer *rec) {
	addEventMask(rec->getEventMask());
	auto ret = _recognizers.emplace_back(rec).get();
	if (_running) {
		ret->onEnter(this);
	}
	return ret;
}

} // namespace stappler::xenolith
