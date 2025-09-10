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
#import <MetalKit/MetalKit.h>

#import "XLMacosView.h"

#include "XLMacosWindow.h"
#include "XLAppWindow.h"
#include "platform/XLContextController.h"

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkSwapchain.h"
#endif

@interface XLMacosViewController : NSViewController <NSWindowDelegate> {
	CADisplayLink *_displayLink;
	CGPoint _currentPointerLocation;
	NSXL::core::InputModifier _currentModifiers;
	NSXL::core::InputKeyCode _keycodes[256];
	uint16_t _scancodes[128];
	NSXLPL::MacosWindow *_engineWindow;
};

- (instancetype _Nonnull)init:(NSSP::NotNull<NSXLPL::MacosWindow>)constroller
					   window:(NSWindow *_Nonnull)window;

- (void)invalidate;

- (XLMacosView *)targetView;

- (void)setEngineLiveResize:(BOOL)value;

@end

@interface XLMacosWindow : NSWindow {
	NSWindowStyleMask _defaultStyle;
}

- (void)setFrame:(NSRect)frameRect
				  display:(BOOL)displayFlag
				 duration:(NSTimeInterval)duration
		completionHandler:(nullable void (^)(void))completionHandler;

- (void)toggleFullScreen:(id)sender withScreen:(NSScreen *)screen;

- (NSWindowStyleMask)defaultStyle;

@end

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

MacosWindow::~MacosWindow() {
	if (_controller && _isRootWindow) {
		_controller->handleRootWindowClosed();
	}

	_rootViewController = nullptr;
	if (_window) {
		_window = nullptr;
	}
}

bool MacosWindow::init(NotNull<ContextController> controller, Rc<WindowInfo> &&info,
		bool isRootWindow) {
	WindowCapabilities caps = WindowCapabilities::FullscreenWithMode
			| WindowCapabilities::FullscreenSeamlessModeSwitch | WindowCapabilities::Fullscreen
			| WindowCapabilities::ServerSideDecorations;

	if (!NativeWindow::init(controller, sp::move(info), caps, isRootWindow)) {
		return false;
	}

	auto style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
			| NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

	auto rect = NSRect{
		{static_cast<CGFloat>(_info->rect.x), static_cast<CGFloat>(_info->rect.y)},
		{static_cast<CGFloat>(_info->rect.width), static_cast<CGFloat>(_info->rect.height)},
	};

	_window = [[XLMacosWindow alloc] initWithContentRect:rect
											   styleMask:style
												 backing:NSBackingStoreBuffered
												   defer:YES];

	_window.animationBehavior = NSWindowAnimationBehaviorNone;

	[_window setReleasedWhenClosed:false];
	_rootViewController = [[XLMacosViewController alloc] init:this window:_window];
	_window.contentViewController = _rootViewController;
	_window.contentView = _window.contentViewController.view;
	_initialized = true;

	if (_windowLoaded) {
		_controller->notifyWindowCreated(this);
		[_window display];
		//[_window makeKeyAndOrderFront:_rootViewController];
	}
	return true;
}

void MacosWindow::mapWindow() {
	// show window in front and activate;
	[_window makeKeyAndOrderFront:nil];
	[_window orderFrontRegardless];
	[_window orderWindow:NSWindowAbove relativeTo:0];

	if (_isRootWindow) {
		[NSApp activateIgnoringOtherApps:YES];
	}
}

void MacosWindow::unmapWindow() {
	[_rootViewController invalidate];
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

void MacosWindow::handleFramePresented(NotNull<core::PresentationFrame> frame) { }

void MacosWindow::handleLayerUpdate(const WindowLayer &layer) {
	auto cursor = layer.flags & WindowLayerFlags::CursorMask;
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
	}
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
	CGSize size;
	if (_rootViewController && _rootViewController.targetView) {
		size = _rootViewController.targetView.layer.frame.size;
	} else {
		size = _window.frame.size;
	}

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
		0, (CAMetalLayer *)_rootViewController.targetView.layer};
	if (instance->vkCreateMetalSurfaceEXT(instance->getInstance(), &createInfo, nullptr, &surface)
			!= VK_SUCCESS) {
		return nullptr;
	}
	return Rc<vk::Surface>::create(instance, surface, this);
#else
	log::source().error("XcbWindow", "No available gAPI found for a surface");
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

void MacosWindow::addMacosStateFlags(NativeWindowStateFlags state) {
	if (!hasFlagAll(_state, state)) {
		_state |= state;
	}
}

void MacosWindow::clearMacosStateFlags(NativeWindowStateFlags state) {
	if (hasFlag(_state, state)) {
		_state &= ~state;
	}
}

void MacosWindow::emitAppFrame() {
	if (_appWindow) {
		_appWindow->setReadyForNextFrame();
		_appWindow->update(core::PresentationUpdateFlags::DisplayLink);
	}
}

void MacosWindow::handleFullscreenTransitionComplete(MacosFullscreenRequest req) {
	if (_hasPendingFullscreenOp) {
		if (_fullscreenRequest == req) {
			_hasPendingFullscreenOp = false;
			_nextScreen = nullptr;
		} else if (req == MacosFullscreenRequest::ExitFullscreen
				&& _fullscreenRequest == MacosFullscreenRequest::ToggleFullscreen) {
			_fullscreenRequest = MacosFullscreenRequest::EnterFullscreen;
		}
	}
}

Status MacosWindow::setFullscreenState(FullscreenInfo &&info) {
	if (_hasPendingFullscreenOp) {
		return Status::ErrorAgain;
	}

	auto enable = info != FullscreenInfo::None;
	if (!enable) {
		if (hasFlag(_state, NativeWindowStateFlags::Fullscreen)) {
			_hasPendingFullscreenOp = true;
			_fullscreenRequest = MacosFullscreenRequest::ExitFullscreen;
			[_window toggleFullScreen:nil];
			_info->fullscreen = move(info);
			_hasOriginalFrame = false;
			return Status::Ok;
		} else {
			return Status::Declined;
		}
	} else {
		auto frame = _window.frame;

		if (!hasFlag(_state, NativeWindowStateFlags::Fullscreen)) {
			_originalFrame = frame;
			_hasOriginalFrame = true;
		}

		if (hasFlag(info.flags, FullscreenFlags::Current)) {
			if (!hasFlag(_state, NativeWindowStateFlags::Fullscreen)) {
				auto current = _controller->getDisplayConfigManager()->getCurrentConfig();

				auto screen = _window.screen;
				NSNumber *screenNumber = [screen deviceDescription][@"NSScreenNumber"];

				for (auto &it : current->monitors) {
					if (CGDisplayUnitNumber([screenNumber unsignedIntValue]) == it.index) {
						info.id = it.id;
						info.mode = it.getCurrent().mode;
						_hasPendingFullscreenOp = true;
						_fullscreenRequest = MacosFullscreenRequest::EnterFullscreen;
						[_window toggleFullScreen:nil];
						_info->fullscreen = move(info);
						return Status::Ok;
						break;
					}
				}
				return Status::Ok;
			}
			return Status::Declined;
		}

		auto current = _controller->getDisplayConfigManager()->getCurrentConfig();
		auto mon = current->getMonitor(info.id);
		if (!mon) {
			return Status::ErrorInvalidArguemnt;
		}

		NSScreen *screen = nil;
		for (screen in [NSScreen screens]) {
			NSNumber *screenNumber = [screen deviceDescription][@"NSScreenNumber"];
			if (CGDisplayUnitNumber([screenNumber unsignedIntValue]) == mon->index) {
				break;
			}
		}

		if (!screen) {
			return Status::ErrorInvalidArguemnt;
		}

		info.id = mon->id;
		info.mode = mon->getCurrent().mode;

		// find target output
		if (hasFlag(_state, NativeWindowStateFlags::Fullscreen)) {
			if (screen != _window.screen) {
				_hasPendingFullscreenOp = true;
				_fullscreenRequest = MacosFullscreenRequest::ToggleFullscreen;
				_nextScreen = screen;
				[_window toggleFullScreen:nil];
				_info->fullscreen = move(info);
				return Status::Ok;
			}
			return Status::Declined;
		}

		if (screen == _window.screen) {
			_hasPendingFullscreenOp = true;
			_fullscreenRequest = MacosFullscreenRequest::EnterFullscreen;
			[_window toggleFullScreen:nil];
			_info->fullscreen = move(info);
			return Status::Ok;
		}

		_hasPendingFullscreenOp = true;
		_fullscreenRequest = MacosFullscreenRequest::EnterFullscreen;
		[_window toggleFullScreen:nil withScreen:screen];
		_info->fullscreen = move(info);
		return Status::Ok;
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
	[self.view setNeedsDisplay:YES];
	[self.targetView setNeedsDisplay:YES];
}

- (instancetype _Nonnull)init:(NSSP::NotNull<NSXLPL::MacosWindow>)w
					   window:(NSWindow *_Nonnull)window {
	self = [super init];
	_engineWindow = w;

	_currentPointerLocation = CGPoint{0, 0};
	_currentModifiers = NSXL::core::InputModifier::None;

	window.delegate = self;

	_displayLink = [window displayLinkWithTarget:self selector:@selector(handleDisplayLink:)];
	[_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
	_displayLink.paused = YES;

	NSXLPL::createKeyTables(_keycodes, _scancodes);

	return self;
}

- (void)invalidate {
	_engineWindow = nullptr;
}

- (XLMacosView *)targetView {
	return (XLMacosView *)self.view;
}

- (void)setEngineLiveResize:(BOOL)value {
	_engineWindow->getController()->notifyWindowConstraintsChanged(_engineWindow,
			value ? NSXL::core::PresentationSwapchainFlags::EnableLiveResize
				  : NSXL::core::PresentationSwapchainFlags::DisableLiveResize);
}

- (void)viewDidLoad {
	[super viewDidLoad];

	_engineWindow->handleWindowLoaded();

	[_engineWindow->getWindow() setHidesOnDeactivate:NO];
	[_engineWindow->getWindow() setCanHide:NO];
	_engineWindow->getWindow().displaysWhenScreenProfileChanges = YES;
}

- (void)viewDidAppear {
	if (!_engineWindow) {
		return;
	}

	[super viewDidAppear];
	_displayLink.paused = NO;
	_engineWindow->clearMacosStateFlags(NSXLPL::NativeWindowStateFlags::Hidden);
}

- (void)viewWillDisappear {
	if (!_engineWindow) {
		return;
	}

	_engineWindow->addMacosStateFlags(NSXLPL::NativeWindowStateFlags::Hidden);
	_displayLink.paused = YES;
	[super viewWillDisappear];
}

- (void)viewDidDisappear {
	[super viewWillDisappear];
}

- (void)loadView {
	auto extent = _engineWindow->getWindow().contentLayoutRect.size;

	//auto view = [[MTKView alloc] initWithFrame:_engineWindow->getWindow().contentLayoutRect
	//									device:nullptr];

	auto view = [[XLMacosView alloc] initWithFrame:NSRect{{0.0f, 0.0f}, extent}
											window:_engineWindow];

	view.wantsLayer = YES;

	self.view = view;
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize {
	return frameSize;
}

- (void)windowDidResize:(NSNotification *)notification {
	if (!_engineWindow) {
		[self.targetView setFrame:self.view.window.contentLayoutRect];
		return;
	}

	NSSize size = self.view.window.contentLayoutRect.size;
	NSSP::log::source().debug("MacosWindow", "windowDidResize: ", size.width, " ", size.height);

	CAMetalLayer *metalLayer = (CAMetalLayer *)self.view.layer; // 3
	metalLayer.drawableSize = [self.view convertSizeToBacking:self.view.frame.size]; // 4

	_engineWindow->getController()->notifyWindowConstraintsChanged(_engineWindow,
			self.view.inLiveResize ? NSXL::core::PresentationSwapchainFlags::EnableLiveResize
								   : NSXL::core::PresentationSwapchainFlags::None);
	_engineWindow->emitAppFrame();

	auto isZoomed = _engineWindow->getWindow().zoomed;
	if (isZoomed) {
		_engineWindow->addMacosStateFlags(NSXLPL::NativeWindowStateFlags::Maximized);
	} else {
		_engineWindow->clearMacosStateFlags(NSXLPL::NativeWindowStateFlags::Maximized);
	}
}

- (void)windowWillMove:(NSNotification *)notification {
}

- (void)windowDidMove:(NSNotification *)notification {
	//_engineWindow->emitAppFrame();
}

- (void)windowWillStartLiveResize:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	_engineWindow->addMacosStateFlags(NSXLPL::NativeWindowStateFlags::Resizing);
}

- (void)windowDidEndLiveResize:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	_engineWindow->clearMacosStateFlags(NSXLPL::NativeWindowStateFlags::Resizing);
	_engineWindow->getController()->notifyWindowConstraintsChanged(_engineWindow,
			NSXL::core::PresentationSwapchainFlags::DisableLiveResize);
}

- (BOOL)windowShouldClose:(NSWindow *)sender {
	if (!_engineWindow) {
		return YES;
	}

	return _engineWindow->getController()->notifyWindowClosed(_engineWindow,
			NSXLPL::WindowCloseOptions::NotifyExitGuard);
}

- (void)windowWillClose:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	NSSP::log::source().debug("XLMacosViewController", "windowWillClose");
	_engineWindow->getWindow().delegate = nullptr;
	_engineWindow->getController()->notifyWindowClosed(_engineWindow,
			NSXLPL::WindowCloseOptions::CloseInPlace | NSXLPL::WindowCloseOptions::IgnoreExitGuard);
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{
		NSXL::core::InputEventData::BoolEvent(NSXL::core::InputEventName::FocusGain, true,
				NSXL::Vec2(_currentPointerLocation.x, _currentPointerLocation.y))});
	_engineWindow->addMacosStateFlags(NSXLPL::NativeWindowStateFlags::Focused);
}

- (void)windowDidResignKey:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	_engineWindow->clearMacosStateFlags(NSXLPL::NativeWindowStateFlags::Focused);
	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{
		NSXL::core::InputEventData::BoolEvent(NSXL::core::InputEventName::FocusGain, false,
				NSXL::Vec2(_currentPointerLocation.x, _currentPointerLocation.y))});
}

- (NSRect)windowWillUseStandardFrame:(NSWindow *)window defaultFrame:(NSRect)newFrame {
	return newFrame;
}

- (BOOL)windowShouldZoom:(NSWindow *)window toFrame:(NSRect)newFrame {
	return YES;
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{
		NSXL::core::InputEventData::BoolEvent(NSXL::core::InputEventName::Fullscreen, true)});
	_engineWindow->addMacosStateFlags(NSXLPL::NativeWindowStateFlags::Fullscreen);

	_engineWindow->handleFullscreenTransitionComplete(
			NSXLPL::MacosFullscreenRequest::EnterFullscreen);
}

- (void)windowWillExitFullScreen:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	_engineWindow->clearMacosStateFlags(NSXLPL::NativeWindowStateFlags::Fullscreen);
	_engineWindow->handleInputEvents(NSXL::Vector<NSXL::core::InputEventData>{
		NSXL::core::InputEventData::BoolEvent(NSXL::core::InputEventName::Fullscreen, false)});
}

- (void)windowDidExitFullScreen:(NSNotification *)notification {
	if (!_engineWindow) {
		return;
	}

	if (_engineWindow->getFullscreenRequest() == NSXLPL::MacosFullscreenRequest::ToggleFullscreen) {
		_engineWindow->handleFullscreenTransitionComplete(
				NSXLPL::MacosFullscreenRequest::ExitFullscreen);
		[_engineWindow->getWindow() toggleFullScreen:nil withScreen:_engineWindow->getNextScreen()];
	} else {
		_engineWindow->handleFullscreenTransitionComplete(
				NSXLPL::MacosFullscreenRequest::ExitFullscreen);
	}
}

- (NSArray<NSWindow *> *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window
														onScreen:(NSScreen *)screen;
{ return @[_engineWindow->getWindow()]; }

- (void)window:(NSWindow *)window
		startCustomAnimationToEnterFullScreenOnScreen:(NSScreen *)screen
										 withDuration:(NSTimeInterval)duration {
	NSSP::log::source().debug("MacosWindow", "startCustomAnimationToEnterFullScreenOnScreen");

	auto frame = window.frame;
	[window setStyleMask:([window styleMask] | NSWindowStyleMaskFullScreen)];
	[window setFrame:frame display:YES];

	[(XLMacosWindow *)window setFrame:screen.frame
							  display:YES
							 duration:duration
					completionHandler:nil];
}

- (NSArray<NSWindow *> *)customWindowsToExitFullScreenForWindow:(NSWindow *)window {
	return @[_engineWindow->getWindow()];
}

- (void)window:(NSWindow *)window
		startCustomAnimationToExitFullScreenWithDuration:(NSTimeInterval)duration {
	NSSP::log::source().debug("MacosWindow", "startCustomAnimationToExitFullScreenWithDuration");

	__weak XLMacosWindow *w = _engineWindow->getWindow();

	auto frame = w.screen.frame;
	[w setStyleMask:w.defaultStyle];
	[w setFrame:frame display:YES];

	auto origFrame = _engineWindow->getOriginalFrame();

	auto center = CGPoint{origFrame.origin.x + origFrame.size.width / 2.0,
		origFrame.origin.y + origFrame.size.height / 2.0};

	NSScreen *screen = nil;

	for (NSScreen *s in NSScreen.screens) {
		if (NSPointInRect(center, s.frame)) {
			screen = s;
			break;
		}
	}

	if (_engineWindow->getFullscreenRequest() == NSXLPL::MacosFullscreenRequest::ToggleFullscreen) {
		auto screenFrame = window.screen.frame;
		auto x = (screenFrame.size.width - origFrame.size.width) / 2.0;
		auto y = (screenFrame.size.height - origFrame.size.height) / 2.0;
		auto targetRect =
				NSRect{NSPoint{screenFrame.origin.x + x, screenFrame.origin.y + y}, origFrame.size};

		[w setFrame:targetRect
						  display:YES
						 duration:duration
				completionHandler:^() {
				  NSSP::log::source().debug("XLMacosWindow",
						  "startCustomAnimationToExitFullScreenWithDuration complete");
				}];

	} else if (screen != w.screen) {
		// this is bypass for the layout bug, when window unfullscreened to other screen
		// first, we unfullscreen window to screen center, then move to final position.
		// If we unfullsceen it into another screen - it will disappear from desktop

		auto screenFrame = window.screen.frame;
		auto x = (screenFrame.size.width - origFrame.size.width) / 2.0;
		auto y = (screenFrame.size.height - origFrame.size.height) / 2.0;
		auto targetRect =
				NSRect{NSPoint{screenFrame.origin.x + x, screenFrame.origin.y + y}, origFrame.size};

		[w setFrame:targetRect
						  display:YES
						 duration:duration
				completionHandler:^() {
				  [w setFrame:origFrame
									display:YES
								   duration:[w animationResizeTime:origFrame]
						  completionHandler:^() {
							NSSP::log::source().debug("XLMacosWindow",
									"startCustomAnimationToExitFullScreenWithDuration complete");
						  }];
				}];
	} else {
		[w setFrame:origFrame
						  display:YES
						 duration:duration
				completionHandler:^() {
				  NSSP::log::source().debug("XLMacosWindow",
						  "startCustomAnimationToExitFullScreenWithDuration complete");
				}];
	}
}

- (void)windowDidFailToExitFullScreen:(NSWindow *)window {
	NSSP::log::source().debug("XLMacosWindow", "windowDidFailToExitFullScreen");
}

- (void)mouseDown:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	CGPoint loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
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
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	CGPoint loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
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
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
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
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	CGPoint loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
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
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
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
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
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
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
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
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
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
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
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
	events.at(1).point.density = _engineWindow->getWindow().backingScaleFactor;

	_engineWindow->handleInputEvents(move(events));
	_currentPointerLocation = loc;
}

- (void)rightMouseDragged:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
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
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
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
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);
	auto mods = NSXLPL::getInputModifiers(uint32_t(theEvent.modifierFlags)) | _currentModifiers;

	NSXL::Vector<NSXL::core::InputEventData> events;

	events.emplace_back(NSXL::core::InputEventData::BoolEvent(
			NSXL::core::InputEventName::PointerEnter, true, NSXL::Vec2(loc.x, loc.y)));

	events.emplace_back(NSXL::core::InputEventData{
		std::numeric_limits<uint32_t>::max(),
		NSXL::core::InputEventName::MouseMove,
		NSXL::core::InputMouseButton::None,
		mods,
		float(loc.x),
		float(loc.y),
	});

	_engineWindow->handleInputEvents(sp::move(events));
	_currentPointerLocation = loc;
}

- (void)mouseExited:(NSEvent *)theEvent {
	if (!_engineWindow) {
		return;
	}

	auto pointInView = [self.targetView convertPoint:theEvent.locationInWindow fromView:nil];
	auto loc = CGPoint([self.targetView convertPointToBacking:pointInView]);

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

- (void)viewDidChangeBackingProperties {
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

	NSXL::String chars = theEvent.characters.UTF8String;
	NSXL::String ichars = theEvent.charactersIgnoringModifiers.UTF8String;

	event.key.keycode = _keycodes[static_cast<uint8_t>(code)];
	event.key.compose = NSXL::core::InputKeyComposeState::Disabled;
	event.key.keysym = NSSP::StringViewUtf8(ichars).getChar();
	event.key.keychar = NSSP::StringViewUtf8(chars).getChar();

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

	NSXL::String chars = theEvent.characters.UTF8String;
	NSXL::String ichars = theEvent.charactersIgnoringModifiers.UTF8String;

	event.key.keycode = _keycodes[static_cast<uint8_t>(code)];
	event.key.compose = NSXL::core::InputKeyComposeState::Disabled;
	event.key.keysym = NSSP::StringViewUtf8(ichars).getChar();
	event.key.keychar = NSSP::StringViewUtf8(chars).getChar();

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

@implementation XLMacosWindow

- (instancetype)initWithContentRect:(NSRect)contentRect
						  styleMask:(NSWindowStyleMask)style
							backing:(NSBackingStoreType)backingStoreType
							  defer:(BOOL)flag {
	self = [super initWithContentRect:contentRect
							styleMask:style
							  backing:backingStoreType
								defer:flag];
	_defaultStyle = style;
	return self;
}

- (NSWindowStyleMask)defaultStyle {
	return _defaultStyle;
}

- (void)setFrame:(NSRect)frameRect
				  display:(BOOL)displayFlag
				 duration:(NSTimeInterval)duration
		completionHandler:(nullable void (^)(void))completionHandler {
	[(XLMacosViewController *)self.contentViewController setEngineLiveResize:YES];
	[NSAnimationContext
			runAnimationGroup:^(NSAnimationContext *_Nonnull context) {
			  context.duration = duration;
			  [self.animator setFrame:frameRect display:displayFlag];
			}
			completionHandler:^(void) {
			  [(XLMacosViewController *)self.contentViewController setEngineLiveResize:NO];
			  if (completionHandler) {
				  completionHandler();
			  }
			}];
}

- (void)setFrame:(NSRect)frameRect display:(BOOL)displayFlag {
	NSSP::log::source().debug("XLMacosWindow", "setFrame: ", frameRect.origin.x, " ",
			frameRect.origin.y, " ", frameRect.size.width, " ", frameRect.size.height);
	[super setFrame:frameRect display:displayFlag];
}

- (void)setFrame:(NSRect)frameRect display:(BOOL)displayFlag animate:(BOOL)animateFlag {
	NSSP::log::source().debug("XLMacosWindow", "setFrame: ", frameRect.origin.x, " ",
			frameRect.origin.y, " ", frameRect.size.width, " ", frameRect.size.height);
	if (!animateFlag) {
		[super setFrame:frameRect display:displayFlag animate:NO];
	} else {
		[self setFrame:frameRect
						  display:displayFlag
						 duration:[self animationResizeTime:frameRect]
				completionHandler:nil];
	}
}

- (void)toggleFullScreen:(id)sender withScreen:(NSScreen *)screen {
	if (screen == self.screen) {
		[self toggleFullScreen:sender];
	} else {
		auto screenFrame = screen.frame;
		auto windowFrame = self.frame;
		auto x = (screenFrame.size.width - windowFrame.size.width) / 2.0;
		auto y = (screenFrame.size.height - windowFrame.size.height) / 2.0;
		auto targetRect = NSRect{NSPoint{screenFrame.origin.x + x, screenFrame.origin.y + y},
			windowFrame.size};

		__weak NSWindow *ref = self;

		[self setFrame:targetRect
						  display:YES
						 duration:[self animationResizeTime:targetRect]
				completionHandler:^() { [ref toggleFullScreen:sender]; }];
	}
}

- (NSTimeInterval)animationResizeTime:(NSRect)newFrame {
	return [super animationResizeTime:newFrame];
}

- (instancetype)animator {
	return [super animator];
}

- (id)animationForKey:(NSAnimatablePropertyKey)key {
	return [super animationForKey:key];
}

@end
