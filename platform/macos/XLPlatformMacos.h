/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_PLATFORM_MACOS_XLPLATFORMMACOS_H_
#define XENOLITH_PLATFORM_MACOS_XLPLATFORMMACOS_H_

#include "XLCommon.h"

#if MACOS

#include "XLPlatformViewInterface.h"

#include <sys/cdefs.h>
#include <objc/objc.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CoreVideo.h>

#if __OBJC__

#define XL_OBJC_CALL(...) __VA_ARGS__
#define XL_OBJC_CALL_V(__TYPE__, ...) __TYPE__(__VA_ARGS__)

#define XL_OBJC_INTERFACE_FORWARD(__NAME__) @class __NAME__
#define XL_OBJC_INTERFACE_BEGIN(__NAME__) @interface __NAME__
#define XL_OBJC_INTERFACE_END(__NAME__) @end
#define XL_OBJC_IMPLEMENTATION_BEGIN(__NAME__) @implementation __NAME__
#define XL_OBJC_IMPLEMENTATION_END(__NAME__) @end

#else

#define __bridge

#define XL_OBJC_CALL(...) ((void *)0)
#define XL_OBJC_CALL_V(__TYPE__, ...) (__TYPE__)()
#define XL_OBJC_INTERFACE_FORWARD(__NAME__) typedef void __NAME__
#define XL_OBJC_INTERFACE_BEGIN(__NAME__) namespace { class __NAME__
#define XL_OBJC_INTERFACE_END(__NAME__) }
#define XL_OBJC_IMPLEMENTATION_BEGIN(__NAME__) namespace {
#define XL_OBJC_IMPLEMENTATION_END(__NAME__) }

#endif

XL_OBJC_INTERFACE_FORWARD(XLMacAppDelegate);
XL_OBJC_INTERFACE_FORWARD(XLMacViewController);
XL_OBJC_INTERFACE_FORWARD(XLMacView);
XL_OBJC_INTERFACE_FORWARD(NSTimer);
XL_OBJC_INTERFACE_FORWARD(NSEvent);
XL_OBJC_INTERFACE_FORWARD(NSWindow);
XL_OBJC_INTERFACE_FORWARD(NSNotification);
XL_OBJC_INTERFACE_FORWARD(CAMetalLayer);


namespace stappler::xenolith::platform {

enum class ApplicationFlags {
	None,
	GuiApplication = 1 << 0,
};

SP_DEFINE_ENUM_AS_MASK(ApplicationFlags)


core::InputModifier getInputModifiers(uint32_t flags);

core::InputMouseButton getInputMouseButton(int32_t buttonNumber);

void createKeyTables(core::InputKeyCode keycodes[256], uint16_t scancodes[stappler::toInt(core::InputKeyCode::Max)]);


void * _Nonnull initMacApplication(ApplicationFlags);

bool runMacApplication();

void stopMacApplication();


class MacViewController : public Ref {
public:
	static MacViewController * _Nonnull makeConroller(ViewInterface * _Nonnull);

	virtual ~MacViewController() = default;

	MacViewController(XLMacViewController * _Nonnull, ViewInterface * _Nonnull, NSWindow * _Nonnull);

	void setTitle(StringView);
	void setDisplayLinkCallback(Function<void()> &&);
	void setVSyncEnabled(bool);
	void mapWindow();
	void wakeup();

	const CAMetalLayer * _Nonnull getLayer() const;
	float getLayerDensity() const;

	void viewDidLoad(const Callback<void()> &);
	void viewDidAppear(const Callback<void()> &);
	void viewWillDisappear(const Callback<void()> &);
	void viewDidDisappear(const Callback<void()> &);
	void viewDidChangeBackingProperties(const Callback<void()> &);

	XLMacView * _Nonnull loadView();

	CGSize windowWillResize(NSWindow * _Nonnull, CGSize frameSize);
	void windowDidResize(NSNotification * _Nonnull);

	void windowWillStartLiveResize(NSNotification * _Nonnull);
	void windowDidEndLiveResize(NSNotification * _Nonnull);

	void mouseDown(NSEvent * _Nonnull);
	void rightMouseDown(NSEvent * _Nonnull);
	void otherMouseDown(NSEvent * _Nonnull);
	void mouseUp(NSEvent * _Nonnull);
	void rightMouseUp(NSEvent * _Nonnull);
	void otherMouseUp(NSEvent * _Nonnull);
	void mouseMoved(NSEvent * _Nonnull);
	void mouseDragged(NSEvent * _Nonnull);
	void scrollWheel(NSEvent * _Nonnull);
	void rightMouseDragged(NSEvent * _Nonnull);
	void otherMouseDragged(NSEvent * _Nonnull);
	void mouseEntered(NSEvent * _Nonnull);
	void mouseExited(NSEvent * _Nonnull);
	void keyDown(NSEvent * _Nonnull);
	void keyUp(NSEvent * _Nonnull);
	void flagsChanged(NSEvent * _Nonnull);

	BOOL becomeFirstResponder(const Callback<BOOL()> &super);
	BOOL resignFirstResponder(const Callback<BOOL()> &super);

	void windowDidBecomeKey(NSNotification * _Nonnull);
	void windowDidResignKey(NSNotification * _Nonnull);

	void updateEngineView(bool displayLink = false);
	void handleDisplayLink();

	void submitTextData(WideStringView str, core::TextCursor cursor, core::TextCursor marked);

protected:
	XLMacViewController * _Nonnull _self = nil;
	NSTimer * _Nullable _updateTimer = nil;
	NSWindow * _Nullable _window = nil;
	CVDisplayLinkRef _Nullable _displayLink = nullptr;
	ViewInterface * _Nonnull _view = nullptr;
	CGSize _currentSize;
	CGPoint _currentPointerLocation;
	core::InputModifier _currentModifiers = core::InputModifier::None;
	Function<void()> _displayLinkCallback;
	core::InputKeyCode _keycodes[256];
	uint16_t _scancodes[128];
};

}

#if __OBJC__

#import <Cocoa/Cocoa.h>
#import <AppKit/NSEvent.h>

XL_OBJC_INTERFACE_BEGIN(XLMacAppDelegate) : NSObject <NSApplicationDelegate> {
	NSWindow *_window;
};

XL_OBJC_INTERFACE_END(XLMacAppDelegate)


XL_OBJC_INTERFACE_BEGIN(XLMacViewController) : NSViewController <NSWindowDelegate> {
	stappler::Rc<stappler::xenolith::platform::MacViewController> _controller;
};

- (instancetype _Nonnull) initWithView: (stappler::xenolith::platform::ViewInterface * _Nonnull)view window: (NSWindow * _Nonnull)window;

- (stappler::xenolith::platform::MacViewController * _Nonnull) engineController;

XL_OBJC_INTERFACE_END(XLMacViewController)


XL_OBJC_INTERFACE_BEGIN(XLMacView) : NSView <NSTextInputClient> {
	NSArray<NSAttributedStringKey> *_validAttributesForMarkedText;
	bool _textDirty;
};

- (void)updateTextCursorWithPosition:(uint32_t)pos length:(uint32_t)len;

- (void)updateTextInputWithText:(stappler::WideStringView)str position:(uint32_t)pos length:(uint32_t)len type:(stappler::xenolith::core::TextInputType)type;

- (void)runTextInputWithText:(stappler::WideStringView)str position:(uint32_t)pos length:(uint32_t)len type:(stappler::xenolith::core::TextInputType)type;

- (void)cancelTextInput;

XL_OBJC_INTERFACE_END(XLMacView)

#endif // __OBJC__

#endif

#endif /* XENOLITH_PLATFORM_MACOS_XLPLATFORMMACOS_H_ */
