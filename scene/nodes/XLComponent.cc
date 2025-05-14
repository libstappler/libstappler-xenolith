/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "XLComponent.h"
#include "XLScene.h"
#include "XLScheduler.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

uint64_t Component::GetNextComponentId() {
	static std::atomic<uint64_t> s_value = 1;
	return s_value.fetch_add(1);
}

Component::Component() : _owner(nullptr), _enabled(true) { }

Component::~Component() { }

bool Component::init() { return true; }

void Component::handleAdded(Node *owner) { _owner = owner; }
void Component::handleRemoved() { _owner = nullptr; }

void Component::handleEnter(Scene *sc) {
	_running = true;
	if (_scheduled) {
		sc->getDirector()->getScheduler()->scheduleUpdate(this, 0, false);
	}
}

void Component::handleExit() {
	if (_scheduled) {
		unscheduleUpdate();
		_scheduled = true; // -re-enable after restart;
	}
	_running = false;
}

void Component::handleVisitBegin(FrameInfo &) { }

void Component::handleVisitNodesBelow(FrameInfo &, SpanView<Rc<Node>>, NodeFlags flags) { }

void Component::handleVisitSelf(FrameInfo &, Node *, NodeFlags flags) { }

void Component::handleVisitNodesAbove(FrameInfo &, SpanView<Rc<Node>>, NodeFlags flags) { }

void Component::handleVisitEnd(FrameInfo &) { }

void Component::update(const UpdateTime &time) { }

void Component::handleContentSizeDirty() { }
void Component::handleTransformDirty(const Mat4 &) { }
void Component::handleReorderChildDirty() { }

bool Component::isRunning() const { return _running; }

bool Component::isEnabled() const { return _enabled; }

void Component::setEnabled(bool b) { _enabled = b; }

void Component::setComponentFlags(ComponentFlags flags) { _componentFlags = flags; }

bool Component::isScheduled() const { return _scheduled; }

void Component::scheduleUpdate() {
	if (!_scheduled) {
		_scheduled = true;
		if (_running) {
			_owner->getScheduler()->scheduleUpdate(this, 0, false);
		}
	}
}

void Component::unscheduleUpdate() {
	if (_scheduled) {
		if (_running) {
			_owner->getScheduler()->unschedule(this);
		}
		_scheduled = false;
	}
}

void Component::setFrameTag(uint64_t tag) { _frameTag = tag; }

CallbackComponent::CallbackComponent() { _componentFlags = ComponentFlags::None; }

void CallbackComponent::handleAdded(Node *owner) {
	Component::handleAdded(owner);

	if (_handleAdded) {
		_handleAdded(this, owner);
	}
}

void CallbackComponent::handleRemoved() {
	if (_handleRemoved) {
		_handleRemoved(this, _owner);
	}
	Component::handleRemoved();
}

void CallbackComponent::handleEnter(Scene *scene) {
	Component::handleEnter(scene);

	if (_handleEnter) {
		_handleEnter(this, scene);
	}
}

void CallbackComponent::handleExit() {
	Component::handleExit();

	if (_handleExit) {
		_handleExit(this);
	}
}

void CallbackComponent::handleVisitBegin(FrameInfo &info) {
	Component::handleVisitBegin(info);

	if (_handleVisitBegin) {
		_handleVisitBegin(this, info);
	}
}

void CallbackComponent::handleVisitNodesBelow(FrameInfo &info, SpanView<Rc<Node>> nodes,
		NodeFlags flags) {
	Component::handleVisitNodesBelow(info, nodes, flags);

	if (_handleVisitNodesBelow) {
		_handleVisitNodesBelow(this, info, nodes, flags);
	}
}

void CallbackComponent::handleVisitSelf(FrameInfo &info, Node *node, NodeFlags flags) {
	Component::handleVisitSelf(info, node, flags);

	if (_handleVisitSelf) {
		_handleVisitSelf(this, info, node, flags);
	}
}

void CallbackComponent::handleVisitNodesAbove(FrameInfo &info, SpanView<Rc<Node>> nodes,
		NodeFlags flags) {
	Component::handleVisitNodesAbove(info, nodes, flags);

	if (_handleVisitNodesAbove) {
		_handleVisitNodesAbove(this, info, nodes, flags);
	}
}

void CallbackComponent::handleVisitEnd(FrameInfo &info) {
	Component::handleVisitEnd(info);

	if (_handleVisitEnd) {
		_handleVisitEnd(this, info);
	}
}

void CallbackComponent::update(const UpdateTime &time) {
	Component::update(time);

	if (_handleUpdate) {
		_handleUpdate(this, time);
	}
}

void CallbackComponent::handleContentSizeDirty() {
	Component::handleContentSizeDirty();

	if (_handleContentSizeDirty) {
		_handleContentSizeDirty(this);
	}
}

void CallbackComponent::handleTransformDirty(const Mat4 &t) {
	Component::handleTransformDirty(t);

	if (_handleTransformDirty) {
		_handleTransformDirty(this, t);
	}
}

void CallbackComponent::handleReorderChildDirty() {
	Component::handleReorderChildDirty();

	if (_handleReorderChildDirty) {
		_handleReorderChildDirty(this);
	}
}

void CallbackComponent::setAddedCallback(Function<void(CallbackComponent *, Node *)> &&cb) {
	_handleAdded = sp::move(cb);
	updateFlags();
}

void CallbackComponent::setRemovedCallback(Function<void(CallbackComponent *, Node *)> &&cb) {
	_handleRemoved = sp::move(cb);
	updateFlags();
}

void CallbackComponent::setEnterCallback(Function<void(CallbackComponent *, Scene *)> &&cb) {
	_handleEnter = sp::move(cb);
	updateFlags();
}

void CallbackComponent::setExitCallback(Function<void(CallbackComponent *)> &&cb) {
	_handleExit = sp::move(cb);
	updateFlags();
}

void CallbackComponent::setVisitBeginCallback(
		Function<void(CallbackComponent *, FrameInfo &)> &&cb) {
	_handleVisitBegin = sp::move(cb);
	updateFlags();
}

void CallbackComponent::setVisitNodesBelowCallback(
		Function<void(CallbackComponent *, FrameInfo &, SpanView<Rc<Node>>, NodeFlags flags)>
				&&cb) {
	_handleVisitNodesBelow = sp::move(cb);
	updateFlags();
}

void CallbackComponent::setVisitSelfCallback(
		Function<void(CallbackComponent *, FrameInfo &, Node *, NodeFlags flags)> &&cb) {
	_handleVisitSelf = sp::move(cb);
	updateFlags();
}

void CallbackComponent::setVisitNodesAboveCallback(
		Function<void(CallbackComponent *, FrameInfo &, SpanView<Rc<Node>>, NodeFlags flags)>
				&&cb) {
	_handleVisitNodesAbove = sp::move(cb);
	updateFlags();
}

void CallbackComponent::setVisitEndCallback(Function<void(CallbackComponent *, FrameInfo &)> &&cb) {
	_handleVisitEnd = sp::move(cb);
	updateFlags();
}

void CallbackComponent::setUpdateCallback(
		Function<void(CallbackComponent *, const UpdateTime &)> &&cb) {
	_handleUpdate = sp::move(cb);
	updateFlags();
}

void CallbackComponent::setContentSizeDirtyCallback(Function<void(CallbackComponent *)> &&cb) {
	_handleContentSizeDirty = sp::move(cb);
	updateFlags();
}

void CallbackComponent::setTransformDirtyCallback(
		Function<void(CallbackComponent *, const Mat4 &)> &&cb) {
	_handleTransformDirty = sp::move(cb);
	updateFlags();
}

void CallbackComponent::setReorderChildDirtyCallback(Function<void(CallbackComponent *)> &&cb) {
	_handleReorderChildDirty = sp::move(cb);
	updateFlags();
}

void CallbackComponent::updateFlags() {
	if (_handleAdded || _handleRemoved) {
		_componentFlags |= ComponentFlags::HandleOwnerEvents;
	} else {
		_componentFlags &= ~ComponentFlags::HandleOwnerEvents;
	}

	if (_handleEnter || _handleExit) {
		_componentFlags |= ComponentFlags::HandleSceneEvents;
	} else {
		_componentFlags &= ~ComponentFlags::HandleSceneEvents;
	}

	if (_handleContentSizeDirty || _handleReorderChildDirty || _handleTransformDirty) {
		_componentFlags |= ComponentFlags::HandleNodeEvents;
	} else {
		_componentFlags &= ~ComponentFlags::HandleNodeEvents;
	}

	if (_handleVisitSelf) {
		_componentFlags |= ComponentFlags::HandleVisitSelf;
	} else {
		_componentFlags &= ~ComponentFlags::HandleVisitSelf;
	}

	if (_handleVisitBegin || _handleVisitNodesBelow || _handleVisitNodesAbove || _handleVisitEnd) {
		_componentFlags |= ComponentFlags::HandleVisitControl;
	} else {
		_componentFlags &= ~ComponentFlags::HandleVisitControl;
	}

	if (_handleUpdate) {
		scheduleUpdate();
	} else {
		unscheduleUpdate();
	}
}

} // namespace stappler::xenolith
