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
#include "XLPlatformMacosObjc.h"

#include "AppKit/NSEvent.h"

#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>

namespace stappler::xenolith::platform {

MacViewController::~MacViewController() { }

MacViewController::MacViewController(ViewInterface *v) : _view(v) {
	createKeyTables(_keycodes, _scancodes);
}

void MacViewController::setTitle(StringView title) {
	_window.title = XL_OBJC_CALL([NSString stringWithUTF8String:title.data()]);
}

void MacViewController::setInfo(MacViewInfo &&info) { _info = move(info); }

void MacViewController::setVSyncEnabled(bool val) {
	auto l = (CAMetalLayer *)_self.view.layer;
	l.displaySyncEnabled = val ? YES : NO;
}

void MacViewController::initWindow() {
	auto extent = _view->getExtent();
	_window = XL_OBJC_CALL([[NSWindow alloc]
			initWithContentRect:NSRect {
				{0.0f, 0.0f}, {
					static_cast<CGFloat>(extent.width), static_cast<CGFloat>(extent.height)
				}
			}
					  styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
			| NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
						backing:NSBackingStoreBuffered
						  defer:YES]);

	_self = XL_OBJC_CALL([[XLMacViewController alloc] initWithController:this window:_window]);

	//window.title = [NSString stringWithUTF8String: view->getTitle().data()];
	_window.contentViewController = _self;
	_window.contentView = XL_OBJC_CALL(_window.contentViewController.view);
}

void MacViewController::mapWindow() {
	if (_window) {
		XL_OBJC_CALL([_window makeKeyAndOrderFront:nil]);
	}
}

void MacViewController::finalizeWindow() {
	CVDisplayLinkStop(_displayLink);
	_view = nullptr;
}

void MacViewController::wakeup() {
	dispatch_async(dispatch_get_main_queue(), ^{
	  if (_view) {
		  _view->update(false);
	  }
	});
}

const CAMetalLayer *MacViewController::getLayer() const {
	return (const CAMetalLayer *)XL_OBJC_CALL(_self.view.layer);
}

float MacViewController::getLayerDensity() const { return _self.view.layer.contentsScale; }

void MacViewController::viewDidLoad(const Callback<void()> &super) { super(); }

static CVReturn MacViewController_DisplayLinkCallback(CVDisplayLinkRef displayLink,
		const CVTimeStamp *now, const CVTimeStamp *outputTime, CVOptionFlags flagsIn,
		CVOptionFlags *flagsOut, void *target) {
	auto v = (MacViewController *)target;
	v->handleDisplayLink();
	return kCVReturnSuccess;
}

void MacViewController::viewDidAppear(const Callback<void()> &super) {
	super();

	_updateTimer = XL_OBJC_CALL([NSTimer scheduledTimerWithTimeInterval:(1.0 / 1000000.0)
			* double(xenolith::config::PresentationSchedulerInterval)
																 target:_self
															   selector:@selector(updateEngineView)
															   userInfo:nil
																repeats:NO]);

	CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);
	CVDisplayLinkSetOutputCallback(_displayLink, &MacViewController_DisplayLinkCallback, this);
	CVDisplayLinkStart(_displayLink);
}

void MacViewController::viewWillDisappear(const Callback<void()> &super) {
	XL_OBJC_CALL([_updateTimer invalidate]);
	_updateTimer = nil;

	super();
}

void MacViewController::viewDidDisappear(const Callback<void()> &super) {
	super();

	CVDisplayLinkStop(_displayLink);
	CVDisplayLinkRelease(_displayLink);

	//_engineView->threadDispose();
	//_engineView->setOsView(nullptr);
	//_engineView = nullptr;
}

XLMacView *MacViewController::loadView() {
	auto extent = _view->getExtent();

	XLMacView *view = XL_OBJC_CALL(
			[[XLMacView alloc] initWithFrame:NSRect{{CGFloat(0.0f), CGFloat(0.0f)},
												 {CGFloat(extent.width), CGFloat(extent.height)}}]);

	view.wantsLayer = YES;

	_currentSize = CGSizeMake(extent.width, extent.height);

	return view;
}

CGSize MacViewController::windowWillResize(NSWindow *sender, CGSize frameSize) { return frameSize; }

void MacViewController::windowDidResize(NSNotification *notification) {
	NSSize size = _self.view.window.contentLayoutRect.size;
	_view->setReadyForNextFrame();
	if (_currentSize.width != size.width || _currentSize.height != size.height) {
		_currentSize = size;
		_view->deprecateSwapchain(false);
		_view->update(true);
	}
}

void MacViewController::windowWillStartLiveResize(NSNotification *notification) {
	_info.captureView(_view);
}

void MacViewController::windowDidEndLiveResize(NSNotification *notification) {
	_info.releaseView(_view);
}

BOOL MacViewController::windowShouldClose(NSWindow *_Nonnull) {
	_view->end();
	return NO;
}

void MacViewController::mouseDown(NSEvent *theEvent) {
	CGPoint loc = CGPoint(XL_OBJC_CALL(
			[_self.view convertPointToBacking:[_self.view convertPoint:theEvent.locationInWindow
															  fromView:nil]]));
	core::InputEventData event({static_cast<uint32_t>(theEvent.buttonNumber),
		core::InputEventName::Begin, core::InputMouseButton::MouseLeft,
		getInputModifiers(uint32_t(theEvent.modifierFlags)), float(loc.x), float(loc.y)});

	_view->handleInputEvent(event);
	_currentPointerLocation = loc;
}

void MacViewController::rightMouseDown(NSEvent *theEvent) {
	CGPoint loc = CGPoint(XL_OBJC_CALL(
			[_self.view convertPointToBacking:[_self.view convertPoint:theEvent.locationInWindow
															  fromView:nil]]));
	core::InputEventData event({static_cast<uint32_t>(theEvent.buttonNumber),
		core::InputEventName::Begin, core::InputMouseButton::MouseRight,
		getInputModifiers(uint32_t(theEvent.modifierFlags)), float(loc.x), float(loc.y)});

	_view->handleInputEvent(event);
	_currentPointerLocation = loc;
}

void MacViewController::otherMouseDown(NSEvent *theEvent) {
	CGPoint loc = CGPoint(XL_OBJC_CALL(
			[_self.view convertPointToBacking:[_self.view convertPoint:theEvent.locationInWindow
															  fromView:nil]]));
	core::InputEventData event({static_cast<uint32_t>(theEvent.buttonNumber),
		core::InputEventName::Begin, getInputMouseButton(uint32_t(theEvent.buttonNumber)),
		getInputModifiers(uint32_t(theEvent.modifierFlags)), float(loc.x), float(loc.y)});

	_view->handleInputEvent(event);
	_currentPointerLocation = loc;
}

void MacViewController::mouseUp(NSEvent *theEvent) {
	CGPoint loc = CGPoint(XL_OBJC_CALL(
			[_self.view convertPointToBacking:[_self.view convertPoint:theEvent.locationInWindow
															  fromView:nil]]));
	core::InputEventData event({static_cast<uint32_t>(theEvent.buttonNumber),
		core::InputEventName::End, core::InputMouseButton::MouseLeft,
		getInputModifiers(uint32_t(theEvent.modifierFlags)), float(loc.x), float(loc.y)});

	_view->handleInputEvent(event);
	_currentPointerLocation = loc;
}

void MacViewController::rightMouseUp(NSEvent *theEvent) {
	CGPoint loc = CGPoint(XL_OBJC_CALL(
			[_self.view convertPointToBacking:[_self.view convertPoint:theEvent.locationInWindow
															  fromView:nil]]));
	core::InputEventData event({static_cast<uint32_t>(theEvent.buttonNumber),
		core::InputEventName::End, core::InputMouseButton::MouseRight,
		getInputModifiers(uint32_t(theEvent.modifierFlags)), float(loc.x), float(loc.y)});

	_view->handleInputEvent(event);
	_currentPointerLocation = loc;
}

void MacViewController::otherMouseUp(NSEvent *theEvent) {
	CGPoint loc = CGPoint(XL_OBJC_CALL(
			[_self.view convertPointToBacking:[_self.view convertPoint:theEvent.locationInWindow
															  fromView:nil]]));
	core::InputEventData event({static_cast<uint32_t>(theEvent.buttonNumber),
		core::InputEventName::End, getInputMouseButton(uint32_t(theEvent.buttonNumber)),
		getInputModifiers(uint32_t(theEvent.modifierFlags)), float(loc.x), float(loc.y)});

	_view->handleInputEvent(event);
	_currentPointerLocation = loc;
}

void MacViewController::mouseMoved(NSEvent *theEvent) {
	CGPoint loc = CGPoint(XL_OBJC_CALL(
			[_self.view convertPointToBacking:[_self.view convertPoint:theEvent.locationInWindow
															  fromView:nil]]));
	core::InputEventData event({std::numeric_limits<uint32_t>::max(),
		core::InputEventName::MouseMove, core::InputMouseButton::None,
		getInputModifiers(uint32_t(theEvent.modifierFlags)), float(loc.x), float(loc.y)});

	_view->handleInputEvent(event);
	_currentPointerLocation = loc;
}

void MacViewController::mouseDragged(NSEvent *theEvent) {
	CGPoint loc = CGPoint(XL_OBJC_CALL(
			[_self.view convertPointToBacking:[_self.view convertPoint:theEvent.locationInWindow
															  fromView:nil]]));

	Vector<core::InputEventData> events;

	events.emplace_back(core::InputEventData({static_cast<uint32_t>(theEvent.buttonNumber),
		core::InputEventName::Move, core::InputMouseButton::MouseLeft,
		getInputModifiers(uint32_t(theEvent.modifierFlags)), float(loc.x), float(loc.y)}));
	events.emplace_back(core::InputEventData({std::numeric_limits<uint32_t>::max(),
		core::InputEventName::MouseMove, core::InputMouseButton::None,
		getInputModifiers(uint32_t(theEvent.modifierFlags)), float(loc.x), float(loc.y)}));

	_view->handleInputEvents(move(events));
	_currentPointerLocation = loc;
}

void MacViewController::scrollWheel(NSEvent *theEvent) {
	CGPoint loc = CGPoint(XL_OBJC_CALL(
			[_self.view convertPointToBacking:[_self.view convertPoint:theEvent.locationInWindow
															  fromView:nil]]));

	Vector<core::InputEventData> events;

	uint32_t buttonId = 0;
	core::InputMouseButton buttonName;
	core::InputModifier mods =
			getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	if (theEvent.scrollingDeltaY != 0) {
		if (theEvent.scrollingDeltaY > 0) {
			buttonName = core::InputMouseButton::MouseScrollUp;
		} else {
			buttonName = core::InputMouseButton::MouseScrollDown;
		}
	} else {
		if (theEvent.scrollingDeltaX > 0) {
			buttonName = core::InputMouseButton::MouseScrollRight;
		} else {
			buttonName = core::InputMouseButton::MouseScrollLeft;
		}
	}

	buttonId = stappler::maxOf<uint32_t>() - stappler::toInt(buttonName);

	events.emplace_back(core::InputEventData(
			{buttonId, core::InputEventName::Begin, buttonName, mods, float(loc.x), float(loc.y)}));
	events.emplace_back(core::InputEventData({buttonId, core::InputEventName::Scroll, buttonName,
		mods, float(loc.x), float(loc.y)}));
	events.emplace_back(core::InputEventData(
			{buttonId, core::InputEventName::End, buttonName, mods, float(loc.x), float(loc.y)}));

	events.at(1).point.valueX = theEvent.scrollingDeltaX;
	events.at(1).point.valueY = theEvent.scrollingDeltaY;
	events.at(1).point.density = 1.0f;

	_view->handleInputEvents(move(events));
	_currentPointerLocation = loc;
}

void MacViewController::rightMouseDragged(NSEvent *theEvent) {
	CGPoint loc = CGPoint(XL_OBJC_CALL(
			[_self.view convertPointToBacking:[_self.view convertPoint:theEvent.locationInWindow
															  fromView:nil]]));
	core::InputEventData event({static_cast<uint32_t>(theEvent.buttonNumber),
		core::InputEventName::Move, core::InputMouseButton::MouseRight,
		getInputModifiers(uint32_t(theEvent.modifierFlags)), float(loc.x), float(loc.y)});

	_view->handleInputEvent(event);
	_currentPointerLocation = loc;
}

void MacViewController::otherMouseDragged(NSEvent *theEvent) {
	CGPoint loc = CGPoint(XL_OBJC_CALL(
			[_self.view convertPointToBacking:[_self.view convertPoint:theEvent.locationInWindow
															  fromView:nil]]));
	core::InputEventData event({static_cast<uint32_t>(theEvent.buttonNumber),
		core::InputEventName::Move, getInputMouseButton(uint32_t(theEvent.buttonNumber)),
		getInputModifiers(uint32_t(theEvent.modifierFlags)), float(loc.x), float(loc.y)});

	_view->handleInputEvent(event);
	_currentPointerLocation = loc;
}

void MacViewController::mouseEntered(NSEvent *theEvent) {
	CGPoint loc = CGPoint(XL_OBJC_CALL(
			[_self.view convertPointToBacking:[_self.view convertPoint:theEvent.locationInWindow
															  fromView:nil]]));

	_view->handleInputEvent(core::InputEventData::BoolEvent(core::InputEventName::PointerEnter,
			true, Vec2(loc.x, loc.y)));
	_currentPointerLocation = loc;
}

void MacViewController::mouseExited(NSEvent *theEvent) {
	CGPoint loc = CGPoint(XL_OBJC_CALL(
			[_self.view convertPointToBacking:[_self.view convertPoint:theEvent.locationInWindow
															  fromView:nil]]));

	_view->handleInputEvent(core::InputEventData::BoolEvent(core::InputEventName::PointerEnter,
			false, Vec2(loc.x, loc.y)));
	_currentPointerLocation = loc;
}

BOOL MacViewController::becomeFirstResponder(const Callback<BOOL()> &super) { return super(); }

BOOL MacViewController::resignFirstResponder(const Callback<BOOL()> &super) { return super(); }

void MacViewController::windowDidBecomeKey(NSNotification *theEvent) {
	_view->handleInputEvent(core::InputEventData::BoolEvent(core::InputEventName::FocusGain, true,
			Vec2(_currentPointerLocation.x, _currentPointerLocation.y)));
}

void MacViewController::windowDidResignKey(NSNotification *theEvent) {
	_view->handleInputEvent(core::InputEventData::BoolEvent(core::InputEventName::FocusGain, false,
			Vec2(_currentPointerLocation.x, _currentPointerLocation.y)));
}

void MacViewController::viewDidChangeBackingProperties(const Callback<void()> &super) {
	CGSize viewScale = XL_OBJC_CALL([_self.view convertSizeToBacking:CGSizeMake(1.0, 1.0)]);
	//self.layer.contentsScale = MIN(viewScale.width, viewScale.height);
	std::cout << viewScale.width << " " << viewScale.height << "\n";

	super();
}

void MacViewController::keyDown(NSEvent *theEvent) {
	auto code = XL_OBJC_CALL([theEvent keyCode]);

	core::InputEventData event({static_cast<uint32_t>(code),
		theEvent.isARepeat ? core::InputEventName::KeyRepeated : core::InputEventName::KeyPressed,
		getInputMouseButton(uint32_t(theEvent.buttonNumber)),
		getInputModifiers(uint32_t(theEvent.modifierFlags)), float(_currentPointerLocation.x),
		float(_currentPointerLocation.y)});

	event.key.keycode = _keycodes[static_cast<uint8_t>(code)];
	event.key.compose = core::InputKeyComposeState::Disabled;
	event.key.keysym = 0;
	event.key.keychar = 0;
	_view->handleInputEvent(event);

	std::cout << (theEvent.isARepeat ? "keyRepeat: " : "keyDown: ") << code << " "
			  << getInputKeyCodeName(event.key.keycode) << "\n";
}

void MacViewController::keyUp(NSEvent *theEvent) {
	auto code = XL_OBJC_CALL([theEvent keyCode]);

	core::InputEventData event({static_cast<uint32_t>(code), core::InputEventName::KeyReleased,
		getInputMouseButton(uint32_t(theEvent.buttonNumber)),
		getInputModifiers(uint32_t(theEvent.modifierFlags)), float(_currentPointerLocation.x),
		float(_currentPointerLocation.y)});

	event.key.keycode = _keycodes[static_cast<uint8_t>(code)];
	event.key.compose = core::InputKeyComposeState::Disabled;
	event.key.keysym = 0;
	event.key.keychar = 0;
	_view->handleInputEvent(event);

	std::cout << "keyUp: " << code << " " << getInputKeyCodeName(event.key.keycode) << "\n";
}

void MacViewController::flagsChanged(NSEvent *theEvent) {
	static std::pair<core::InputModifier, core::InputKeyCode> testmask[] = {
		std::make_pair(core::InputModifier::ShiftL, core::InputKeyCode::LEFT_SHIFT),
		std::make_pair(core::InputModifier::ShiftR, core::InputKeyCode::RIGHT_SHIFT),
		std::make_pair(core::InputModifier::CtrlL, core::InputKeyCode::LEFT_CONTROL),
		std::make_pair(core::InputModifier::CtrlR, core::InputKeyCode::RIGHT_CONTROL),
		std::make_pair(core::InputModifier::AltL, core::InputKeyCode::LEFT_ALT),
		std::make_pair(core::InputModifier::AltR, core::InputKeyCode::RIGHT_ALT),
		std::make_pair(core::InputModifier::WinL, core::InputKeyCode::LEFT_SUPER),
		std::make_pair(core::InputModifier::WinR, core::InputKeyCode::RIGHT_SUPER),
		std::make_pair(core::InputModifier::CapsLock, core::InputKeyCode::CAPS_LOCK),
		std::make_pair(core::InputModifier::NumLock, core::InputKeyCode::NUM_LOCK),
		std::make_pair(core::InputModifier::Mod5, core::InputKeyCode::WORLD_1),
		std::make_pair(core::InputModifier::Mod4, core::InputKeyCode::WORLD_2),
	};

	core::InputModifier mods = getInputModifiers(uint32_t(theEvent.modifierFlags));
	if (mods == _currentModifiers) {
		return;
	}

	auto diff = mods ^ _currentModifiers;

	core::InputEventData event({static_cast<uint32_t>(0), core::InputEventName::KeyReleased,
		core::InputMouseButton::None, mods, float(_currentPointerLocation.x),
		float(_currentPointerLocation.y)});

	for (auto &it : testmask) {
		if ((diff & it.first) != 0) {
			event.id = toInt(it.first);
			if ((mods & it.first) != 0) {
				event.event = core::InputEventName::KeyPressed;
			}
			event.key.keycode = it.second;
			event.key.compose = core::InputKeyComposeState::Disabled;
			event.key.keysym = 0;
			event.key.keychar = 0;
			break;
		}
	}

	if (event.id) {
		_view->handleInputEvent(event);
	}

	_currentModifiers = mods;
}

void MacViewController::updateEngineView(bool displayLink) { _view->update(displayLink); }

void MacViewController::handleDisplayLink() {
	// WARINIG - no thread control, display link MUST be disabled to modify callback
	_info.handleDisplayLink(_view);
}

void MacViewController::submitTextData(WideStringView str, core::TextCursor cursor,
		core::TextCursor marked) {
	//_engineView->submitTextData(str, cursor, marked);
}

} // namespace stappler::xenolith::platform

XL_OBJC_IMPLEMENTATION_BEGIN(XLMacAppDelegate)

- (instancetype _Nonnull)init {
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

- (void)doNothing:(id)object {
}

XL_OBJC_IMPLEMENTATION_END(XLMacAppDelegate)


XL_OBJC_IMPLEMENTATION_BEGIN(XLMacViewController)

- (instancetype _Nonnull)initWithController:
								 (stappler::xenolith::platform::MacViewController *)controller
									 window:(NSWindow *)window {
	self = [super init];
	_controller = controller;
	return self;
}

- (stappler::xenolith::platform::MacViewController *)engineController {
	return _controller;
}

- (void)viewDidLoad {
	if (_controller) {
		_controller->viewDidLoad([self] { [super viewDidLoad]; });
	}
}

- (void)viewDidAppear {
	if (_controller) {
		_controller->viewDidAppear([self] { [super viewDidAppear]; });
	}
}

- (void)viewWillDisappear {
	if (_controller) {
		_controller->viewWillDisappear([self] { [super viewWillDisappear]; });
	}
}

- (void)viewDidDisappear {
	if (_controller) {
		_controller->viewDidDisappear([self] { [super viewDidDisappear]; });
		_controller = nullptr;
	}
}

- (void)loadView {
	self.view = _controller->loadView();
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize {
	return frameSize;
}

- (void)windowDidResize:(NSNotification *)notification {
	if (_controller) {
		_controller->windowDidResize(notification);
	}
}

- (void)windowWillStartLiveResize:(NSNotification *)notification {
	if (_controller) {
		_controller->windowWillStartLiveResize(notification);
	}
}

- (void)windowDidEndLiveResize:(NSNotification *)notification {
	if (_controller) {
		_controller->windowDidEndLiveResize(notification);
	}
}

- (BOOL)windowShouldClose:(NSWindow *)sender {
	auto ret = _controller->windowShouldClose(sender);
	_controller = nullptr;
	return ret;
}

- (void)mouseDown:(NSEvent *)theEvent {
	if (_controller) {
		_controller->mouseDown(theEvent);
	}
}

- (void)rightMouseDown:(NSEvent *)theEvent {
	if (_controller) {
		_controller->rightMouseDown(theEvent);
	}
}

- (void)otherMouseDown:(NSEvent *)theEvent {
	if (_controller) {
		_controller->otherMouseDown(theEvent);
	}
}

- (void)mouseUp:(NSEvent *)theEvent {
	if (_controller) {
		_controller->mouseUp(theEvent);
	}
}

- (void)rightMouseUp:(NSEvent *)theEvent {
	if (_controller) {
		_controller->rightMouseUp(theEvent);
	}
}

- (void)otherMouseUp:(NSEvent *)theEvent {
	if (_controller) {
		_controller->otherMouseUp(theEvent);
	}
}

- (void)mouseMoved:(NSEvent *)theEvent {
	if (_controller) {
		_controller->mouseMoved(theEvent);
	}
}

- (void)mouseDragged:(NSEvent *)theEvent {
	if (_controller) {
		_controller->mouseDragged(theEvent);
	}
}

- (void)scrollWheel:(NSEvent *)theEvent {
	if (_controller) {
		_controller->scrollWheel(theEvent);
	}
}

- (void)rightMouseDragged:(NSEvent *)theEvent {
	if (_controller) {
		_controller->rightMouseDragged(theEvent);
	}
}

- (void)otherMouseDragged:(NSEvent *)theEvent {
	if (_controller) {
		_controller->otherMouseDragged(theEvent);
	}
}

- (void)mouseEntered:(NSEvent *)theEvent {
	if (_controller) {
		_controller->mouseEntered(theEvent);
	}
}

- (void)mouseExited:(NSEvent *)theEvent {
	if (_controller) {
		_controller->mouseExited(theEvent);
	}
}

- (BOOL)becomeFirstResponder {
	return _controller->becomeFirstResponder([self] { return [super becomeFirstResponder]; });
}

- (BOOL)resignFirstResponder {
	return _controller->resignFirstResponder([self] { return [super resignFirstResponder]; });
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
	if (_controller) {
		_controller->windowDidBecomeKey(notification);
	}
}

- (void)windowDidResignKey:(NSNotification *)notification {
	if (_controller) {
		_controller->windowDidResignKey(notification);
	}
}

- (void)viewDidChangeBackingProperties {
	if (_controller) {
		_controller->viewDidChangeBackingProperties(
				[self] { [self viewDidChangeBackingProperties]; });
	}
}

- (void)keyDown:(NSEvent *)theEvent {
	if (_controller) {
		_controller->keyDown(theEvent);
	}
}

- (void)keyUp:(NSEvent *)theEvent {
	if (_controller) {
		_controller->keyUp(theEvent);
	}
}

- (void)flagsChanged:(NSEvent *)theEvent {
	if (_controller) {
		_controller->flagsChanged(theEvent);
	}
}

- (void)updateEngineView {
	if (_controller) {
		_controller->updateEngineView(false);
	}
}

XL_OBJC_IMPLEMENTATION_END(XLMacViewController)


static const NSRange kEmptyRange = {NSNotFound, 0};

XL_OBJC_IMPLEMENTATION_BEGIN(XLMacView)

+ (Class)layerClass {
	return [CAMetalLayer class];
}

- (instancetype)initWithFrame:(NSRect)frameRect {
	self = [super initWithFrame:frameRect];
	_validAttributesForMarkedText = [NSArray array];
	/*_textInput = stappler::Rc<TextInputManager>::create(&_textInterface);
	_textDirty = false;
	_textHandler.onText = [self] (WideStringView str, TextCursor cursor, TextCursor marked) {
		_textDirty = true;
	};
	_textHandler.onInput = [] (bool) {

	};
	_textHandler.onEnded = [] () {

	};*/
	return self;
}

- (BOOL)wantsUpdateLayer {
	return YES;
}

- (CALayer *)makeBackingLayer {
	self.layer = [self.class.layerClass layer];

	CGSize viewScale = [self convertSizeToBacking:CGSizeMake(1.0, 1.0)];
	self.layer.contentsScale = MIN(viewScale.width, viewScale.height);
	return self.layer;
}

- (BOOL)layer:(CALayer *)layer
		shouldInheritContentsScale:(CGFloat)newScale
						fromWindow:(NSWindow *)window {
	std::cout << "shouldInheritContentsScale: " << newScale << "\n";
	return YES;
}

- (BOOL)acceptsFirstResponder {
	return YES;
}

- (void)viewDidMoveToWindow {
	auto trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
													 options:NSTrackingMouseMoved
			| NSTrackingActiveAlways | NSTrackingInVisibleRect | NSTrackingMouseEnteredAndExited
													   owner:self
													userInfo:nil];
	[self addTrackingArea:trackingArea];

	self.window.delegate = (XLMacViewController *)self.window.contentViewController;
}

- (void)keyDown:(NSEvent *)theEvent {
	//TextInputManager *m = _textInput.get();
	//if (m->isInputEnabled()) {
	//[self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];
	if (_textDirty) {
		//[(XLMacViewController *)self.window.contentViewController submitTextData:m->getString() cursor:m->getCursor() marked:m->getMarked()];
		_textDirty = false;
	}
	//}
	[super keyDown:theEvent];
}

- (void)deleteForward:(nullable id)sender {
	std::cout << "deleteForward\n";
	//TextInputManager *m = _textInput.get();
	//m->deleteForward();
}

- (void)deleteBackward:(nullable id)sender {
	std::cout << "deleteBackward\n";
	//TextInputManager *m = _textInput.get();
	//m->deleteBackward();
}

- (void)insertTab:(nullable id)sender {
	//TextInputManager *m = _textInput.get();
	//m->insertText(u"\t");
}

- (void)insertBacktab:(nullable id)sender {
}

- (void)insertNewline:(nullable id)sender {
	//TextInputManager *m = _textInput.get();
	//m->insertText(u"\n");
}

/* The receiver inserts string replacing the content specified by replacementRange. string can be either an NSString or NSAttributedString instance. */
- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
	NSString *characters;
	if ([string isKindOfClass:[NSAttributedString class]]) {
		characters = [(NSAttributedString *)string string];
	} else {
		characters = string;
	}

	stappler::xenolith::WideString str;
	str.reserve(characters.length);
	for (size_t i = 0; i < characters.length; ++i) {
		str.push_back([characters characterAtIndex:i]);
	}

	//TextInputManager *m = _textInput.get();
	//m->insertText(str, TextCursor(uint32_t(replacementRange.location), uint32_t(replacementRange.length)));
}

/* The receiver inserts string replacing the content specified by replacementRange. string can be either an NSString or NSAttributedString instance. selectedRange specifies the selection inside the string being inserted; hence, the location is relative to the beginning of string. When string is an NSString, the receiver is expected to render the marked text with distinguishing appearance (i.e. NSTextView renders with -markedTextAttributes). */
- (void)setMarkedText:(id)string
		   selectedRange:(NSRange)selectedRange
		replacementRange:(NSRange)replacementRange {
	std::cout << "setMarkedText\n";
	NSString *characters;
	if ([string isKindOfClass:[NSAttributedString class]]) {
		characters = [(NSAttributedString *)string string];
	} else {
		characters = string;
	}

	stappler::xenolith::WideString str;
	str.reserve(characters.length);
	for (size_t i = 0; i < characters.length; ++i) {
		str.push_back([characters characterAtIndex:i]);
	}

	//TextInputManager *m = _textInput.get();
	//m->setMarkedText(str, TextCursor(uint32_t(replacementRange.location), uint32_t(replacementRange.length)), TextCursor(uint32_t(selectedRange.location), uint32_t(selectedRange.length)));
}

/* The receiver unmarks the marked text. If no marked text, the invocation of this method has no effect. */
- (void)unmarkText {
	//TextInputManager *m = _textInput.get();
	//m->unmarkText();
}

/* Returns the selection range. The valid location is from 0 to the document length. */
- (NSRange)selectedRange {
	//TextInputManager *m = _textInput.get();
	//auto cursor = m->getCursor();
	//if (cursor.length > 0) {
	//	return NSRange{cursor.start, cursor.length};
	//}
	return kEmptyRange;
}

/* Returns the marked range. Returns {NSNotFound, 0} if no marked range. */
- (NSRange)markedRange {
	//TextInputManager *m = _textInput.get();
	//auto cursor = m->getMarked();
	//if (cursor.length > 0) {
	//	return NSRange{cursor.start, cursor.length};
	//}
	return kEmptyRange;
}

/* Returns whether or not the receiver has marked text. */
- (BOOL)hasMarkedText {
	//TextInputManager *m = _textInput.get();
	//auto cursor = m->getMarked();
	//if (cursor.length > 0) {
	//	return YES;
	//}
	return NO;
}

/* Returns attributed string specified by range. It may return nil. If non-nil return value and actualRange is non-NULL, it contains the actual range for the return value. The range can be adjusted from various reasons (i.e. adjust to grapheme cluster boundary, performance optimization, etc). */
- (nullable NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
														 actualRange:(nullable NSRangePointer)
																			 actualRange {
	std::cout << "attributedSubstringForProposedRange\n";
	//TextInputManager *m = _textInput.get();
	//WideStringView str = m->getStringByRange(TextCursor{uint32_t(range.location), uint32_t(range.length)});
	//if (actualRange != nil) {
	//	auto fullString = m->getString();

	//	actualRange->location = (str.data() - fullString.data());
	//	actualRange->length = str.size();
	//}

	//return [[NSAttributedString alloc] initWithString:[NSString stringWithCharacters:(const unichar *)str.data() length:str.size()]];
	return nil;
}

/* Returns an array of attribute names recognized by the receiver.
*/
- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText {
	return _validAttributesForMarkedText;
}

/* Returns the first logical rectangular area for range. The return value is in the screen coordinate. The size value can be negative if the text flows to the left. If non-NULL, actuallRange contains the character range corresponding to the returned area.
*/
- (NSRect)firstRectForCharacterRange:(NSRange)range
						 actualRange:(nullable NSRangePointer)actualRange {
	std::cout << "firstRectForCharacterRange\n";
	return self.frame;
}

/* Returns the index for character that is nearest to point. point is in the screen coordinate system.
*/
- (NSUInteger)characterIndexForPoint:(NSPoint)point {
	std::cout << "characterIndexForPoint\n";
	return 0;
}

- (NSAttributedString *)attributedString {
	std::cout << "attributedString\n";
	//TextInputManager *m = _textInput.get();
	//auto str = m->getString();

	//return [[NSAttributedString alloc] initWithString:[NSString stringWithCharacters:(const unichar *)str.data() length:str.size()]];
	return nil;
}

/* Returns the fraction of distance for point from the left side of the character. This allows caller to perform precise selection handling.
*/
- (CGFloat)fractionOfDistanceThroughGlyphForPoint:(NSPoint)point {
	std::cout << "fractionOfDistanceThroughGlyphForPoint\n";
	return 0.0f;
}

/* Returns the baseline position relative to the origin of rectangle returned by -firstRectForCharacterRange:actualRange:. This information allows the caller to access finer-grained character position inside the NSTextInputClient document.
*/
- (CGFloat)baselineDeltaForCharacterAtIndex:(NSUInteger)anIndex {
	std::cout << "baselineDeltaForCharacterAtIndex\n";
	return 0.0f;
}

/* Returns if the marked text is in vertical layout.
 */
- (BOOL)drawsVerticallyForCharacterAtIndex:(NSUInteger)charIndex {
	std::cout << "drawsVerticallyForCharacterAtIndex\n";
	return NO;
}

- (void)updateTextCursorWithPosition:(uint32_t)pos length:(uint32_t)len {
	//TextInputManager *m = _textInput.get();
	//m->cursorChanged(TextCursor(pos, len));
}

- (void)updateTextInputWithText:(stappler::WideStringView)str
					   position:(uint32_t)pos
						 length:(uint32_t)len
						   type:(stappler::xenolith::core::TextInputType)type {
	_textDirty = false;
	//TextInputManager *m = _textInput.get();
	//m->run(&_textHandler, str, TextCursor(pos, len), TextCursor::InvalidCursor, type);
}

- (void)runTextInputWithText:(stappler::WideStringView)str
					position:(uint32_t)pos
					  length:(uint32_t)len
						type:(stappler::xenolith::core::TextInputType)type {
	_textDirty = false;
	//TextInputManager *m = _textInput.get();
	//m->run(&_textHandler, str, TextCursor(pos, len), TextCursor::InvalidCursor, type);
	//m->setInputEnabled(true);
}

- (void)cancelTextInput {
	//TextInputManager *m = _textInput.get();
	//m->cancel();
	_textDirty = false;
}

XL_OBJC_IMPLEMENTATION_END(XLMacView)
