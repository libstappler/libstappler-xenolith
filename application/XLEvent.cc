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

#include "XLEvent.h"
#include "XLApplication.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

String Event::ZERO_STRING;
Bytes Event::ZERO_BYTES;

void Event::dispatch() const {
	Application::getInstance()->dispatchEvent(*this);
}

Ref *Event::getObject() const {
	return _object;
}

const EventHeader &Event::getHeader() const {
	return _header;
}

EventHeader::Category Event::getCategory() const {
	return _header.getCategory();
}
EventHeader::EventID Event::getEventID() const {
	return _header.getEventID();
}

bool Event::is(const EventHeader &eventHeader) const {
	return _header.getEventID() == eventHeader.getEventID();
}

bool Event::operator == (const EventHeader &eventHeader) const {
	return is(eventHeader);
}

Value Event::getValue() const {
	switch (_type) {
	case Type::Int:
		return Value(_value.intValue);
		break;
	case Type::Float:
		return Value(_value.floatValue);
		break;
	case Type::Bool:
		return Value(_value.boolValue);
		break;
	case Type::String:
		return Value(*_value.strValue);
		break;
	case Type::Bytes:
		return Value(*_value.bytesValue);
		break;
	case Type::Data:
		return Value(*_value.dataValue);
		break;
	case Type::Object:
	case Type::None: break;
	}
	return Value();
}

Event::Event(const EventHeader &header, Ref *object)
: _header(header), _object(object) { _value.intValue = 0; }

Event::Event(const EventHeader &header, Ref *object, EventValue val, Type type)
: _header(header), _type(type), _object(object), _value(val) { }

void Event::send(const EventHeader &header, Ref *object, int64_t value) {
	EventValue val; val.intValue = value;
	send(header, object, val, Type::Int);
}
void Event::send(const EventHeader &header, Ref *object, double value) {
	EventValue val; val.floatValue = value;
	send(header, object, val, Type::Float);
}
void Event::send(const EventHeader &header, Ref *object, bool value) {
	EventValue val; val.boolValue = value;
	send(header, object, val, Type::Bool);
}
void Event::send(const EventHeader &header, Ref *object, Ref *value) {
	EventValue val; val.objValue = value;
	send(header, object, val, Type::Object);
}
void Event::send(const EventHeader &header, Ref *object, const char *value) {
	String str = value;
	send(header, object, str);
}
void Event::send(const EventHeader &header, Ref *object, const String &value) {
	auto app = Application::getInstance();
	if (app) {
		app->performOnAppThread([header, object, value] () {
			EventValue val; val.strValue = &value;
			Event event(header, object, val, Type::String);
			event.dispatch();
		});
	}
}
void Event::send(const EventHeader &header, Ref *object, const StringView &value) {
	auto app = Application::getInstance();
	if (app) {
		app->performOnAppThread([header, object, value = value.str<Interface>()] () {
			EventValue val; val.strValue = &value;
			Event event(header, object, val, Type::String);
			event.dispatch();
		});
	}
}

void Event::send(const EventHeader &header, Ref *object, const BytesView &value) {
	auto app = Application::getInstance();
	if (app) {
		app->performOnAppThread([header, object, value = value.bytes<Interface>()] () {
			EventValue val; val.bytesValue = &value;
			Event event(header, object, val, Type::Bytes);
			event.dispatch();
		});
	}
}

void Event::send(const EventHeader &header, Ref *object, const Value &value) {
	auto app = Application::getInstance();
	if (app) {
		app->performOnAppThread([header, object, value] () {
			EventValue val; val.dataValue = &value;
			Event event(header, object, val, Type::Data);
			event.dispatch();
		});
	}
}
void Event::send(const EventHeader &header, Ref *object, EventValue val, Type type) {
	auto app = Application::getInstance();
	if (app) {
		app->performOnAppThread([header, object, val, type] () {
			Event event(header, object, val, type);
			event.dispatch();
		});
	}
}
void Event::send(const EventHeader &header, Ref *object) {
	auto app = Application::getInstance();
	if (app) {
		app->performOnAppThread([header, object] () {
			Event event(header, object);
			event.dispatch();
		});
	}
}

}
