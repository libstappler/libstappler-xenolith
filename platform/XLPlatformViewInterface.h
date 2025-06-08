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

#include "SPEnum.h"
#include "XLCoreInput.h"
#include "XLCoreLoop.h"
#include "XLCorePresentationEngine.h"
#include "XLPlatformTextInputInterface.h"

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

enum class ViewLayerFlags : uint32_t {
	None,
	CursorText,
	CursorPointer,
	CursorHelp,
	CursorProgress,
	CursorWait,
	CursorCopy,
	CursorAlias,
	CursorNoDrop,
	CursorNotAllowed,
	CursorAllScroll,
	CursorRowResize,
	CursorColResize,
	CursorMask = 0xF,

	ResizableTop = 1 << 27,
	ResizableRight = 1 << 28,
	ResizableBottom = 1 << 29,
	ResizableLeft = 1 << 30,
	ResizeMask = ResizableTop | ResizableRight | ResizableBottom | ResizableLeft,
};

SP_DEFINE_ENUM_AS_MASK(ViewLayerFlags)

struct ViewLayer {
	Rect rect;
	ViewLayerFlags flags;

	bool operator==(const ViewLayer &) const = default;
	bool operator!=(const ViewLayer &) const = default;
};

class SP_PUBLIC ViewInterface : public Ref {
public:
	static EventHeader onBackground;
	static EventHeader onFocus;

	virtual ~ViewInterface() = default;

	virtual bool init(PlatformApplication &, core::Loop &);

	virtual void update(bool displayLink); // from view thread
	virtual void end() = 0; // from view thread

	virtual void handleInputEvent(const core::InputEventData &); // from view thread
	virtual void handleInputEvents(Vector<core::InputEventData> &&); // from view thread

	virtual Extent2 getExtent() const = 0; // from view thread
	virtual const WindowInfo &getWindowInfo() const = 0; // from view thread

	const ViewLayer *getTopLayer(Vec2) const; // from view thread
	const ViewLayer *getBottomLayer(Vec2) const; // from view thread

	virtual bool isTextInputEnabled() const = 0; // from view thread

	virtual void linkWithNativeWindow(void *) = 0; // from view thread

	virtual uint64_t getBackButtonCounter() const = 0; // from any thread

	virtual void readFromClipboard(Function<void(BytesView, StringView)> &&,
			Ref *) = 0; // from any thread
	virtual void writeToClipboard(BytesView,
			StringView contentType = StringView()) = 0; // from any thread

	PlatformApplication *getApplication() const { return _application; } // from any thread

	core::Loop *getLoop() const { return _loop; } // from any thread

	bool isOnThisThread() const; // from any thread

	void performOnThread(Function<void()> &&func, Ref *target = nullptr, bool immediate = false,
			StringView tag = STAPPLER_LOCATION); // from any thread

	core::PresentationEngine *getPresentationEngine() const { return _presentationEngine; }

	void updateConfig(); // from any thread

	void setReadyForNextFrame(); // from any thread

	void setRenderOnDemand(bool value); // from any thread
	bool isRenderOnDemand() const; // from any thread

	void setFrameInterval(uint64_t); // from any thread
	uint64_t getFrameInterval() const; // from any thread

	void setContentPadding(const Padding &padding); // from any thread

	// Block current thread until next frame
	void waitUntilFrame(); // from any thread

	void acquireTextInput(TextInputRequest &&); // from app thread
	void releaseTextInput(); // from app thread

	void updateLayers(Vector<ViewLayer> &&); // from app thread

protected:
	friend class TextInputInterface;

	//
	// native text input functions
	//
	// This functions operates with internal platform's text input buffer to provide
	// user with platform's autocomplete, spelling correction and specialized screen keyboard
	//

	// Run text input mode with buffer with new string, cursor and input type,
	// or update text input buffer
	virtual bool updateTextInput(const TextInputRequest &,
			TextInputFlags flags = TextInputFlags::RunIfDisabled) = 0; // from view thread

	// Disable text input, if it was enabled
	virtual void cancelTextInput() = 0; // from view thread

	virtual void propagateInputEvent(core::InputEventData &); // from app thread
	virtual void propagateTextInput(TextInputState &); // from app thread

	virtual void handleLayers(Vector<ViewLayer> &&); // from view thread
	virtual void handleMotionEvent(const core::InputEventData &); // from view thread

	virtual void handleLayerUpdate(const ViewLayer &);

	Rc<core::Loop> _loop;
	Rc<core::PresentationEngine> _presentationEngine;
	Rc<TextInputInterface> _textInput;
	Rc<PlatformApplication> _application;

	Vector<ViewLayer> _layers;

	bool _inBackground = false;
	bool _hasFocus = true;
	bool _pointerInWindow = false;

	// usually, text input can be captured from keyboard, but on some systems text input separated from keyboard input
	bool _handleTextInputFromKeyboard = true;
	bool _handleLayerForMotion = true;
	ViewLayer _currentLayer;
};

} // namespace stappler::xenolith::platform

#endif /* XENOLITH_PLATFORM_XLPLATFORMVIEWINTERFACE_H_ */
