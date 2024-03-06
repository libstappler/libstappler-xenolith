/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

namespace STAPPLER_VERSIONIZED stappler::xenolith {

uint64_t Component::GetNextComponentId() {
	static std::atomic<uint64_t> s_value = 1;
	return s_value.fetch_add(1);
}

Component::Component()
: _owner(nullptr), _enabled(true) { }

Component::~Component() { }

bool Component::init() {
	return true;
}

void Component::onAdded(Node *owner) {
	_owner = owner;
}
void Component::onRemoved() {
	_owner = nullptr;
}

void Component::onEnter(Scene *sc) {
	_running = true;
	if (_scheduled) {
		sc->getScheduler()->scheduleUpdate(this, 0, false);
	}
}

void Component::onExit() {
	if (_scheduled) {
		unscheduleUpdate();
		_scheduled = true; // -re-enable after restart;
	}
	_running = false;
}

void Component::visit(FrameInfo &, NodeFlags parentFlags) { }

void Component::update(const UpdateTime &time) { }

void Component::onContentSizeDirty() { }
void Component::onTransformDirty(const Mat4 &) { }
void Component::onReorderChildDirty() { }

bool Component::isRunning() const {
	return _running;
}

bool Component::isEnabled() const {
	return _enabled;
}

void Component::setEnabled(bool b) {
	_enabled = b;
}

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

void Component::setFrameTag(uint64_t tag) {
	_frameTag = tag;
}

}
