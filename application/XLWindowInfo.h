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

#ifndef XENOLITH_APPLICATION_XLWINDOWINFO_H_
#define XENOLITH_APPLICATION_XLWINDOWINFO_H_

#include "XLApplicationConfig.h" // IWYU pragma: keep
#include "XLCorePresentationEngine.h" // IWYU pragma: keep
#include "XlCoreMonitorInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppWindow;

using core::ModeInfo;
using core::MonitorId;
using core::MonitorInfo;
using core::ScreenInfo;
using core::FullscreenFlags;
using core::FullscreenInfo;
using core::ViewConstraints;
using core::WindowState;

enum class WindowCursor : uint8_t {
	Undefined,
	Default,
	ContextMenu,
	Help,
	Pointer,
	Progress,
	Wait,
	Cell,
	Crosshair,
	Text,
	VerticalText,
	Alias,
	Copy,
	Move,
	NoDrop,
	NotAllowed,
	Grab,
	Grabbing,

	AllScroll,
	ZoomIn,
	ZoomOut,
	DndAsk,

	RightPtr,
	Pencil,
	Target,

	ResizeRight,
	ResizeTop,
	ResizeTopRight,
	ResizeTopLeft,
	ResizeBottom,
	ResizeBottomRight,
	ResizeBottomLeft,
	ResizeLeft,
	ResizeLeftRight,
	ResizeTopBottom,
	ResizeTopRightBottomLeft,
	ResizeTopLeftBottomRight,
	ResizeCol,
	ResizeRow,
	ResizeAll,
	Max,
};

enum class WindowLayerFlags : uint32_t {
	None,
	MoveGrip,
	ResizeTopLeftGrip,
	ResizeTopGrip,
	ResizeTopRightGrip,
	ResizeRightGrip,
	ResizeBottomRightGrip,
	ResizeBottomGrip,
	ResizeBottomLeftGrip,
	ResizeLeftGrip,

	GripMask = 0xF,

	// Grip-like flag to open window menu with left-click
	WindowMenuLeft = 1 << 4,

	// Grip-like flag to open window menu with right-click
	WindowMenuRight = 1 << 5,
};

SP_DEFINE_ENUM_AS_MASK(WindowLayerFlags)

struct WindowLayer {
	Rect rect;
	WindowCursor cursor = WindowCursor::Undefined;
	WindowLayerFlags flags = WindowLayerFlags::None;

	operator bool() const {
		return cursor != WindowCursor::Undefined || flags != WindowLayerFlags::None;
	}

	bool operator==(const WindowLayer &) const = default;
	bool operator!=(const WindowLayer &) const = default;
};

// This flags defined when window is created, and immutable
// Note that window manager can decide, what actual capabilities window will have,
// you can only ask for a some capabilities
//
// For actions (like move), you can ask to allow requests, but if you don't,
// it will not make window non-movable, it only can forbid to move it via server-size decorations
//
// To acquire actual actions flags, provided by WM, use WindowUpdate InputEvent,
// or AppWindow::getWindowActions()
enum class WindowCreationFlags : uint32_t {
	None = 0,

	// Ask window manager to allow move requests
	AllowMove = 1 << 0,

	// Ask window manager to allow resize requests
	AllowResize = 1 << 1,

	// Ask window manager to allow minimize requests
	AllowMinimize = 1 << 2,

	// Ask window manager to allow maximize requests
	AllowMaximize = 1 << 3,

	// Ask window manager to allow fullscreen requests
	AllowFullscreen = 1 << 4,

	// Ask window manager to allow close requests
	AllowClose = 1 << 5,

	// Flags for the regular, common OS window
	Regular =
			AllowMove | AllowResize | AllowMinimize | AllowMaximize | AllowFullscreen | AllowClose,

	// draw window without server-side decoration borders
	UserSpaceDecorations = 1 << 6,

	// Use direct output to display, bypassing whole WM stack
	// Check if it actually supported with WindowCapabilities::DirectOutput
	DirectOutput = 1 << 27,

	// Try to use server-side decoration where available
	PreferServerSideDecoration = 1 << 28,

	// Try to use system-native decoration where available
	// Used to enable client-side native decorations (libdecor on Wayland)
	PreferNativeDecoration = 1 << 29,

	// If possible, use server-defined cursors instead of client-side libraries
	// Note that server-side cursor theme may not contain all cursors
	PreferServerSideCursors = 1 << 30,
};

SP_DEFINE_ENUM_AS_MASK(WindowCreationFlags)

enum class WindowAttributes : uint32_t {
	None,
	Opaque = 1 << 0,
	Movable = 1 << 1,
	Resizeable = 1 << 2,
	Minimizable = 1 << 3,
	Maximizable = 1 << 4,
};

SP_DEFINE_ENUM_AS_MASK(WindowAttributes)

// Cababilities, prowided by OS Window Manager
enum class WindowCapabilities : uint32_t {
	None,

	// Switch between windowed and fullscreen modes
	// If not provided - window is only windowed or only fullscreen
	Fullscreen = 1 << 0,

	// Suupport for exculsive output to display, can be more performant then usual fullscreen.
	// Note that on some platforms (Wayland, MacOS, some X11 implementations), there is no need
	// in specific exculsive mode, it's automatic
	FullscreenExclusive = 1 << 1,

	// Support for different display modes in fullscreen mode
	// Without this capability, only ModeInfo::Current can be set as mmode for FullscreenInfo
	FullscreenWithMode = 1 << 2,

	// Systems with this capability can switch display mode without exit/enter cycle when
	// application already fullscreened
	FullscreenSeamlessModeSwitch = 1 << 3,

	// Window server can draw window decoration by itself
	// Without this capability, application is responsible for decoration drawing
	ServerSideDecorations = 1 << 4,

	// There is a client library for native client decoration drawing
	NativeDecorations = 1 << 5,

	// Window server can draw cursors by itself
	ServerSideCursors = 1 << 6,

	// Subwindows are allowed
	Subwindows = 1 << 7,

	// Direct output is available on platform
	DirectOutput = 1 << 8,

	// 'Back' action can close application (Android-like)
	BackIsExit = 1 << 9,

	// Full user-space decoration mode is supported
	UserSpaceDecorations = 1 << 10,

	// Above and below state supprted
	AboveBelowState = 1 << 11,

	// DemandsAttention state supprted
	DemandsAttentionState = 1 << 12,

	// SkipTaskbar and SkipPager state supported
	SkipTaskbarState = 1 << 13,

	// Platform supports CloseGuard
	CloseGuard = 1 << 14,

	// Window supports separate MaximizeVert/MaximizeHorz in enableState/disableState
	SeparateMaximize = 1 << 15,
};

SP_DEFINE_ENUM_AS_MASK(WindowCapabilities)

struct SP_PUBLIC DecorationInfo {
	float borderRadius = 0.0f;
	float shadowWidth = 0.0f;
	float shadowMinValue = 0.1f;
	float shadowMaxValue = 0.25f;
	float shadowCurrentValue = 0.3f;
	Vec2 shadowOffset;

	operator bool() const { return borderRadius > 0.0f || shadowWidth > 0.0f; }
};

struct SP_PUBLIC WindowInfo final : public Ref {
	String id;
	String title;
	URect rect = URect(0, 0, 1'024, 768);
	float density = 0.0f;
	WindowCreationFlags flags = WindowCreationFlags::None;

	// initial fullscreen mode
	FullscreenInfo fullscreen = FullscreenInfo::None;

	// TODO: extra window attributes go here

	core::PresentMode preferredPresentMode = core::PresentMode::Mailbox;
	core::ImageFormat imageFormat = core::ImageFormat::Undefined;
	core::ColorSpace colorSpace = core::ColorSpace::SRGB_NONLINEAR_KHR;

	// provided by WM, no reason to set it diraectly
	WindowCapabilities capabilities = WindowCapabilities::None;

	// provided by WM, no reason to set it diraectly
	WindowState state = WindowState::None;

	// Insets for decorations, that appears above user-drawing space
	// Canvas inside this inset always be visible for user
	Padding decorationInsets;

	// User-space decorations properties
	// Note that user-space decorations shadows drawed by native WM integration,
	// but rounded corners and their shadow insets should be drawed by GL canvas itself
	DecorationInfo userDecorations;

	core::FrameConstraints exportConstraints() const {
		return core::FrameConstraints{
			.extent = Extent2(rect.width, rect.height),
			.contentPadding = decorationInsets,
			.transform = core::SurfaceTransformFlags::Identity,
			.density = density,
		};
	}

	Value encode() const;
};

SP_PUBLIC StringView getWindowCursorName(WindowCursor);

inline const CallbackStream &operator<<(const CallbackStream &stream, WindowCursor t) {
	stream << getWindowCursorName(t);
	return stream;
}

inline std::ostream &operator<<(std::ostream &stream, WindowCursor t) {
	stream << getWindowCursorName(t);
	return stream;
}


} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_XLWINDOWINFO_H_
