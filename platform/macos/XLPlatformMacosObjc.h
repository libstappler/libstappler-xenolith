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

#ifndef XENOLITH_PLATFORM_MACOS_XLPLATFORMMACOSOBJC_H_
#define XENOLITH_PLATFORM_MACOS_XLPLATFORMMACOSOBJC_H_

#include "XLPlatformMacos.h"

#if MACOS

#include <sys/cdefs.h>
#include <objc/objc.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CoreVideo.h>

#import <Cocoa/Cocoa.h>
#import <AppKit/NSEvent.h>

XL_OBJC_INTERFACE_BEGIN(XLMacAppDelegate) : NSObject <NSApplicationDelegate> {
	NSWindow *_window;
};

- (void)doNothing:(id _Nonnull)object;

XL_OBJC_INTERFACE_END(XLMacAppDelegate)


XL_OBJC_INTERFACE_BEGIN(XLMacViewController) : NSViewController <NSWindowDelegate> {
	stappler::Rc<stappler::xenolith::platform::MacViewController> _controller;
};

- (instancetype _Nonnull) initWithController: (stappler::xenolith::platform::MacViewController * _Nonnull)constroller window: (NSWindow * _Nonnull)window;

- (stappler::xenolith::platform::MacViewController * _Nonnull) engineController;

- (void)updateEngineView;

XL_OBJC_INTERFACE_END(XLMacViewController)


XL_OBJC_INTERFACE_BEGIN(XLMacView) : NSView <NSTextInputClient, NSViewLayerContentScaleDelegate> {
	NSArray<NSAttributedStringKey> *_validAttributesForMarkedText;
	bool _textDirty;
};

- (void)updateTextCursorWithPosition:(uint32_t)pos length:(uint32_t)len;

- (void)updateTextInputWithText:(stappler::WideStringView)str position:(uint32_t)pos length:(uint32_t)len type:(stappler::xenolith::core::TextInputType)type;

- (void)runTextInputWithText:(stappler::WideStringView)str position:(uint32_t)pos length:(uint32_t)len type:(stappler::xenolith::core::TextInputType)type;

- (void)cancelTextInput;

XL_OBJC_INTERFACE_END(XLMacView)

#endif // MACOS

#endif // XENOLITH_PLATFORM_MACOS_XLPLATFORMMACOSOBJC_H_
