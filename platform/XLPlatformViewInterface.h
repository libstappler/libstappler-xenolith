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

#ifndef XENOLITH_PLATFORM_XLPLATFORMVIEWINTERFACE_H_
#define XENOLITH_PLATFORM_XLPLATFORMVIEWINTERFACE_H_

#include "XLCoreInput.h"
#include "XLCoreInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class ViewInterface {
public:
	virtual ~ViewInterface() { }

	virtual void update(bool displayLink) = 0;
	virtual void end() = 0;

	virtual void handleInputEvent(const core::InputEventData &) = 0;
	virtual void handleInputEvents(Vector<core::InputEventData> &&) = 0;

	virtual Extent2 getExtent() const = 0;

	virtual bool isInputEnabled() const = 0;

	virtual void deprecateSwapchain(bool fast = false) = 0;

	virtual uint64_t retainView() = 0;
	virtual void releaseView(uint64_t) = 0;

	virtual void setReadyForNextFrame() = 0;

	virtual void linkWithNativeWindow(void *) = 0;
	virtual void stopNativeWindow() = 0;

	virtual void setContentPadding(const Padding &) = 0;

	virtual uint64_t getBackButtonCounter() const = 0;
};

class ViewInterfaceRef final {
public:
	~ViewInterfaceRef() {
		set(nullptr);
	}

	ViewInterfaceRef() { }

	ViewInterfaceRef(ViewInterface *iface) {
		set(iface);
	}

	ViewInterfaceRef(const ViewInterfaceRef &r) {
		set(r.get());
	}

	ViewInterfaceRef(ViewInterfaceRef &&r) : refId(r.refId), ref(r.ref) {
		r.refId = 0;
		r.ref = nullptr;
	}

	ViewInterfaceRef &operator=(ViewInterface *iface) {
		set(iface);
		return *this;
	}

	ViewInterfaceRef &operator=(const ViewInterfaceRef &r) {
		set(r.get());
		return *this;
	}

	ViewInterfaceRef &operator=(ViewInterfaceRef &&r) {
		set(nullptr);
		refId = r.refId;
		ref = r.ref;
		r.refId = 0;
		r.ref = nullptr;
		return *this;
	}

	ViewInterface *get() const { return ref; }

	inline operator ViewInterface * () const { return get(); }
	inline explicit operator bool () const { return ref != nullptr; }
	inline ViewInterface * operator->() const { return get(); }

private:
	void set(ViewInterface *r) {
		if (ref) {
			ref->releaseView(refId);
		}
		ref = r;
		if (ref) {
			refId = ref->retainView();
		} else {
			refId = 0;
		}
	}

	uint64_t refId = 0;
	ViewInterface *ref = nullptr;
};

}

#endif /* XENOLITH_PLATFORM_XLPLATFORMVIEWINTERFACE_H_ */
