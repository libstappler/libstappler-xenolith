/**
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

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CADisplayLink.h>

#include "XLMacosWindow.h"
#include "XLAppWindow.h"
#include "platform/XLContextController.h"

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkSwapchain.h"
#endif

@interface XLMacosViewController : NSViewController <NSWindowDelegate> {
	NSXLPL::MacosWindow *_engineWindow;
	CADisplayLink *_displayLink;
	CGSize _currentSize;
	CGSize _viewScale;
	CGPoint _currentPointerLocation;
	NSXL::core::InputModifier _currentModifiers;
	NSXL::core::InputKeyCode _keycodes[256];
	uint16_t _scancodes[128];
};

- (instancetype _Nonnull)init:(NSSP::NotNull<NSXLPL::MacosWindow>)constroller
					   window:(NSWindow *_Nonnull)window;

@end

@interface XLMacosView : NSView <NSTextInputClient, NSViewLayerContentScaleDelegate> {
	NSXLPL::MacosWindow *_window;
	NSArray<NSAttributedStringKey> *_validAttributesForMarkedText;
	bool _textDirty;

	NSTrackingArea *_mainArea;
	NSArray<NSTrackingArea *> *_cursorAreas;
};

- (instancetype)initWithFrame:(NSRect)frameRect window:(NSXLPL::MacosWindow *)window;

- (void)updateTextCursorWithPosition:(uint32_t)pos length:(uint32_t)len;

- (void)updateTextInputWithText:(stappler::WideStringView)str
					   position:(uint32_t)pos
						 length:(uint32_t)len
						   type:(stappler::xenolith::core::TextInputType)type;

- (void)runTextInputWithText:(stappler::WideStringView)str
					position:(uint32_t)pos
					  length:(uint32_t)len
						type:(stappler::xenolith::core::TextInputType)type;

- (void)cancelTextInput;

@end

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

MacosWindow::~MacosWindow() {
	if (_controller && _isRootWindow) {
		_controller->handleRootWindowClosed();
	}

	_rootView = nullptr;
	_rootViewController = nullptr;
	if (_window) {
		_window = nullptr;
	}
}

bool MacosWindow::init(NotNull<ContextController> controller, Rc<WindowInfo> &&info,
		bool isRootWindow) {
	WindowCapabilities caps =
			WindowCapabilities::Fullscreen | WindowCapabilities::ServerSideDecorations;

	if (!NativeWindow::init(controller, sp::move(info), caps, isRootWindow)) {
		return false;
	}

	auto style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
			| NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

	auto rect = NSRect{
		{static_cast<CGFloat>(_info->rect.x), static_cast<CGFloat>(_info->rect.y)},
		{static_cast<CGFloat>(_info->rect.width), static_cast<CGFloat>(_info->rect.height)},
	};

	_window = [[NSWindow alloc] initWithContentRect:rect
										  styleMask:style
											backing:NSBackingStoreBuffered
											  defer:YES];
	[_window setReleasedWhenClosed:false];
	_rootViewController = [[XLMacosViewController alloc] init:this window:_window];
	_window.contentViewController = _rootViewController;
	_window.contentView = _window.contentViewController.view;
	_rootView = (XLMacosView *)_window.contentView;
	_initialized = true;

	if (_windowLoaded) {
		_controller->notifyWindowCreated(this);
		[_window makeKeyAndOrderFront:_rootViewController];
	}
	return true;
}

void MacosWindow::mapWindow() { }

void MacosWindow::unmapWindow() {
	_rootView = nullptr;
	_rootViewController = nullptr;
}

bool MacosWindow::close() {
	if (!_controller->notifyWindowClosed(this)) {
		return false;
	}

	if (_window) {
		[_window close];
	}
	return true;
}

void MacosWindow::handleFramePresented(NotNull<core::PresentationFrame>) { }

void MacosWindow::handleLayerUpdate(const WindowLayer &layer) {
	/*auto cursor = layer.flags & WindowLayerFlags::CursorMask;
	if (cursor != _currentCursor) {
		NSCursor *targetCursor = nullptr;
		switch (cursor) {
		case NSXL::WindowLayerFlags::CursorDefault: targetCursor = [NSCursor arrowCursor]; break;
		case NSXL::WindowLayerFlags::CursorContextMenu:
			targetCursor = [NSCursor contextualMenuCursor];
			break;
		case NSXL::WindowLayerFlags::CursorHelp: targetCursor = [NSCursor arrowCursor]; break;
		case NSXL::WindowLayerFlags::CursorPointer:
			targetCursor = [NSCursor pointingHandCursor];
			break;
		case NSXL::WindowLayerFlags::CursorProgress: break;
		case NSXL::WindowLayerFlags::CursorWait: break;
		case NSXL::WindowLayerFlags::CursorCell: break;
		case NSXL::WindowLayerFlags::CursorCrosshair:
			targetCursor = [NSCursor crosshairCursor];
			break;
		case NSXL::WindowLayerFlags::CursorText: targetCursor = [NSCursor IBeamCursor]; break;
		case NSXL::WindowLayerFlags::CursorVerticalText:
			targetCursor = [NSCursor IBeamCursorForVerticalLayout];
			break;
		case NSXL::WindowLayerFlags::CursorAlias: targetCursor = [NSCursor dragLinkCursor]; break;
		case NSXL::WindowLayerFlags::CursorCopy: targetCursor = [NSCursor dragCopyCursor]; break;
		case NSXL::WindowLayerFlags::CursorMove: break;
		case NSXL::WindowLayerFlags::CursorNoDrop:
			targetCursor = [NSCursor operationNotAllowedCursor];
			break;
		case NSXL::WindowLayerFlags::CursorNotAllowed:
			targetCursor = [NSCursor operationNotAllowedCursor];
			break;
		case NSXL::WindowLayerFlags::CursorGrab: targetCursor = [NSCursor openHandCursor]; break;
		case NSXL::WindowLayerFlags::CursorGrabbing:
			targetCursor = [NSCursor closedHandCursor];
			break;

		case NSXL::WindowLayerFlags::CursorAllScroll: break;
		case NSXL::WindowLayerFlags::CursorZoomIn: targetCursor = [NSCursor zoomInCursor]; break;
		case NSXL::WindowLayerFlags::CursorZoomOut: targetCursor = [NSCursor zoomOutCursor]; break;
		case NSXL::WindowLayerFlags::CursorDndAsk: break;

		case NSXL::WindowLayerFlags::CursorRightPtr: break;
		case NSXL::WindowLayerFlags::CursorPencil: break;
		case NSXL::WindowLayerFlags::CursorTarget: break;

		case NSXL::WindowLayerFlags::CursorResizeRight:
			targetCursor = [NSCursor resizeRightCursor];
			break;
		case NSXL::WindowLayerFlags::CursorResizeTop:
			targetCursor = [NSCursor resizeUpCursor];
			break;
		case NSXL::WindowLayerFlags::CursorResizeTopRight: break;
		case NSXL::WindowLayerFlags::CursorResizeTopLeft: break;
		case NSXL::WindowLayerFlags::CursorResizeBottom:
			targetCursor = [NSCursor resizeDownCursor];
			break;
		case NSXL::WindowLayerFlags::CursorResizeBottomRight: break;
		case NSXL::WindowLayerFlags::CursorResizeBottomLeft: break;
		case NSXL::WindowLayerFlags::CursorResizeLeft:
			targetCursor = [NSCursor resizeLeftCursor];
			break;
		case NSXL::WindowLayerFlags::CursorResizeLeftRight:
			targetCursor = [NSCursor resizeLeftRightCursor];
			break;
		case NSXL::WindowLayerFlags::CursorResizeTopBottom:
			targetCursor = [NSCursor resizeUpDownCursor];
			break;
		case NSXL::WindowLayerFlags::CursorResizeTopRightBottomLeft: break;
		case NSXL::WindowLayerFlags::CursorResizeTopLeftBottomRight: break;
		case NSXL::WindowLayerFlags::CursorResizeCol:
			targetCursor = [NSCursor columnResizeCursor];
			break;
		case NSXL::WindowLayerFlags::CursorResizeRow:
			targetCursor = [NSCursor rowResizeCursor];
			break;
		case NSXL::WindowLayerFlags::CursorResizeAll: break;
		default: break;
		}

		if (targetCursor) {
			[targetCursor set];
			_currentCursor = cursor;
		} else {
			if (_currentCursor != WindowLayerFlags::None) {
				[[NSCursor arrowCursor] set];
				_currentCursor = WindowLayerFlags::None;
			}
		}
	}*/
	NativeWindow::handleLayerUpdate(layer);
}

core::SurfaceInfo MacosWindow::getSurfaceOptions(core::SurfaceInfo &&info) const {
	return sp::move(info);
}

core::FrameConstraints MacosWindow::exportConstraints(core::FrameConstraints &&c) const {
	core::FrameConstraints constraints = sp::move(c);

	if (constraints.density == 0.0f) {
		constraints.density = 1.0f;
	}

	constraints.density *= _window.backingScaleFactor;

	return constraints;
}

Extent2 MacosWindow::getExtent() const {
	auto size = _window.frame.size;
	size.width *= _window.backingScaleFactor;
	size.height *= _window.backingScaleFactor;
	return Extent2(static_cast<uint32_t>(size.width), static_cast<uint32_t>(size.height));
}

Rc<core::Surface> MacosWindow::makeSurface(NotNull<core::Instance> cinstance) {
#if MODULE_XENOLITH_BACKEND_VK
	if (cinstance->getApi() != core::InstanceApi::Vulkan) {
		return nullptr;
	}

	auto instance = static_cast<vk::Instance *>(cinstance.get());

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkMetalSurfaceCreateInfoEXT createInfo{VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT, nullptr,
		0, (CAMetalLayer *)_rootView.layer};
	if (instance->vkCreateMetalSurfaceEXT(instance->getInstance(), &createInfo, nullptr, &surface)
			!= VK_SUCCESS) {
		return nullptr;
	}
	return Rc<vk::Surface>::create(instance, surface, this);
#else
	log::error("XcbWindow", "No available gAPI found for a surface");
	return nullptr;
#endif
}

core::PresentationOptions MacosWindow::getPreferredOptions() const {
	core::PresentationOptions opts;
	opts.followDisplayLinkBarrier = true;
	return opts;
}

void MacosWindow::handleWindowLoaded() {
	_windowLoaded = true;
	if (_initialized) {
		_controller->notifyWindowCreated(this);
		[_window makeKeyAndOrderFront:_rootViewController];
	}
}

void MacosWindow::handleDisplayLink() {
	if (_appWindow) {
		_appWindow->update(core::PresentationUpdateFlags::DisplayLink);
	}
}

bool MacosWindow::updateTextInput(const TextInputRequest &req, TextInputFlags flags) {
	return false;
}

void MacosWindow::cancelTextInput() { }

} // namespace stappler::xenolith::platform

@implementation XLMacosViewController

- (void)handleDisplayLink:(CADisplayLink *)obj {
	if (_engineWindow) {
		_engineWindow->handleDisplayLink();
	}
}

- (instancetype _Nonnull)init:(NSSP::NotNull<NSXLPL::MacosWindow>)w
					   window:(NSWindow *_Nonnull)window {
	self = [super init];
	_engineWindow = w;

	_currentSize = CGSize{0, 0};
	_currentPointerLocation = CGPoint{0, 0};
	_currentModifiers = NSXL::core::InputModifier::None;

	window.delegate = self;

	_displayLink = [window displayLinkWithTarget:self selector:@selector(handleDisplayLink:)];
	[_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
	_displayLink.paused = YES;

	NSXLPL::createKeyTables(_keycodes, _scancodes);

	return self;
}

- (void)viewDidLoad {
	[super viewDidLoad];

	_engineWindow->handleWindowLoaded();
}

- (void)viewDidAppear {
	[super viewDidAppear];
	_displayLink.paused = NO;
}

- (void)viewWillDisappear {
	_displayLink.paused = YES;
	[super viewWillDisappear];
}

- (void)viewDidDisappear {
	_engineWindow = nullptr;
	[super viewWillDisappear];
}

- (void)loadView {
	auto extent = _engineWindow->getWindow().contentLayoutRect.size;

	auto view = [[XLMacosView alloc] initWithFrame:NSRect{{0.0f, 0.0f}, extent}
											window:_engineWindow];

	view.wantsLayer = YES;

	_currentSize = extent;

	self.view = view;
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize {
	return frameSize;
}

- (void)windowDidResize:(NSNotification *)notification {
	NSSize size = self.view.window.contentLayoutRect.size;
	if (_currentSize.width != size.width || _currentSize.height != size.height) {
		_currentSize = size;
		_engineWindow->getController()->notifyWindowConstraintsChanged(_engineWindow,
				self.view.inLiveResize ? NSXL::core::PresentationSwapchainFlags::EnableLiveResize
									   : NSXL::core::PresentationSwapchainFlags::None);
	}
}

- (void)windowWillStartLiveResize:(NSNotification *)notification {
	//NSSP::log::debug("XLMacosViewController", "windowWillStartLiveResize");
}

- (void)windowDidEndLiveResize:(NSNotification *)notification {
	_engineWindow->getController()->notifyWindowConstraintsChanged(_engineWindow,
			NSXL::core::PresentationSwapchainFlags::DisableLiveResize);
}

- (BOOL)windowShouldClose:(NSWindow *)sender {
	return _engineWindow->getController()->notifyWindowClosed(_engineWindow,
			NSXLPL::WindowCloseOptions::NotifyExitGuard);
}

- (void)windowWillClose:(NSNotification *)notification {
	NSSP::log::debug("XLMacosViewController", "windowWillClose");
	_engineWindow->getWindow().delegate = nullptr;
	_engineWindow->getController()->notifyWindowClosed(_engineWindow,
			NSXLPL::WindowCloseOptions::CloseInPlace | NSXLPL::WindowCloseOptions::IgnoreExitGuard);
}

- (void)mouseDown:(NSEvent *)theEvent {
	auto pointInView = [self.view convertPoint:theEvent.locationInWindow fromView:nil];
	CGPoint loc = CGPoint([self.view convertPointToBacking:pointInView]);
	auto mods = NSXLPL::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSXL::core::InputEventData event{
		static_cast<uint32_t>(theEvent.buttonNumber),
		NSXL::core::InputEventName::Begin,
		NSXL::core::InputMouseButton::MouseLeft,
		mods,
		float(loc.x),
		float(loc.y),
	};

	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{event});
	_currentPointerLocation = loc;
}

- (void)rightMouseDown:(NSEvent *)theEvent {
	auto pointInView = [self.view convertPoint:theEvent.locationInWindow fromView:nil];
	CGPoint loc = CGPoint([self.view convertPointToBacking:pointInView]);
	auto mods = NSXLPL::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSXL::core::InputEventData event{
		static_cast<uint32_t>(theEvent.buttonNumber),
		NSXL::core::InputEventName::Begin,
		NSXL::core::InputMouseButton::MouseRight,
		mods,
		float(loc.x),
		float(loc.y),
	};

	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{event});
	_currentPointerLocation = loc;
}

- (void)otherMouseDown:(NSEvent *)theEvent {
	auto pointInView = [self.view convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.view convertPointToBacking:pointInView]);
	auto mods = NSXLPL::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSXL::core::InputEventData event{
		static_cast<uint32_t>(theEvent.buttonNumber),
		NSXL::core::InputEventName::Begin,
		NSXLPL::getInputMouseButton(uint32_t(theEvent.buttonNumber)),
		mods,
		float(loc.x),
		float(loc.y),
	};

	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{event});
	_currentPointerLocation = loc;
}

- (void)mouseUp:(NSEvent *)theEvent {
	auto pointInView = [self.view convertPoint:theEvent.locationInWindow fromView:nil];
	CGPoint loc = CGPoint([self.view convertPointToBacking:pointInView]);
	auto mods = NSXLPL::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSXL::core::InputEventData event{
		static_cast<uint32_t>(theEvent.buttonNumber),
		NSXL::core::InputEventName::End,
		NSXL::core::InputMouseButton::MouseLeft,
		mods,
		float(loc.x),
		float(loc.y),
	};

	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{event});
	_currentPointerLocation = loc;
}

- (void)rightMouseUp:(NSEvent *)theEvent {
	auto pointInView = [self.view convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.view convertPointToBacking:pointInView]);
	auto mods = NSXLPL::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSXL::core::InputEventData event{
		static_cast<uint32_t>(theEvent.buttonNumber),
		NSXL::core::InputEventName::End,
		NSXL::core::InputMouseButton::MouseRight,
		mods,
		float(loc.x),
		float(loc.y),
	};

	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{event});
	_currentPointerLocation = loc;
}

- (void)otherMouseUp:(NSEvent *)theEvent {
	auto pointInView = [self.view convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.view convertPointToBacking:pointInView]);
	auto mods = NSXLPL::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSXL::core::InputEventData event{
		static_cast<uint32_t>(theEvent.buttonNumber),
		NSXL::core::InputEventName::End,
		NSXLPL::getInputMouseButton(uint32_t(theEvent.buttonNumber)),
		mods,
		float(loc.x),
		float(loc.y),
	};

	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{event});
	_currentPointerLocation = loc;
}

- (void)mouseMoved:(NSEvent *)theEvent {
	auto pointInView = [self.view convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.view convertPointToBacking:pointInView]);
	auto mods = NSXLPL::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSXL::core::InputEventData event{
		std::numeric_limits<uint32_t>::max(),
		NSXL::core::InputEventName::MouseMove,
		NSXL::core::InputMouseButton::None,
		mods,
		float(loc.x),
		float(loc.y),
	};

	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{event});
	_currentPointerLocation = loc;
}

- (void)mouseDragged:(NSEvent *)theEvent {
	auto pointInView = [self.view convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.view convertPointToBacking:pointInView]);
	auto mods = NSXLPL::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSXL::Vector<NSXL::core::InputEventData> events;

	events.emplace_back(NSXL::core::InputEventData{
		static_cast<uint32_t>(theEvent.buttonNumber),
		NSXL::core::InputEventName::Move,
		NSXL::core::InputMouseButton::MouseLeft,
		mods,
		float(loc.x),
		float(loc.y),
	});
	events.emplace_back(NSXL::core::InputEventData{
		std::numeric_limits<uint32_t>::max(),
		NSXL::core::InputEventName::MouseMove,
		NSXL::core::InputMouseButton::None,
		mods,
		float(loc.x),
		float(loc.y),
	});

	_engineWindow->handleInputEvents(move(events));
	_currentPointerLocation = loc;
}

- (void)scrollWheel:(NSEvent *)theEvent {
	auto pointInView = [self.view convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.view convertPointToBacking:pointInView]);
	auto mods = NSXLPL::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSXL::Vector<NSXL::core::InputEventData> events;

	uint32_t buttonId = 0;
	NSXL::core::InputMouseButton buttonName;

	if (theEvent.scrollingDeltaY != 0) {
		if (theEvent.scrollingDeltaY > 0) {
			buttonName = NSXL::core::InputMouseButton::MouseScrollUp;
		} else {
			buttonName = NSXL::core::InputMouseButton::MouseScrollDown;
		}
	} else {
		if (theEvent.scrollingDeltaX > 0) {
			buttonName = NSXL::core::InputMouseButton::MouseScrollRight;
		} else {
			buttonName = NSXL::core::InputMouseButton::MouseScrollLeft;
		}
	}

	buttonId = stappler::maxOf<uint32_t>() - stappler::toInt(buttonName);

	events.emplace_back(NSXL::core::InputEventData{
		buttonId,
		NSXL::core::InputEventName::Begin,
		buttonName,
		mods,
		float(loc.x),
		float(loc.y),
	});
	events.emplace_back(NSXL::core::InputEventData{
		buttonId,
		NSXL::core::InputEventName::Scroll,
		buttonName,
		mods,
		float(loc.x),
		float(loc.y),
	});
	events.emplace_back(NSXL::core::InputEventData{
		buttonId,
		NSXL::core::InputEventName::End,
		buttonName,
		mods,
		float(loc.x),
		float(loc.y),
	});

	events.at(1).point.valueX = theEvent.scrollingDeltaX;
	events.at(1).point.valueY = theEvent.scrollingDeltaY;
	events.at(1).point.density = 1.0f;

	_engineWindow->handleInputEvents(move(events));
	_currentPointerLocation = loc;
}

- (void)rightMouseDragged:(NSEvent *)theEvent {
	auto pointInView = [self.view convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.view convertPointToBacking:pointInView]);
	auto mods = NSXLPL::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSXL::core::InputEventData event{
		static_cast<uint32_t>(theEvent.buttonNumber),
		NSXL::core::InputEventName::Move,
		NSXL::core::InputMouseButton::MouseRight,
		mods,
		float(loc.x),
		float(loc.y),
	};

	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{event});
	_currentPointerLocation = loc;
}

- (void)otherMouseDragged:(NSEvent *)theEvent {
	auto pointInView = [self.view convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.view convertPointToBacking:pointInView]);
	auto mods = NSXLPL::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSXL::core::InputEventData event{
		static_cast<uint32_t>(theEvent.buttonNumber),
		NSXL::core::InputEventName::Move,
		NSXLPL::getInputMouseButton(uint32_t(theEvent.buttonNumber)),
		mods,
		float(loc.x),
		float(loc.y),
	};

	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{event});
	_currentPointerLocation = loc;
}

- (void)mouseEntered:(NSEvent *)theEvent {
	auto pointInView = [self.view convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.view convertPointToBacking:pointInView]);

	_engineWindow->handleInputEvents(
			NSXL::Vector<NSXL::core::InputEventData>{NSXL::core::InputEventData::BoolEvent(
					NSXL::core::InputEventName::PointerEnter, true, NSXL::Vec2(loc.x, loc.y))});
	_currentPointerLocation = loc;
}

- (void)mouseExited:(NSEvent *)theEvent {
	auto pointInView = [self.view convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.view convertPointToBacking:pointInView]);

	_engineWindow->handleInputEvents(
			NSXL::Vector<NSXL::core::InputEventData>{NSXL::core::InputEventData::BoolEvent(
					NSXL::core::InputEventName::PointerEnter, false, NSXL::Vec2(loc.x, loc.y))});
	_currentPointerLocation = loc;
}

- (BOOL)becomeFirstResponder {
	return [super becomeFirstResponder];
}

- (BOOL)resignFirstResponder {
	return [super resignFirstResponder];
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{
		NSXL::core::InputEventData::BoolEvent(NSXL::core::InputEventName::FocusGain, true,
				NSXL::Vec2(_currentPointerLocation.x, _currentPointerLocation.y))});
}

- (void)windowDidResignKey:(NSNotification *)notification {
	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{
		NSXL::core::InputEventData::BoolEvent(NSXL::core::InputEventName::FocusGain, false,
				NSXL::Vec2(_currentPointerLocation.x, _currentPointerLocation.y))});
}

- (void)viewDidChangeBackingProperties {
	_viewScale = [self.view convertSizeToBacking:CGSizeMake(1.0, 1.0)];

	NSSP::log::debug("XLMacosViewController", "viewDidChangeBackingProperties: ", _viewScale.width,
			" ", _viewScale.height);
}

- (void)keyDown:(NSEvent *)theEvent {
	auto mods = NSXLPL::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;
	auto code = [theEvent keyCode];

	NSXL::core::InputEventData event{
		static_cast<uint32_t>(code),
		theEvent.isARepeat ? NSXL::core::InputEventName::KeyRepeated
						   : NSXL::core::InputEventName::KeyPressed,
		NSXLPL::getInputMouseButton(uint32_t(theEvent.buttonNumber)),
		mods,
		float(_currentPointerLocation.x),
		float(_currentPointerLocation.y),
	};

	event.key.keycode = _keycodes[static_cast<uint8_t>(code)];
	event.key.compose = NSXL::core::InputKeyComposeState::Disabled;
	event.key.keysym = 0;
	event.key.keychar = 0;

	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{event});
}

- (void)keyUp:(NSEvent *)theEvent {
	auto mods = NSXLPL::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;
	auto code = XL_OBJC_CALL([theEvent keyCode]);

	NSXL::core::InputEventData event{
		static_cast<uint32_t>(code),
		NSXL::core::InputEventName::KeyReleased,
		NSXLPL::getInputMouseButton(uint32_t(theEvent.buttonNumber)),
		mods,
		float(_currentPointerLocation.x),
		float(_currentPointerLocation.y),
	};

	event.key.keycode = _keycodes[static_cast<uint8_t>(code)];
	event.key.compose = NSXL::core::InputKeyComposeState::Disabled;
	event.key.keysym = 0;
	event.key.keychar = 0;

	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{event});
}

- (void)flagsChanged:(NSEvent *)theEvent {
	static std::pair<NSXL::core::InputModifier, NSXL::core::InputKeyCode> testmask[] = {
		std::make_pair(NSXL::core::InputModifier::ShiftL, NSXL::core::InputKeyCode::LEFT_SHIFT),
		std::make_pair(NSXL::core::InputModifier::ShiftR, NSXL::core::InputKeyCode::RIGHT_SHIFT),
		std::make_pair(NSXL::core::InputModifier::CtrlL, NSXL::core::InputKeyCode::LEFT_CONTROL),
		std::make_pair(NSXL::core::InputModifier::CtrlR, NSXL::core::InputKeyCode::RIGHT_CONTROL),
		std::make_pair(NSXL::core::InputModifier::AltL, NSXL::core::InputKeyCode::LEFT_ALT),
		std::make_pair(NSXL::core::InputModifier::AltR, NSXL::core::InputKeyCode::RIGHT_ALT),
		std::make_pair(NSXL::core::InputModifier::WinL, NSXL::core::InputKeyCode::LEFT_SUPER),
		std::make_pair(NSXL::core::InputModifier::WinR, NSXL::core::InputKeyCode::RIGHT_SUPER),
		std::make_pair(NSXL::core::InputModifier::CapsLock, NSXL::core::InputKeyCode::CAPS_LOCK),
		std::make_pair(NSXL::core::InputModifier::NumLock, NSXL::core::InputKeyCode::NUM_LOCK),
		std::make_pair(NSXL::core::InputModifier::Mod5, NSXL::core::InputKeyCode::WORLD_1),
		std::make_pair(NSXL::core::InputModifier::Mod4, NSXL::core::InputKeyCode::WORLD_2),
	};

	NSXL::core::InputModifier mods = NSXLPL::getInputModifiers(uint32_t(theEvent.modifierFlags));
	if (mods == _currentModifiers) {
		return;
	}

	auto diff = mods ^ _currentModifiers;

	NSXL::core::InputEventData event{
		static_cast<uint32_t>(0),
		NSXL::core::InputEventName::KeyReleased,
		NSXL::core::InputMouseButton::None,
		mods,
		float(_currentPointerLocation.x),
		float(_currentPointerLocation.y),
	};

	for (auto &it : testmask) {
		if ((diff & it.first) != 0) {
			event.id = NSSP::toInt(it.first);
			if ((mods & it.first) != 0) {
				event.event = NSXL::core::InputEventName::KeyPressed;
			}
			event.key.keycode = it.second;
			event.key.compose = NSXL::core::InputKeyComposeState::Disabled;
			event.key.keysym = 0;
			event.key.keychar = 0;
			break;
		}
	}

	if (event.id) {
		_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{event});
	}

	_currentModifiers = mods;
}

@end


static const NSRange kEmptyRange = {NSNotFound, 0};

@implementation XLMacosView

+ (Class)layerClass {
	return [CAMetalLayer class];
}

- (instancetype)initWithFrame:(NSRect)frameRect window:(NSXLPL::MacosWindow *)window {
	self = [super initWithFrame:frameRect];
	_validAttributesForMarkedText = [NSArray array];
	_textDirty = false;
	_window = window;

	_mainArea = nullptr;
	_cursorAreas = nullptr;

	self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

	return self;
}

- (BOOL)wantsUpdateLayer {
	return YES;
}

- (CALayer *)makeBackingLayer {
	self.layer = [self.class.layerClass layer];

	CGSize viewScale = [self convertSizeToBacking:CGSizeMake(1.0, 1.0)];
	self.layer.contentsScale = MIN(viewScale.width, viewScale.height);

	//_displayLink = [[CAMetalDisplayLink alloc] initWithMetalLayer:(CAMetalLayer *)self.layer];

	return self.layer;
}

- (BOOL)layer:(CALayer *)layer
		shouldInheritContentsScale:(CGFloat)newScale
						fromWindow:(NSWindow *)window {
	NSSP::log::debug("XLMacosView", "shouldInheritContentsScale: ", newScale);
	return YES;
}

- (BOOL)acceptsFirstResponder {
	return YES;
}

- (void)viewDidMoveToWindow {
}

- (void)updateTrackingAreas {
	[self removeTrackingArea:_mainArea];

	_mainArea = [[NSTrackingArea alloc]
			initWithRect:[self bounds]
				 options:NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited
			| NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect
				   owner:self
				userInfo:nil];
	[self addTrackingArea:_mainArea];
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

@end
