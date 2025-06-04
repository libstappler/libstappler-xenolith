/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_PLATFORM_XLPLATFORMVIEWINTERFACE_H_
#define XENOLITH_PLATFORM_XLPLATFORMVIEWINTERFACE_H_

#include "XLCoreInput.h"
#include "XLCoreLoop.h"
#include "XLCorePresentationEngine.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class PlatformApplication;

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct SP_PUBLIC WindowInfo {
	String title;
	String bundleId;
	URect rect = URect(0, 0, 1'024, 768);
	Padding decoration;
	float density = 0.0f;
};

class ViewLayer : public Ref {
public:
	void handleEnter();
	void handleExit();
	void handleUpdate();


protected:
	URect rect;
};

class SP_PUBLIC ViewInterface : public Ref {
public:
	virtual ~ViewInterface() = default;

	virtual bool init(PlatformApplication &, core::Loop &);

	virtual void update(bool displayLink);
	virtual void end() = 0;

	virtual void handleInputEvent(const core::InputEventData &) = 0;
	virtual void handleInputEvents(Vector<core::InputEventData> &&) = 0;

	virtual Extent2 getExtent() const = 0;

	virtual bool isInputEnabled() const = 0;

	virtual void linkWithNativeWindow(void *) = 0;

	virtual uint64_t getBackButtonCounter() const = 0;

	virtual void readFromClipboard(Function<void(BytesView, StringView)> &&, Ref *) = 0;
	virtual void writeToClipboard(BytesView, StringView contentType = StringView()) = 0;

	PlatformApplication *getApplication() const { return _application; }

	core::Loop *getLoop() const { return _loop; }

	bool isOnThisThread() const;

	void performOnThread(Function<void()> &&func, Ref *target = nullptr, bool immediate = false,
			StringView tag = STAPPLER_LOCATION);

	core::PresentationEngine *getPresentationEngine() const { return _presentationEngine; }

	void updateConfig();

	void setReadyForNextFrame();

	void setRenderOnDemand(bool value);
	bool isRenderOnDemand() const;

	void setFrameInterval(uint64_t);
	uint64_t getFrameInterval() const;

	void setContentPadding(const Padding &padding);

	void waitUntilFrame();

protected:
	Rc<core::Loop> _loop;
	Rc<core::PresentationEngine> _presentationEngine;
	Rc<PlatformApplication> _application;
};

} // namespace stappler::xenolith::platform

#endif /* XENOLITH_PLATFORM_XLPLATFORMVIEWINTERFACE_H_ */
