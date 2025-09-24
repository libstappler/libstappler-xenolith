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

#include "XLWindowsWindow.h"
#include "XLWindowsWindowClass.h"
#include "XLWindowsContextController.h"
#include "XLAppWindow.h"

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkPresentationEngine.h" // IWYU pragma: keep
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

WindowsWindow::~WindowsWindow() {
	if (_window) {
		SetWindowLongPtrW(_window, GWLP_USERDATA, 0);
		DestroyWindow(_window);
		_window = nullptr;
	}
}

bool WindowsWindow::init(NotNull<WindowsContextController> c, Rc<WindowInfo> &&info) {
	if (!NativeWindow::init(c, move(info), c->getCapabilities())) {
		return false;
	}

	auto dcm = c->getDisplayConfigManager();
	auto cfg = dcm->getCurrentConfig();
	if (cfg) {
		for (auto &it : cfg->monitors) {
			auto &c = it.getCurrent();
			_frameRate = std::max(_frameRate, uint32_t(c.mode.rate));
		}
	}

	_wTitle = string::toUtf16<Interface>(_info->title);

	_class = c->acquuireWindowClass(string::toUtf16<Interface>(_info->id));

	RECT rect = {0, 0, long(_info->rect.width), long(_info->rect.height)};

	_currentState.style = WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_MAXIMIZEBOX | WS_MINIMIZEBOX
			| WS_SYSMENU | WS_CAPTION | WS_THICKFRAME;

	_info->state |= WindowState::Enabled;

	AdjustWindowRect(&rect, _currentState.style, FALSE);

	_currentState.position = IVec2(rect.left, rect.top);
	_currentState.extent = Extent2(rect.right - rect.left, rect.bottom - rect.top);

	_window = CreateWindowExW(0, // Optional window styles.
			(wchar_t *)_class->getName().data(), // Window class
			(wchar_t *)_wTitle.data(), // Window text
			_currentState.style, // Window style

			// Size and position
			CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,

			NULL, // Parent window
			NULL, // Menu
			_class->getModule(), // Instance handle
			nullptr);

	if (_window) {
		SetWindowLongPtrW(_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	}

	_density = float(GetDpiForWindow(_window)) / float(USER_DEFAULT_SCREEN_DPI);

	return _window != nullptr;
}

void WindowsWindow::mapWindow() {
	ShowWindow(_window, SW_SHOWNORMAL);
	SetForegroundWindow(_window);
	SetActiveWindow(_window);
	SetFocus(_window);
}

void WindowsWindow::unmapWindow() { }

bool WindowsWindow::close() {
	if (!_controller->notifyWindowClosed(this)) {
		if (hasFlag(_info->state, WindowState::CloseGuard)) {
			updateState(0, _info->state | WindowState::CloseRequest);
		}
		return false;
	}
	return true;
}

void WindowsWindow::handleFramePresented(NotNull<core::PresentationFrame> frame) {
	auto e = frame->getFrameConstraints().extent;
	_commitedExtent = Extent2(e.width, e.height);
}

core::SurfaceInfo WindowsWindow::getSurfaceOptions(const core::Device &dev,
		NotNull<core::Surface> surface) const {
	if (hasFlag(_info->state, WindowState::Fullscreen)
			&& hasFlag(_info->fullscreen.flags, core::FullscreenFlags::Exclusive)) {
		// try to acquire surface info for exclusive fullscreen first
		if (auto hmon = MonitorFromWindow(_window, MONITOR_DEFAULTTONEAREST)) {
			//auto fsMode = core::FullScreenExclusiveMode::Allowed;
			auto fsMode = core::FullScreenExclusiveMode::ApplicationControlled;
			auto info = surface->getSurfaceOptions(dev, fsMode, hmon);
			// check if exclusive fullscreen actually available
			if (info.fullscreenMode == fsMode && info.fullscreenHandle == hmon) {
				return info;
			}
		}
	}

	// default surface info acquisition
	return surface->getSurfaceOptions(dev, core::FullScreenExclusiveMode::Default, nullptr);
}

core::FrameConstraints WindowsWindow::exportConstraints(core::FrameConstraints &&ct) const {
	auto c = NativeWindow::exportConstraints(sp::move(ct));

	c.extent = Extent3(_currentState.extent, 1);
	if (c.density == 0.0f) {
		c.density = 1.0f;
	}
	if (_density != 0.0f) {
		c.density *= _density;
		c.surfaceDensity = _density;
	}

	c.frameInterval = 1'000'000'000ULL / _frameRate;

	return c;
}

Extent2 WindowsWindow::getExtent() const { return _currentState.extent; }

Rc<core::Surface> WindowsWindow::makeSurface(NotNull<core::Instance> cinstance) {
#if MODULE_XENOLITH_BACKEND_VK
	if (cinstance->getApi() != core::InstanceApi::Vulkan) {
		return nullptr;
	}

	auto instance = static_cast<vk::Instance *>(cinstance.get());

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkWin32SurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR, nullptr,
		0, _class->getModule(), _window};

	if (instance->vkCreateWin32SurfaceKHR(instance->getInstance(), &createInfo, nullptr, &surface)
			!= VK_SUCCESS) {
		return nullptr;
	}
	return Rc<vk::Surface>::create(instance, surface, this);
#else
	log::source().error("WindowsWindow", "No available GAPI found for a surface");
	return nullptr;
#endif
}

core::PresentationOptions WindowsWindow::getPreferredOptions() const {
	return core::PresentationOptions();
}

bool WindowsWindow::enableState(WindowState state) { return NativeWindow::enableState(state); }

bool WindowsWindow::disableState(WindowState state) { return NativeWindow::disableState(state); }

void WindowsWindow::handleDisplayChanged(const DisplayConfig *cfg) {
	if (hasFlag(_info->state, WindowState::Fullscreen)) {
		auto display = cfg->getLogical(_info->fullscreen.id);
		if (!display) {
			// target monitor not found, exit form fullscreen
			_controller->getLooper()->performOnThread([this] {
				updateState(0, _info->state & ~WindowState::Fullscreen);
				updateWindowState(_savedState);
			}, this);
		} else if (display->rect != _currentState.frame) {
			_currentState.frame = display->rect;
			updateWindowState(_currentState);
		}

		if (display) {
			SetForegroundWindow(_window);
			SetActiveWindow(_window);
			SetFocus(_window);
			EnableWindow(_window, TRUE);
		}
	}
}

Status WindowsWindow::handleDestroy() {
	XL_WIN32_LOG(std::source_location::current().function_name());
	PostQuitMessage(0);
	return Status::Ok;
}

Status WindowsWindow::handleMove(IVec2 pos) {
	XL_WIN32_LOG(std::source_location::current().function_name());
	_currentState.position = pos;
	return Status::Ok;
}

Status WindowsWindow::handleResize(Extent2 e, WindowState state, WindowState mask) {
	XL_WIN32_LOG(std::source_location::current().function_name(), " ", e.width, " ", e.height);
	if (_currentState.extent != e) {
		_currentState.extent = e;
		_controller->notifyWindowConstraintsChanged(this,
				core::UpdateConstraintsFlags::DeprecateSwapchain);
	}
	auto newState = (_info->state & ~mask) | state;
	if (newState != _info->state) {
		updateState(0, newState);
	}
	return Status::Ok;
}

Status WindowsWindow::handleActivate(ActivateStatus) {
	XL_WIN32_LOG(std::source_location::current().function_name());
	return Status::Ok;
}

Status WindowsWindow::handleFocus(bool focusGain) {
	XL_WIN32_LOG(std::source_location::current().function_name(), " ", focusGain);

	if (focusGain) {
		updateState(0, _info->state | WindowState::Focused);
	} else {
		updateState(0, _info->state & ~WindowState::Focused);
	}
	return Status::Ok;
}

Status WindowsWindow::handleEnabled(bool enabled) {
	XL_WIN32_LOG(std::source_location::current().function_name());

	if (enabled) {
		updateState(0, _info->state | WindowState::Enabled);
	} else {
		updateState(0, _info->state & ~WindowState::Enabled);
	}
	return Status::Ok;
}

Status WindowsWindow::handlePaint() {
	if (hasFlag(_info->state, WindowState::Resizing)) {
		//XL_WIN32_LOG(std::source_location::current().function_name());
		if (_appWindow) {
			_appWindow->getPresentationEngine()->waitUntilFramePresentation();
			return Status::Ok;
		}
	}
	return Status::Propagate;
}

Status WindowsWindow::handleClose() {
	XL_WIN32_LOG(std::source_location::current().function_name());

	if (!_controller->notifyWindowClosed(this)) {
		if (hasFlag(_info->state, WindowState::CloseGuard)) {
			updateState(0, _info->state | WindowState::CloseRequest);
		}
	}
	return Status::Ok;
}

Status WindowsWindow::handleEraseBackground() {
	//XL_WIN32_LOG(std::source_location::current().function_name());
	emitAppFrame();
	return Status::Propagate;
}

Status WindowsWindow::handleWindowVisible(bool visible) {
	XL_WIN32_LOG(std::source_location::current().function_name());
	if (visible) {
		updateState(0, _info->state & ~WindowState::Minimized);
	} else {
		updateState(0, _info->state | WindowState::Minimized);
	}
	return Status::Ok;
}

Status WindowsWindow::handleSetCursor() {
	//XL_WIN32_LOG(std::source_location::current().function_name());
	return Status::Propagate;
}

Status WindowsWindow::handleStyleChanging(StyleType, STYLESTRUCT *style) {
	XL_WIN32_LOG(std::source_location::current().function_name());
	return Status::Propagate;
}

Status WindowsWindow::handleStyleChanged(StyleType type, const STYLESTRUCT *style) {
	XL_WIN32_LOG(std::source_location::current().function_name());
	switch (type) {
	case StyleType::Style: _currentState.style = style->styleNew; break;
	case StyleType::ExtendedStyle: _currentState.exstyle = style->styleNew; break;
	}
	return Status::Propagate;
}

Status WindowsWindow::handleWindowDecorations(bool enabled, const NCCALCSIZE_PARAMS *,
		const RECT *) {
	XL_WIN32_LOG(std::source_location::current().function_name());
	return Status::Propagate;
}

Status WindowsWindow::handleKeyPress(core::InputKeyCode keyCode, int scancode, char32_t c) {
	_enabledModifiers = KeyCodes::getKeyMods();

	c = makeKeyChar(c);

	auto ev = core::InputEventData{
		uint32_t(keyCode),
		core::InputEventName::KeyPressed,
		{{
			core::InputMouseButton::Touch,
			_enabledModifiers,
			_pointerLocation.x,
			_pointerLocation.y,
		}},
	};
	ev.key.keycode = keyCode;
	ev.key.compose = core::InputKeyComposeState::Nothing;
	ev.key.keysym = scancode;
	ev.key.keychar = c;

	_pendingEvents.emplace_back(sp::move(ev));
	return Status::Ok;
}

Status WindowsWindow::handleKeyRepeat(core::InputKeyCode keyCode, int scancode, char32_t c,
		int count) {
	_enabledModifiers = KeyCodes::getKeyMods();

	c = makeKeyChar(c);

	auto ev = core::InputEventData{
		uint32_t(keyCode),
		core::InputEventName::KeyRepeated,
		{{
			core::InputMouseButton::Touch,
			_enabledModifiers,
			_pointerLocation.x,
			_pointerLocation.y,
		}},
	};
	ev.key.keycode = keyCode;
	ev.key.compose = core::InputKeyComposeState::Nothing;
	ev.key.keysym = scancode;
	ev.key.keychar = c;

	if (!count) {
		_pendingEvents.emplace_back(sp::move(ev));
	} else {
		Vector<core::InputEventData> data;
		for (int i = count; i >= 0; --i) { _pendingEvents.emplace_back(sp::move(ev)); }
	}

	return Status::Ok;
}

Status WindowsWindow::handleKeyRelease(core::InputKeyCode keyCode, int scancode, char32_t c) {
	c = makeKeyChar(c);

	auto ev = core::InputEventData{
		uint32_t(keyCode),
		core::InputEventName::KeyReleased,
		{{
			core::InputMouseButton::Touch,
			_enabledModifiers,
			_pointerLocation.x,
			_pointerLocation.y,
		}},
	};
	ev.key.keycode = keyCode;
	ev.key.compose = c ? core::InputKeyComposeState::Forced : core::InputKeyComposeState::Nothing;
	ev.key.keysym = scancode;
	ev.key.keychar = c;

	_enabledModifiers = KeyCodes::getKeyMods();

	_pendingEvents.emplace_back(sp::move(ev));
	return Status::Ok;
}

Status WindowsWindow::handleChar(char32_t c) {
	_enabledModifiers = KeyCodes::getKeyMods();
	c = makeKeyChar(c);

	if (c && _textInput && isTextInputEnabled()) {
		_textInput->insertText(string::toUtf16<Interface>(c), core::InputKeyComposeState::Forced);
	}
	return Status::Ok;
}

Status WindowsWindow::handleMouseMove(IVec2 pos) {
	_enabledModifiers = KeyCodes::getKeyMods();

	_pointerLocation = Vec2(pos.x, _currentState.extent.height - pos.y);

	if (!_mouseTracked) {
		TRACKMOUSEEVENT tme;
		ZeroMemory(&tme, sizeof(tme));
		tme.cbSize = sizeof(tme);
		tme.dwFlags = TME_LEAVE;
		tme.hwndTrack = _window;
		::TrackMouseEvent(&tme);

		_mouseTracked = true;

		updateState(0, _info->state | WindowState::Pointer);
	}

	_pendingEvents.emplace_back(core::InputEventData({
		maxOf<uint32_t>(),
		core::InputEventName::MouseMove,
		{{
			core::InputMouseButton::None,
			_enabledModifiers,
			float(_pointerLocation.x),
			float(_pointerLocation.y),
		}},
	}));
	return Status::Ok;
}

Status WindowsWindow::handleMouseLeave() {
	updateState(0, _info->state & ~WindowState::Pointer);
	_mouseTracked = false;
	return Status::Ok;
}

Status WindowsWindow::handleMouseEvent(IVec2 pos, core::InputMouseButton btn,
		core::InputEventName ev) {
	switch (ev) {
	case core::InputEventName::Begin:
		if (_pointerButtonCapture++ == 0) {
			::SetCapture(_window);
		}
		break;
	case core::InputEventName::End:
		if (_pointerButtonCapture-- == 1) {
			::ReleaseCapture();
		}
		break;
	default: break;
	}

	_enabledModifiers = KeyCodes::getKeyMods();

	_pendingEvents.emplace_back(core::InputEventData({
		uint32_t(btn),
		ev,
		{{
			btn,
			_enabledModifiers,
			float(_pointerLocation.x),
			float(_pointerLocation.y),
		}},
	}));
	return Status::Ok;
}

Status WindowsWindow::handleMouseWheel(Vec2 value) {
	_enabledModifiers = KeyCodes::getKeyMods();

	core::InputMouseButton btn = core::InputMouseButton::None;
	if (value.x > 0) {
		btn = core::InputMouseButton::MouseScrollRight;
	} else if (value.x < 0) {
		btn = core::InputMouseButton::MouseScrollLeft;
	}

	if (value.y > 0) {
		btn = core::InputMouseButton::MouseScrollDown;
	} else if (value.y < 0) {
		btn = core::InputMouseButton::MouseScrollUp;
	}

	core::InputEventData event({
		toInt(btn),
		core::InputEventName::Scroll,
		{{
			btn,
			_enabledModifiers,
			_pointerLocation.x,
			_pointerLocation.y,
		}},
	});

	event.point.valueX = value.x * 10.0f;
	event.point.valueY = value.y * 10.0f;

	_pendingEvents.emplace_back(sp::move(event));
	return Status::Ok;
}

Status WindowsWindow::handleMouseCaptureChanged() {
	if (_pointerButtonCapture > 0) {
		::ReleaseCapture();
		_pointerButtonCapture = 0;
	}
	return Status::Ok;
}

Status WindowsWindow::handlePositionChanging(WINDOWPOS *pos) {
	XL_WIN32_LOG(std::source_location::current().function_name());

	// Commented code below can lock live resizing until commited extent differs from requested

	/*if (hasFlag(_info->state, WindowState::Resizing)) {
		if (_currentState.extent != _commitedExtent) {
			pos->x = _currentState.frame.x;
			pos->y = _currentState.frame.y;
			pos->cx = _currentState.frame.width;
			pos->cy = _currentState.frame.height;
			return Status::Ok;
		}
	}*/

	if (_currentState.isFullscreen) {
		auto cfg = _controller->getDisplayConfigManager()->getCurrentConfig();
		auto display = cfg->getLogical(_info->fullscreen.id);
		if (display) {
			pos->x = display->rect.x;
			pos->y = display->rect.y;
			pos->cx = display->rect.width;
			pos->cy = display->rect.height;
			return Status::Ok;
		} else {
			pos->x = _savedState.frame.x;
			pos->y = _savedState.frame.y;
			pos->cx = _savedState.frame.width;
			pos->cy = _savedState.frame.height;

			// target monitor not found, exit form fullscreen
			_controller->getLooper()->performOnThread([this] {
				updateWindowState(_savedState);
				updateState(0, _info->state & ~WindowState::Fullscreen);
			}, this);
			return Status::Ok;
		}
	}
	return Status::Propagate;
}

Status WindowsWindow::handlePositionChanged(const WINDOWPOS *pos) {
	XL_WIN32_LOG(std::source_location::current().function_name(), " ", pos->x, " ", pos->y, " ",
			pos->cx, " ", pos->cy);
	_currentState.frame = IRect{pos->x, pos->y, uint32_t(pos->cx), uint32_t(pos->cy)};
	return Status::Propagate;
}

Status WindowsWindow::handleSizing(core::ViewConstraints, RECT *rect) {
	XL_WIN32_LOG(std::source_location::current().function_name(), " ", rect->left, " ", rect->top,
			" ", rect->right - rect->left, " ", rect->bottom - rect->top);
	return Status::Propagate;
}

Status WindowsWindow::handleMoving(RECT *) {
	XL_WIN32_LOG(std::source_location::current().function_name());
	return Status::Propagate;
}

Status WindowsWindow::handleMoveResize(bool enter) {
	XL_WIN32_LOG(std::source_location::current().function_name());
	if (enter) {
		updateState(0, _info->state | WindowState::Resizing);
	} else {
		updateState(0, _info->state & ~WindowState::Resizing);
	}
	return Status::Ok;
}

Status WindowsWindow::handleDpiChanged(Vec2 scale, const RECT *rect) {
	_density = std::max(scale.x, scale.y);

	auto mon = MonitorFromWindow(_window, MONITOR_DEFAULTTONEAREST);
	auto cfg = _controller->getDisplayConfigManager()->getCurrentConfig();

	uint32_t rate = 0;

	for (auto &it : cfg->logical) {
		if (it.xid == mon) {
			for (auto &mId : it.monitors) {
				auto v = cfg->getMonitor(mId);
				if (v) {
					rate = std::max(v->getCurrent().mode.rate, rate);
				}
			}
			break;
		}
	}

	if (rate == 0) {
		rate = 60'000;
	}

	_frameRate = rate;
	_controller->notifyWindowConstraintsChanged(this, core::UpdateConstraintsFlags::None);

	return Status::Ok;
}

bool WindowsWindow::updateTextInput(const TextInputRequest &, TextInputFlags flags) { return true; }

void WindowsWindow::cancelTextInput() { }

void WindowsWindow::setCursor(WindowCursor cursor) {
	switch (cursor) {
	case WindowCursor::Undefined:
	case WindowCursor::ContextMenu:
	case WindowCursor::VerticalText:
	case WindowCursor::Cell:
	case WindowCursor::Alias:
	case WindowCursor::Copy:
	case WindowCursor::Grab:
	case WindowCursor::Grabbing:
	case WindowCursor::ZoomIn:
	case WindowCursor::ZoomOut:
	case WindowCursor::DndAsk:
	case WindowCursor::RightPtr:
	case WindowCursor::Target:
	case WindowCursor::Default: SetCursor(LoadCursorW(0, IDC_ARROW)); break;
	case WindowCursor::Pointer: SetCursor(LoadCursorW(0, IDC_HAND)); break;

	case WindowCursor::Help: SetCursor(LoadCursorW(0, IDC_HELP)); break;
	case WindowCursor::Progress: SetCursor(LoadCursorW(0, IDC_APPSTARTING)); break;
	case WindowCursor::Wait: SetCursor(LoadCursorW(0, IDC_WAIT)); break;
	case WindowCursor::Crosshair: SetCursor(LoadCursorW(0, IDC_CROSS)); break;
	case WindowCursor::Text: SetCursor(LoadCursorW(0, IDC_IBEAM)); break;
	case WindowCursor::Move: SetCursor(LoadCursorW(0, IDC_SIZEALL)); break;
	case WindowCursor::NoDrop: SetCursor(LoadCursorW(0, IDC_NO)); break;
	case WindowCursor::NotAllowed: SetCursor(LoadCursorW(0, IDC_NO)); break;

	case WindowCursor::AllScroll: SetCursor(LoadCursorW(0, IDC_SIZEALL)); break;
	case WindowCursor::Pencil: SetCursor(LoadCursorW(0, MAKEINTRESOURCE(32'631))); break;

	case WindowCursor::ResizeRight:
	case WindowCursor::ResizeLeft:
	case WindowCursor::ResizeLeftRight:
	case WindowCursor::ResizeCol: SetCursor(LoadCursorW(0, IDC_SIZEWE)); break;
	case WindowCursor::ResizeTop:
	case WindowCursor::ResizeBottom:
	case WindowCursor::ResizeTopBottom:
	case WindowCursor::ResizeRow: SetCursor(LoadCursorW(0, IDC_SIZENS)); break;
	case WindowCursor::ResizeTopLeft:
	case WindowCursor::ResizeBottomRight:
	case WindowCursor::ResizeTopLeftBottomRight: SetCursor(LoadCursorW(0, IDC_SIZENWSE)); break;
	case WindowCursor::ResizeTopRight:
	case WindowCursor::ResizeBottomLeft:
	case WindowCursor::ResizeTopRightBottomLeft: SetCursor(LoadCursorW(0, IDC_SIZENESW)); break;
	case WindowCursor::ResizeAll: SetCursor(LoadCursorW(0, IDC_SIZEALL)); break;
	case WindowCursor::Max: break;
	}
}

Status WindowsWindow::setFullscreenState(FullscreenInfo &&info) {
	auto makeFullscreenState = [](State current, const LogicalDisplay *display) -> State {
		return State{
			(current.style & (~WS_BORDER) & (~WS_DLGFRAME) & (~WS_THICKFRAME) & (~WS_CAPTION))
					| WS_VISIBLE,
			(current.exstyle & (~WS_EX_WINDOWEDGE) & (~WS_EX_DLGMODALFRAME) & (~WS_EX_CLIENTEDGE)
					& (~WS_EX_STATICEDGE))
					| WS_EX_TOPMOST,
			IVec2(display->rect.x, display->rect.y),
			Extent2(display->rect.width, display->rect.height),
			display->rect,
			true,
		};
	};

	auto enable = info != FullscreenInfo::None;
	if (!enable) {
		if (hasFlag(_info->state, WindowState::Fullscreen)) {
			// disable fullscreen
			_info->fullscreen = move(info);
			updateWindowState(_savedState);
			updateState(0, _info->state & ~WindowState::Fullscreen);
			return Status::Ok;
		} else {
			return Status::Declined;
		}
	} else {
		if (info == FullscreenInfo::Current) {
			if (!hasFlag(_info->state, WindowState::Fullscreen)) {
				_savedState = _currentState;

				auto mon = MonitorFromWindow(_window, MONITOR_DEFAULTTONEAREST);
				auto cfg = _controller->getDisplayConfigManager()->getCurrentConfig();

				const LogicalDisplay *display = nullptr;
				for (auto &it : cfg->logical) {
					if (it.xid == mon) {
						display = &it;
						for (auto &mId : it.monitors) {
							auto v = cfg->getMonitor(mId);
							if (v) {
								info.mode = v->getCurrent().mode;
								info.id = mId;
							}
						}
						break;
					}
				}

				if (display) {
					_currentState = makeFullscreenState(_currentState, display);
					_info->fullscreen = move(info);
					updateWindowState(_currentState);
					updateState(0, _info->state | WindowState::Fullscreen);
					return Status::Ok;
				} else {
					return Status::ErrorInvalidArguemnt;
				}
			}
			return Status::Declined;
		}

		// fullscreen to specific monitor
		if (hasFlag(_info->state, WindowState::Fullscreen) && _info->fullscreen.id == info.id) {
			return Status::Declined;
		}

		auto cfg = _controller->getDisplayConfigManager()->getCurrentConfig();
		const LogicalDisplay *display = nullptr;
		for (auto &it : cfg->logical) {
			if (std::find(it.monitors.begin(), it.monitors.end(), info.id) != it.monitors.end()) {
				display = &it;
				break;
			}
		}
		if (!display) {
			return Status::ErrorInvalidArguemnt;
		}

		if (!hasFlag(_info->state, WindowState::Fullscreen)) {
			_savedState = _currentState;
		}

		_currentState = makeFullscreenState(_currentState, display);
		_info->fullscreen = move(info);
		updateWindowState(_currentState);
		updateState(0, _info->state | WindowState::Fullscreen);

		return Status::Ok;
	} // namespace stappler::xenolith::platform
	return Status::ErrorNotImplemented;
}

char32_t WindowsWindow::makeKeyChar(char32_t c) {
	if (c >= 0xd800 && c <= 0xdbff) {
		_highSurrogate = c;
		return 0;
	} else if (c >= 0xdc00 && c <= 0xdfff) {
		if (_highSurrogate) {
			char32_t ret = (_highSurrogate - 0xd800) << 10;
			ret += c - 0xdc00;
			ret += 0x1'0000;
			_highSurrogate = 0;
			return ret;
		} else {
			_highSurrogate = 0;
			return 0;
		}
	} else if (c != 0) {
		_highSurrogate = 0;
		return c;
	}
	return 0;
}

void WindowsWindow::updateWindowState(const State &state) {
	_currentState.isFullscreen = state.isFullscreen;
	SetWindowLongPtrW(_window, GWL_STYLE, state.style);
	SetWindowLongPtrW(_window, GWL_EXSTYLE, state.exstyle);
	SetWindowPos(_window, hasFlag(state.exstyle, DWORD(WS_EX_TOPMOST)) ? HWND_TOPMOST : HWND_TOP,
			state.frame.x, state.frame.y, state.frame.width, state.frame.height,
			SWP_SHOWWINDOW | SWP_FRAMECHANGED);
	_currentState = state;
	XL_WIN32_LOG(std::source_location::current().function_name());
}

} // namespace stappler::xenolith::platform
