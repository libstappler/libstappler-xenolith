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

#ifndef XENOLITH_CORE_XLCOREINSTANCE_H_
#define XENOLITH_CORE_XLCOREINSTANCE_H_

#include "XLCore.h"
#include "SPDso.h"

#if MODULE_XENOLITH_SCENE

namespace stappler::xenolith {

struct ViewInfo;
class View;
class Application;

}

#endif

namespace stappler::xenolith::core {

class Loop;
class Queue;
class Device;

struct LoopInfo;

struct DeviceProperties {
	String deviceName;
	uint32_t apiVersion = 0;
	uint32_t driverVersion = 0;
	bool supportsPresentation = false;
};

class Instance : public Ref {
public:
	using TerminateCallback = Function<void()>;

	static constexpr uint32_t DefaultDevice = maxOf<uint32_t>();

	Instance(Dso &&, TerminateCallback &&, Rc<Ref> &&);
	virtual ~Instance();

	const Vector<DeviceProperties> &getAvailableDevices() const { return _availableDevices; }

	virtual Rc<Loop> makeLoop(LoopInfo &&) const;

	Ref *getUserdata() const { return _userdata; }

#if MODULE_XENOLITH_FONT
	virtual Rc<core::Queue> makeFontQueue(StringView name = StringView("FontQueue")) const;
#endif

#if MODULE_XENOLITH_SCENE
	virtual Rc<View> makeView(Application &, const Device &, ViewInfo &&) const;
#endif

protected:
	Dso _dsoModule;
	TerminateCallback _terminate;
	Rc<Ref> _userdata;
	Vector<DeviceProperties> _availableDevices;
};

}

#endif /* XENOLITH_CORE_XLCOREINSTANCE_H_ */
