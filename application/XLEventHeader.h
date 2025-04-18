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

#ifndef XENOLITH_APPLICATION_XLEVENTHEADER_H_
#define XENOLITH_APPLICATION_XLEVENTHEADER_H_

#include "XLCommon.h"
#include "SPData.h"

#define XL_DECLARE_EVENT_CLASS(class, event) \
	STAPPLER_VERSIONIZED_NAMESPACE::xenolith::EventHeader class::event(#class, #class "." #event);

#define XL_DECLARE_EVENT(class, catName, event) \
	STAPPLER_VERSIONIZED_NAMESPACE::xenolith::EventHeader class::event(catName, catName "." #event);

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Event;

/* Event headers contains category and id of event.
 Headers should be used to statically declare event to be recognized by dispatcher and listeners
 Header declares unique event name, to be used, when we need to recognize this event

 EventHeaders should be declared statically with simple constructor:
	EventHeader eventHeader(Category);
 */

class SP_PUBLIC EventHeader {
public:
	using Category = int;
	using EventID = int;

	static Category getCategoryForName(StringView catName);
public:

	EventHeader() = delete;
	EventHeader(StringView catName, StringView eventName);
	EventHeader(Category cat, StringView eventName);
	~EventHeader();

	EventHeader(const EventHeader &other);
	EventHeader(EventHeader &&other);

	EventHeader &operator=(const EventHeader &other);
	EventHeader &operator=(EventHeader &&other);

	Category getCategory() const;
	EventID getEventID() const;
	StringView getName() const;

	bool isInCategory(Category cat) const;

	operator int ();

	bool operator == (const Event &event) const;

	template <typename T>
	inline void operator() (Ref *object, const T &value) const { send(object, value); }

	inline void operator() (Ref *object) const { send(object); }

protected:
	void send(Ref *object, int64_t value) const;
	void send(Ref *object, double value) const;
	void send(Ref *object, bool value) const;
	void send(Ref *object, Ref *value) const;
	void send(Ref *object, const char *value) const;
	void send(Ref *object, const String &value) const;
	void send(Ref *object, const StringView &value) const;
	void send(Ref *object, const BytesView &value) const;
	void send(Ref *object, const Value &value) const;
	void send(Ref *object = nullptr) const;

	Category _category = 0;
	EventID _id = 0;
	StringView _name;
};

}

#endif /* XENOLITH_APPLICATION_XLEVENTHEADER_H_ */
