/**
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

#ifndef XENOLITH_SCENE_NODES_XLEVENTLISTENER_H_
#define XENOLITH_SCENE_NODES_XLEVENTLISTENER_H_

#include "XLEvent.h"
#include "XLEventHandler.h"
#include "XLComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class SP_PUBLIC EventListener : public Component, public EventHandler {
public:
	using EventCallback = Function<void(const Event &)>;

	virtual ~EventListener();
	virtual bool init() override;

	void onEventRecieved(const Event &ev, const EventCallback &);

	EventHandlerNode * onEvent(const EventHeader &h, EventCallback &&, bool destroyAfterEvent = false);
	EventHandlerNode * onEventWithObject(const EventHeader &h, Ref *obj, EventCallback &&, bool destroyAfterEvent = false);

	void clear();
};

}

#endif /* XENOLITH_SCENE_NODES_XLEVENTLISTENER_H_ */
