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

#ifndef XENOLITH_APPLICATION_INPUT_XLGESTURERECOGNIZER_H_
#define XENOLITH_APPLICATION_INPUT_XLGESTURERECOGNIZER_H_

#include "XLCommon.h"
#include "XLInput.h"
#include "SPMovingAverage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

static constexpr float TapDistanceAllowed = 12.0f;
static constexpr float TapDistanceAllowedMulti = 32.0f;
static constexpr TimeInterval TapIntervalAllowed = TimeInterval::microseconds(300'000ULL);

class InputListener;

enum class GestureEvent {
	/** Action just started, listener should return true if it want to "capture" it.
	 * Captured actions will be automatically propagated to end-listener
	 * Other listeners branches will not recieve updates on action, that was not captured by them.
	 * Only one listener on every level can capture action. If one of the listeners return 'true',
	 * action will be captured by this listener, no other listener on this level can capture this action.
	 */
	Began,

	/** Action was activated:
	 * on Touch - touch was moved
	 * on Tap - n-th  tap was recognized
	 * on Press - long touch was recognized
	 * on Swipe - touch was moved
	 * on Pinch - any of two touches was moved, scale was changed
	 * on Rotate - any of two touches was moved, rotation angle was changed
	 */
	Activated,
	Moved = Activated,
	OnLongPress = Activated,
	Repeat = Activated,

	/** Action was successfully ended, no recognition errors was occurred */
	Ended,

	/** Action was not successfully ended, recognizer detects error in action
	 * pattern and failed to continue recognition */
	Cancelled,
};

struct SP_PUBLIC GestureData {
	GestureEvent event = GestureEvent::Began;
	const InputEvent *input = nullptr;

	Vec2 location() const { return input->currentLocation; }
	uint32_t getId() const { return input->data.id; }
};

struct SP_PUBLIC GestureScroll : GestureData {
	Vec2 pos;
	Vec2 amount;

	const Vec2 &location() const;
	void cleanup();
};

struct SP_PUBLIC GestureTap : GestureData {
	Vec2 pos;
	uint32_t id = maxOf<uint32_t>();
	uint32_t count = 0;
	Time time;

	void cleanup();
};

struct SP_PUBLIC GesturePress : GestureData {
	Vec2 pos;
	uint32_t id = maxOf<uint32_t>();
	TimeInterval limit;
	TimeInterval time;
	uint32_t tickCount = 0;

	void cleanup();
};

struct SP_PUBLIC GestureSwipe : GestureData {
	Vec2 firstTouch;
	Vec2 secondTouch;
	Vec2 midpoint;
	Vec2 delta;
	Vec2 velocity;
	float density = 1.0f;

	void cleanup();
};

struct SP_PUBLIC GesturePinch : GestureData {
	Vec2 first;
	Vec2 second;
	Vec2 center;
	float startDistance = 0.0f;
	float prevDistance = 0.0f;
	float distance = 0.0f;
	float scale = 0.0f;
	float velocity = 0.0f;
	float density = 1.0f;

	void cleanup();
};

class SP_PUBLIC GestureRecognizer : public Ref {
public:
	using EventMask = std::bitset<toInt(InputEventName::Max)>;
	using ButtonMask = std::bitset<toInt(InputMouseButton::Max)>;

	virtual ~GestureRecognizer() { }

	virtual bool init();

	virtual bool canHandleEvent(const InputEvent &event) const;
	virtual InputEventState handleInputEvent(const InputEvent &, float density);

	virtual void onEnter(InputListener *);
	virtual void onExit();

	uint32_t getEventCount() const;
	bool hasEvent(const InputEvent &) const;

	EventMask getEventMask() const;

	virtual void update(uint64_t dt);
	virtual Vec2 getLocation() const;
	virtual void cancel();

	virtual void setMaxEvents(size_t value) { _maxEvents = value; }
	virtual size_t getMaxEvents() const { return _maxEvents; }

protected:
	virtual bool canAddEvent(const InputEvent &) const;
	virtual InputEventState addEvent(const InputEvent &, float density);
	virtual InputEventState removeEvent(const InputEvent &, bool success, float density);
	virtual InputEventState renewEvent(const InputEvent &, float density);

	virtual InputEvent *getTouchById(uint32_t id, uint32_t *index);

	Vector<InputEvent> _events;
	size_t _maxEvents = 0;
	EventMask _eventMask;
	ButtonMask _buttonMask;
	float _density = 1.0f;
};

class SP_PUBLIC GestureTouchRecognizer : public GestureRecognizer {
public:
	using InputCallback = Function<bool(const GestureData &)>;

	virtual ~GestureTouchRecognizer() { }

	virtual bool init(InputCallback &&, ButtonMask &&);

	// disable touches if no button specified
	virtual bool canHandleEvent(const InputEvent &event) const override;

	void removeRecognizedEvent(uint32_t);

protected:
	using GestureRecognizer::init;

	virtual InputEventState addEvent(const InputEvent &, float density) override;
	virtual InputEventState removeEvent(const InputEvent &, bool successful,
			float density) override;
	virtual InputEventState renewEvent(const InputEvent &, float density) override;

	GestureData _event = GestureData{GestureEvent::Cancelled, nullptr};
	InputCallback _callback;
};

class SP_PUBLIC GestureTapRecognizer : public GestureRecognizer {
public:
	using InputCallback = Function<void(const GestureTap &)>;
	using ButtonMask = std::bitset<toInt(InputMouseButton::Max)>;

	virtual ~GestureTapRecognizer() { }

	virtual bool init(InputCallback &&, ButtonMask &&, uint32_t maxTapCount);

	virtual void update(uint64_t dt) override;
	virtual void cancel() override;

protected:
	using GestureRecognizer::init;

	virtual InputEventState addEvent(const InputEvent &, float density) override;
	virtual InputEventState removeEvent(const InputEvent &, bool successful,
			float density) override;
	virtual InputEventState renewEvent(const InputEvent &, float density) override;

	virtual void registerTap();

	GestureTap _gesture;
	InputCallback _callback;
	uint32_t _maxTapCount = 2;
	InputEvent _tmpEvent;
};

class SP_PUBLIC GesturePressRecognizer : public GestureRecognizer {
public:
	using InputCallback = Function<bool(const GesturePress &)>;

	virtual ~GesturePressRecognizer() { }

	virtual bool init(InputCallback &&, TimeInterval interval, bool continuous, ButtonMask &&);

	virtual void update(uint64_t dt) override;
	virtual void cancel() override;

protected:
	using GestureRecognizer::init;

	virtual InputEventState addEvent(const InputEvent &, float density) override;
	virtual InputEventState removeEvent(const InputEvent &, bool successful,
			float density) override;
	virtual InputEventState renewEvent(const InputEvent &, float density) override;

	Time _lastTime = Time::now();
	bool _notified = false;

	GesturePress _gesture;
	InputCallback _callback;

	TimeInterval _interval;
	bool _continuous = false;
};

class SP_PUBLIC GestureSwipeRecognizer : public GestureRecognizer {
public:
	using InputCallback = Function<bool(const GestureSwipe &)>;

	virtual ~GestureSwipeRecognizer() { }

	virtual bool init(InputCallback &&, float threshold, bool includeThreshold, ButtonMask &&);

	virtual void cancel() override;

protected:
	using GestureRecognizer::init;

	virtual InputEventState addEvent(const InputEvent &, float density) override;
	virtual InputEventState removeEvent(const InputEvent &, bool successful,
			float density) override;
	virtual InputEventState renewEvent(const InputEvent &, float density) override;

	Time _lastTime;
	math::MovingAverage<4> _velocityX, _velocityY;

	bool _swipeBegin = false;
	uint32_t _currentTouch = maxOf<uint32_t>();

	GestureSwipe _gesture;
	InputCallback _callback;

	float _threshold = 6.0f;
	bool _includeThreshold = true;
};

class SP_PUBLIC GesturePinchRecognizer : public GestureRecognizer {
public:
	using InputCallback = Function<void(const GesturePinch &)>;

	virtual ~GesturePinchRecognizer() { }

	virtual bool init(InputCallback &&, ButtonMask &&);

	virtual void cancel() override;

protected:
	using GestureRecognizer::init;

	virtual InputEventState addEvent(const InputEvent &, float density) override;
	virtual InputEventState removeEvent(const InputEvent &, bool successful,
			float density) override;
	virtual InputEventState renewEvent(const InputEvent &, float density) override;

	Time _lastTime;
	math::MovingAverage<3> _velocity;

	GesturePinch _gesture;
	InputCallback _callback;
};

class SP_PUBLIC GestureScrollRecognizer : public GestureRecognizer {
public:
	using InputCallback = Function<bool(const GestureScroll &)>;

	virtual ~GestureScrollRecognizer() { }

	virtual bool init(InputCallback &&);

	virtual InputEventState handleInputEvent(const InputEvent &, float density) override;

protected:
	using GestureRecognizer::init;

	GestureScroll _gesture;
	InputCallback _callback;
};

class SP_PUBLIC GestureMoveRecognizer : public GestureRecognizer {
public:
	using InputCallback = Function<bool(const GestureData &)>;

	virtual ~GestureMoveRecognizer() { }

	virtual bool init(InputCallback &&, bool withinNode);

	virtual bool canHandleEvent(const InputEvent &event) const override;
	virtual InputEventState handleInputEvent(const InputEvent &, float density) override;

	virtual void onEnter(InputListener *) override;
	virtual void onExit() override;

protected:
	using GestureRecognizer::init;

	GestureData _event = GestureData{GestureEvent::Cancelled, nullptr};
	InputCallback _callback;
	InputListener *_listener = nullptr;
	bool _onlyWithinNode = true;
};

class SP_PUBLIC GestureKeyRecognizer : public GestureRecognizer {
public:
	using InputCallback = Function<bool(const GestureData &)>;
	using KeyMask = std::bitset<toInt(InputKeyCode::Max)>;

	virtual ~GestureKeyRecognizer() { }

	virtual bool init(InputCallback &&, KeyMask &&);

	virtual bool canHandleEvent(const InputEvent &) const override;

	bool isKeyPressed(InputKeyCode) const;

protected:
	using GestureRecognizer::init;

	virtual InputEventState addEvent(const InputEvent &, float density) override;
	virtual InputEventState removeEvent(const InputEvent &, bool success, float density) override;
	virtual InputEventState renewEvent(const InputEvent &, float density) override;

	KeyMask _keyMask;
	KeyMask _pressedKeys;
	InputCallback _callback;
};

class SP_PUBLIC GestureMouseOverRecognizer : public GestureRecognizer {
public:
	using InputCallback = Function<bool(const GestureData &)>;

	virtual ~GestureMouseOverRecognizer() { }

	virtual bool init(InputCallback &&, float padding = 0.0f);

	virtual InputEventState handleInputEvent(const InputEvent &, float density) override;

	virtual void onEnter(InputListener *) override;
	virtual void onExit() override;

protected:
	using GestureRecognizer::init;

	void updateState(const InputEvent &);

	GestureData _event = GestureData{GestureEvent::Cancelled, nullptr};
	bool _viewHasPointer = false;
	bool _viewHasFocus = false;
	bool _hasMouseOver = false;
	bool _value = false;
	float _padding = 0.0f;
	InputCallback _callback;
	InputListener *_listener = nullptr;
};

SP_PUBLIC std::ostream &operator<<(std::ostream &, GestureEvent);

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_INPUT_XLGESTURERECOGNIZER_H_ */
