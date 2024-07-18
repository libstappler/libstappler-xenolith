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

#include "XLCommon.h"

#import <Cocoa/Cocoa.h>

@interface XLMacAppDelegate : NSObject <NSApplicationDelegate> {
	NSWindow *_window;
}



@end

namespace stappler::xenolith::platform {

static XLMacAppDelegate *s_delegate = nullptr;

void * initMacApplication() {
	if (!s_delegate) {
		s_delegate = [[XLMacAppDelegate alloc] init];
	}
	
	auto app = [NSApplication sharedApplication];
	
	if (!app.delegate) {
		app.delegate = s_delegate;
		
		[NSThread detachNewThreadSelector:@selector(doNothing:)
						   toTarget:s_delegate
						 withObject:nil];
	}
	
	return (__bridge void *)s_delegate;
}

bool runMacApplication() {
	if (![[NSRunningApplication currentApplication] isFinishedLaunching]) {
		[NSApp run];
		return true;
	}
	return false;
}

void stopMacApplication() {
	if (s_delegate) {
		[NSApp stop:s_delegate];
		[NSApp abortModal];
	}
}

}

@implementation XLMacAppDelegate

- (instancetype _Nonnull) init {
	stappler::memory::pool::initialize();

	return self;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
	return YES;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
	stappler::log::info("XLMacAppDelegate", "applicationDidFinishLaunching");
}

- (void)applicationWillTerminate:(NSNotification *)notification {
	stappler::memory::pool::terminate();
}

- (void)doNothing:(id)object { }

@end
