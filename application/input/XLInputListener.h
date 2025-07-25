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

#ifndef XENOLITH_APPLICATION_INPUT_XLINPUTLISTENER_H_
#define XENOLITH_APPLICATION_INPUT_XLINPUTLISTENER_H_

#include "XLComponent.h"
#include "XLNodeInfo.h"
#include "XLGestureRecognizer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Node;
class Scene;
class GestureRecognizer;

class SP_PUBLIC InputListener : public Component {
public:
	using EventMask = std::bitset<toInt(InputEventName::Max)>;
	using ButtonMask = std::bitset<toInt(InputMouseButton::Max)>;
	using KeyMask = std::bitset<toInt(InputKeyCode::Max)>;

	template <typename T>
	using InputCallback = Function<bool(const T &)>;

	using DefaultEventFilter = std::function<bool(const InputEvent &)>;
	using EventFilter = Function<bool(const InputEvent &, const DefaultEventFilter &)>;

	static EventMask EventMaskTouch;
	static EventMask EventMaskKeyboard;

	static ButtonMask makeButtonMask(std::initializer_list<InputMouseButton> &&);
	static EventMask makeEventMask(std::initializer_list<InputEventName> &&);
	static KeyMask makeKeyMask(std::initializer_list<InputKeyCode> &&);

	virtual ~InputListener() = default;

	bool init(int32_t priority = 0);

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleVisitSelf(FrameInfo &, Node *, NodeFlags flags) override;

	virtual void update(const UpdateTime &) override;

	void setViewLayerFlags(WindowLayerFlags);
	WindowLayerFlags getViewLayerFlags() const { return _layerFlags; }

	void setOwner(Node *pOwner);
	Node *getOwner() const { return _owner; }

	void setPriority(int32_t);
	int32_t getPriority() const { return _priority; }

	void setDedicatedFocus(uint32_t);
	uint32_t getDedicatedFocus() const { return _dedicatedFocus; }

	void setOpacityFilter(float value) { _opacityFilter = value; }
	float getOpacityFilter() const { return _opacityFilter; }

	void setTouchPadding(float value) { _touchPadding = value; }
	float getTouchPadding() const { return _touchPadding; }

	void setExclusive();
	void setExclusiveForTouch(uint32_t eventId);

	void setSwallowEvents(EventMask &&);
	void setSwallowEvents(const EventMask &);
	void setSwallowAllEvents();
	void setSwallowEvent(InputEventName);
	void clearSwallowAllEvents();
	void clearSwallowEvent(InputEventName);
	void clearSwallowEvents(const EventMask &);

	bool isSwallowAllEvents() const;
	bool isSwallowAllEvents(const EventMask &) const;
	bool isSwallowAnyEvents(const EventMask &) const;
	bool isSwallowEvent(InputEventName) const;

	void setTouchFilter(const EventFilter &);

	bool shouldSwallowEvent(const InputEvent &) const;
	bool canHandleEvent(const InputEvent &event) const;
	InputEventState handleEvent(const InputEvent &event);

	GestureRecognizer *addTouchRecognizer(InputCallback<GestureData> &&,
			ButtonMask && = makeButtonMask({InputMouseButton::Touch}));
	GestureRecognizer *addTapRecognizer(InputCallback<GestureTap> &&,
			ButtonMask && = makeButtonMask({InputMouseButton::Touch}), uint32_t maxTapCount = 2);
	GestureRecognizer *addPressRecognizer(InputCallback<GesturePress> &&,
			TimeInterval interval = TapIntervalAllowed, bool continuous = false,
			ButtonMask && = makeButtonMask({InputMouseButton::Touch}));
	GestureRecognizer *addSwipeRecognizer(InputCallback<GestureSwipe> &&,
			float threshold = TapDistanceAllowed, bool sendThreshold = false,
			ButtonMask && = makeButtonMask({InputMouseButton::Touch}));
	GestureRecognizer *addPinchRecognizer(InputCallback<GesturePinch> &&,
			ButtonMask && = makeButtonMask({InputMouseButton::Touch}));
	GestureRecognizer *addScrollRecognizer(InputCallback<GestureScroll> &&);
	GestureRecognizer *addMoveRecognizer(InputCallback<GestureData> &&, bool withinNode = true);
	GestureRecognizer *addMouseOverRecognizer(InputCallback<GestureData> &&, float padding = 0.0f);

	GestureKeyRecognizer *addKeyRecognizer(InputCallback<GestureData> &&, KeyMask && = KeyMask());

	void setPointerEnterCallback(Function<bool(bool)> &&);
	void setBackgroudCallback(Function<bool(bool)> &&);
	void setFocusCallback(Function<bool(bool)> &&);
	void setCloseRequestCallback(Function<bool(bool)> &&);
	void setScreenUpdateCallback(Function<bool(bool)> &&);
	void setFullscreenCallback(Function<bool(bool)> &&);

	void clear();

protected:
	bool shouldProcessEvent(const InputEvent &) const;
	bool _shouldProcessEvent(const InputEvent &) const; // default realization

	void addEventMask(const EventMask &);

	GestureRecognizer *addRecognizer(GestureRecognizer *);

	int32_t _priority = 0; // 0 - scene graph
	uint32_t _dedicatedFocus = 0; // 0 - unused
	EventMask _eventMask;
	EventMask _swallowEvents;
	WindowLayerFlags _layerFlags = WindowLayerFlags::None;

	float _touchPadding = 0.0f;
	float _opacityFilter = 0.0f;

	Scene *_scene = nullptr;

	EventFilter _eventFilter;
	Vector<Rc<GestureRecognizer>> _recognizers;
	Map<InputEventName, Function<bool(bool)>> _callbacks;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_INPUT_XLINPUTLISTENER_H_ */
