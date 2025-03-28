/**
 Copyright (c) 2020 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLEventHandler.h"
#include "XLApplication.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

EventHandler::EventHandler() { }

EventHandler::~EventHandler() {
	clearEvents();
}

void EventHandler::addHandlerNode(EventHandlerNode *handler) {
	auto linkId = handler->retain();
	_handlers.insert(handler);
	Application::getInstance()->performOnAppThread([handler, linkId] {
		Application::getInstance()->addEventListener(handler);

		handler->release(linkId);
	}, nullptr);
}
void EventHandler::removeHandlerNode(EventHandlerNode *handler) {
	auto linkId = handler->retain();
	if (_handlers.erase(handler) > 0) {
		handler->setSupport(nullptr);
		Application::getInstance()->performOnAppThread([handler, linkId] {
			Application::getInstance()->removeEventListner(handler);
			handler->release(linkId);
		}, nullptr);
	}
}

EventHandlerNode * EventHandler::setEventHandler(const EventHeader &h, Callback && callback, bool destroyAfterEvent) {
	return EventHandlerNode::onEvent(h, nullptr, sp::move(callback), this, destroyAfterEvent);
}

EventHandlerNode * EventHandler::setEventHandlerForObject(const EventHeader &h, Ref *obj, Callback && callback, bool destroyAfterEvent) {
	return EventHandlerNode::onEvent(h, obj, sp::move(callback), this, destroyAfterEvent);
}

Ref *EventHandler::getInterface() const {
	if (auto ref = dynamic_cast<const Ref *>(this)) {
		return const_cast<Ref *>(ref);
	}
	return nullptr;
}

void EventHandler::clearEvents() {
	auto h = sp::move(_handlers);
	_handlers.clear();

	for (auto it : h) {
		it->setSupport(nullptr);
	}

	Application::getInstance()->performOnAppThread([h = sp::move(h)] {
		for (auto it : h) {
			Application::getInstance()->removeEventListner(it);
		}
	}, nullptr);
}

Rc<EventHandlerNode> EventHandlerNode::onEvent(const EventHeader &header, Ref *ref, Callback && callback, EventHandler *obj, bool destroyAfterEvent) {
	if (callback) {
		auto h = Rc<EventHandlerNode>::alloc(header, ref, sp::move(callback), obj, destroyAfterEvent);
		obj->addHandlerNode(h);
		return h;
	}
	return nullptr;
}

EventHandlerNode::EventHandlerNode(const EventHeader &header, Ref *ref, Callback && callback, EventHandler *obj, bool destroyAfterEvent)
: _destroyAfterEvent(destroyAfterEvent), _eventID(header.getEventID()), _callback(sp::move(callback)), _obj(ref), _support(obj) { }

EventHandlerNode::~EventHandlerNode() { }

void EventHandlerNode::setSupport(EventHandler *s) {
	_support = s;
}

bool EventHandlerNode::shouldRecieveEventWithObject(EventHeader::EventID eventID, Ref *object) const {
	return _eventID == eventID && (!_obj || object == _obj);
};

EventHeader::EventID EventHandlerNode::getEventID() const { return _eventID; }

void EventHandlerNode::onEventRecieved(const Event &event) const {
	auto self = (Ref *)this;
	auto id = self->retain();
	auto s = _support.load();
	if (s) {
		Rc<Ref> iface(s->getInterface());
		_callback(event);
		if (_destroyAfterEvent) {
			s->removeHandlerNode(const_cast<EventHandlerNode *>(this));
		}
	}
	self->release(id);
}

}
