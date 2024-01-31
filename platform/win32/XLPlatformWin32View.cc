/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XLPlatformWin32View.h"

#define XL_WIN32_DEBUG 1

#ifndef XL_WIN32_LOG
#if XL_WIN32_DEBUG
#define XL_WIN32_LOG(...) log::debug("Win32", __VA_ARGS__)
#else
#define XL_WIN32_LOG(...)
#endif
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static core::InputModifier getKeyMods(void) {
	core::InputModifier mods = core::InputModifier::None;

	if (GetKeyState(VK_SHIFT) & 0x8000) {
		mods |= core::InputModifier::Shift;
	}
	if (GetKeyState(VK_RSHIFT) & 0x8000) {
		mods |= core::InputModifier::ShiftR;
	}
	if (GetKeyState(VK_LSHIFT) & 0x8000) {
		mods |= core::InputModifier::ShiftL;
	}
	if (GetKeyState(VK_CONTROL) & 0x8000) {
		mods |= core::InputModifier::Ctrl;
	}
	if (GetKeyState(VK_RCONTROL) & 0x8000) {
		mods |= core::InputModifier::CtrlR;
	}
	if (GetKeyState(VK_LCONTROL) & 0x8000) {
		mods |= core::InputModifier::CtrlL;
	}
	if (GetKeyState(VK_MENU) & 0x8000) {
		mods |= core::InputModifier::Menu;
	}
	if (GetKeyState(VK_RMENU) & 0x8000) {
		mods |= core::InputModifier::MenuR;
	}
	if (GetKeyState(VK_LMENU) & 0x8000) {
		mods |= core::InputModifier::MenuL;
	}
	if (GetKeyState(VK_LWIN) & 0x8000) {
		mods |= core::InputModifier::Win | core::InputModifier::WinL;
	}
	if (GetKeyState(VK_RWIN) & 0x8000) {
		mods |= core::InputModifier::Win | core::InputModifier::WinR;
	}
	if (GetKeyState(VK_CAPITAL) & 1) {
		mods |= core::InputModifier::CapsLock;
	}
	if (GetKeyState(VK_NUMLOCK) & 1) {
		mods |= core::InputModifier::NumLock;
	}
	if (GetKeyState(VK_SCROLL) & 1) {
		mods |= core::InputModifier::ScrollLock;
	}
	return mods;
}

Win32View::Win32View() { }

Win32View::~Win32View() {
	close();

	UnregisterClassW(_windowClass.lpszClassName, _winInstance);
}

bool Win32View::init(ViewInterface *view, Win32Library *win32, Win32ViewInfo &&info) {
	_view = view;
	_win32 = win32;
	_info = move(info);
	_width = _info.rect.width;
	_height = _info.rect.height;
	_winInstance = GetModuleHandleW(nullptr);

	_monitors = _win32->pollMonitors();
	for (auto &it : _monitors) {
		_rate = std::max(_rate, uint32_t(it.dm.dmDisplayFrequency));
	}

	_className = string::toUtf16<Interface>(_info.bundleId);
	_windowName = string::toUtf16<Interface>(_info.name);
	_windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	_windowClass.lpfnWndProc = wndProc;
	_windowClass.cbClsExtra = 0;
	_windowClass.cbWndExtra = 0;
	_windowClass.hInstance = _winInstance;
	_windowClass.hIcon = (HICON) LoadImage(nullptr, IDI_APPLICATION, IMAGE_ICON,
			0, 0, LR_DEFAULTSIZE | LR_SHARED);
	_windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	_windowClass.hbrBackground = nullptr;
	_windowClass.lpszMenuName = nullptr;
	_windowClass.lpszClassName = (wchar_t*) _className.data();

	RegisterClassW (&_windowClass);

	DWORD wintyle = WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_MAXIMIZEBOX
			| WS_MINIMIZEBOX | WS_SYSMENU | WS_CAPTION | WS_THICKFRAME;

	RECT rect = { 0, 0, long(_info.rect.width), long(_info.rect.height) };

	AdjustWindowRect(&rect, wintyle, FALSE);

	_window = CreateWindowExW(0, // Optional window styles.
			(wchar_t*) _className.data(), // Window class
			(wchar_t*) _windowName.data(), // Window text
			wintyle, // Window style

			// Size and position
			CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,

			NULL,       // Parent window
			NULL,       // Menu
			_winInstance,  // Instance handle
			this        // Additional application data
			);

	if (_window) {
		SetPropW(_window, L"Xenolith", this);
		SetTimer(_window, 0, 1 /*(millisec)*/, nullptr);
	}

	return _window != nullptr;
}

uint64_t Win32View::getScreenFrameInterval() const {
	return 1'000'000 / _rate;
}

void Win32View::dispose() {
	_view = nullptr;
}

void Win32View::close() {
	if (_window) {
		DestroyWindow (_window);
		_window = nullptr;
	}
}

void Win32View::mapWindow() {
	ShowWindow(_window, SW_SHOWNORMAL);
}

void Win32View::schedule(uint64_t t) {
	//log::verbose("Win32View", "schedule: ", t);
	//SetTimer(_window, 1, t / 1000 + 1 /*(millisec)*/, nullptr);
}

void Win32View::wakeup() {
	PostMessageW(_window, WM_USER, 0, 0);
}

void Win32View::handleTimer() {
	if (_inSizeMove) {
		_view->update(false);
	} else {
		_shouldUpdate = true;
	}
	// Таймер замедлен на 16 миллисекунд, чтобы обновление по нему не срабатывало раньше релизного окна кадра
	SetTimer(_window, 0, 1 /*(millisec)*/, nullptr);
}

void Win32View::handleWakeup() {
	if (_inSizeMove) {
		_view->update(false);
	} else {
		_shouldUpdate = true;
	}
}

bool Win32View::handleClose() {
	_shouldQuit = true;
	return _view != nullptr;
}

bool Win32View::handleSize(uint32_t w, uint32_t h, bool maximized, bool minimized) {
	if (!_view) {
		return false;
	}

	log::debug("View", w, " ", h, " ", maximized, " ", minimized);
	_view->setReadyForNextFrame();
	if (w != _width || h != _height) {
		_width = w;
		_height = h;
		_view->deprecateSwapchain(true);
	}
	if (_inSizeMove) {
		_view->update(false);
	} else {
		_shouldUpdate = true;
	}

	if (minimized != _iconified) {
		_iconified = minimized;
		_enabledModifiers = getKeyMods();
		_view->handleInputEvent(
				core::InputEventData::BoolEvent(core::InputEventName::Background,
						_iconified, _enabledModifiers, Vec2(float(_cursorPos.x), float(_cursorPos.y))));
	}

	return true;
}

void Win32View::handleEnterSizeMove() {
	if (!_view) {
		return;
	}

	_info.captureView(_view);
	_inSizeMove = true;
}

void Win32View::handleExitSizeMove() {
	if (!_view) {
		return;
	}

	_inSizeMove = false;
	_info.releaseView(_view);
}

void Win32View::handlePaint() {
	if (!_view) {
		return;
	}

	if (_inSizeMove) {
		_info.handlePaint(_view);
	}
}

void Win32View::handleMouseMove(int x, int y) {
	if (!_view) {
		return;
	}

	_enabledModifiers = getKeyMods();
	_cursorPos = Vec2(x, _height - y);

	if (!_mouseTracked) {
		TRACKMOUSEEVENT tme;
		ZeroMemory(&tme, sizeof(tme));
		tme.cbSize = sizeof(tme);
		tme.dwFlags = TME_LEAVE;
		tme.hwndTrack = _window;
		TrackMouseEvent(&tme);

		_mouseTracked = true;

		_view->handleInputEvent(
				core::InputEventData::BoolEvent(
						core::InputEventName::PointerEnter, true,
						Vec2(float(_cursorPos.x), float(_cursorPos.y))));
	}

	_view->handleInputEvent(core::InputEventData( { maxOf<uint32_t>(),
			core::InputEventName::MouseMove, core::InputMouseButton::None,
			_enabledModifiers, float(_cursorPos.x), float(_cursorPos.y) }));
}

void Win32View::handleMouseLeave() {
	if (!_view) {
		return;
	}

	_view->handleInputEvent(
			core::InputEventData::BoolEvent(core::InputEventName::PointerEnter,
					false, Vec2(float(_cursorPos.x), float(_cursorPos.y))));
	_mouseTracked = false;
}

void Win32View::handleKeyPress(core::InputKeyCode keyCode, int scancode, char32_t c) {
	if (!_view) {
		return;
	}

	_enabledModifiers = getKeyMods();

	c = makeKeyChar(c);

	auto ev =core::InputEventData{uint32_t(keyCode),
		core::InputEventName::KeyPressed, core::InputMouseButton::Touch, _enabledModifiers,
		_cursorPos.x, _cursorPos.y};
	ev.key.keycode = keyCode;
	ev.key.compose = core::InputKeyComposeState::Nothing;
	ev.key.keysym = scancode;
	ev.key.keychar = c;
	_view->handleInputEvent(ev);
}

void Win32View::handleKeyRepeat(core::InputKeyCode keyCode, int scancode, char32_t c, int count) {
	if (!_view) {
		return;
	}

	_enabledModifiers = getKeyMods();

	c = makeKeyChar(c);

	auto ev =core::InputEventData{uint32_t(keyCode),
		core::InputEventName::KeyRepeated, core::InputMouseButton::Touch, _enabledModifiers,
		_cursorPos.x, _cursorPos.y};
	ev.key.keycode = keyCode;
	ev.key.compose = core::InputKeyComposeState::Nothing;
	ev.key.keysym = scancode;
	ev.key.keychar = c;
	if (!count) {
		_view->handleInputEvent(ev);
	} else {
		Vector<core::InputEventData> data;
		for (int i = count; i >= 0; -- i) {
			data.emplace_back(ev);
		}
		_view->handleInputEvents(move(data));
	}
}

void Win32View::handleKeyRelease(core::InputKeyCode keyCode, int scancode, char32_t c) {
	if (!_view) {
		return;
	}

	auto ev =core::InputEventData{uint32_t(keyCode),
		core::InputEventName::KeyReleased, core::InputMouseButton::Touch, _enabledModifiers,
		_cursorPos.x, _cursorPos.y};

	c = makeKeyChar(c);

	ev.key.keycode = keyCode;
	ev.key.compose = c ? core::InputKeyComposeState::Forced : core::InputKeyComposeState::Nothing;
	ev.key.keysym = scancode;
	ev.key.keychar = c;
	_view->handleInputEvent(ev);
	_enabledModifiers = getKeyMods();
}

void Win32View::handleFocus(bool value) {
	if (!_view) {
		return;
	}

	_enabledModifiers = getKeyMods();
	_view->handleInputEvent(
			core::InputEventData::BoolEvent(core::InputEventName::FocusGain,
					value, _enabledModifiers, Vec2(float(_cursorPos.x), float(_cursorPos.y))));
}

void Win32View::handleChar(char32_t c) {
	if (!_view) {
		return;
	}

	_enabledModifiers = getKeyMods();
	c = makeKeyChar(c);

	if (c) {
		// virtualize key press
		Vector<core::InputEventData> data;

		auto evPress = core::InputEventData{uint32_t(core::InputKeyCode::Unknown),
			core::InputEventName::KeyPressed, core::InputMouseButton::Touch, _enabledModifiers,
			_cursorPos.x, _cursorPos.y};
		evPress.key.keycode = core::InputKeyCode::Unknown;
		evPress.key.compose = core::InputKeyComposeState::Nothing;
		evPress.key.keysym = 0;
		evPress.key.keychar = c;

		auto evRelease = core::InputEventData{uint32_t(core::InputKeyCode::Unknown),
			core::InputEventName::KeyReleased, core::InputMouseButton::Touch, _enabledModifiers,
			_cursorPos.x, _cursorPos.y};
		evRelease.key.keycode = core::InputKeyCode::Unknown;
		evRelease.key.compose = core::InputKeyComposeState::Nothing;
		evRelease.key.keysym = 0;
		evRelease.key.keychar = 0;

		data.emplace_back(evPress);
		data.emplace_back(evRelease);
	}
}

void Win32View::handleMouseEvent(core::InputMouseButton btn, core::InputEventName ev) {
	switch (ev) {
	case core::InputEventName::Begin:
		if (_pointerButtonCapture ++ == 0) {
			SetCapture(_window);
		}
		break;
	case core::InputEventName::End:
		if (_pointerButtonCapture -- == 1) {
			ReleaseCapture();
		}
		break;
	default:
		break;
	}

	_enabledModifiers = getKeyMods();
	_view->handleInputEvent(core::InputEventData{uint32_t(btn),
		ev, btn, _enabledModifiers, _cursorPos.x, _cursorPos.y});
}

void Win32View::handleMouseWheel(float x, float y) {
	_enabledModifiers = getKeyMods();

	core::InputMouseButton btn = core::InputMouseButton::None;
	if (x > 0) {
		btn = core::InputMouseButton::MouseScrollRight;
	} else if (x < 0) {
		btn = core::InputMouseButton::MouseScrollLeft;
	}

	if (y > 0) {
		btn = core::InputMouseButton::MouseScrollDown;
	} else if (y < 0) {
		btn = core::InputMouseButton::MouseScrollUp;
	}

	core::InputEventData event({
		toInt(btn),
		core::InputEventName::Scroll,
		btn,
		_enabledModifiers, _cursorPos.x, _cursorPos.y
	});

	event.point.valueX = x * 10.0f;
	event.point.valueY = y * 10.0f;

	_view->handleInputEvent(event);
}

char32_t Win32View::makeKeyChar(char32_t c) {
	if (c >= 0xd800 && c <= 0xdbff) {
		_highSurrogate = c;
		return 0;
	} else if (c >= 0xdc00 && c <= 0xdfff) {
		if (_highSurrogate) {
			char32_t ret = (_highSurrogate - 0xd800) << 10;
			ret += c - 0xdc00;
			ret += 0x10000;
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

LRESULT Win32View::wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	auto view = (Win32View *)GetPropW(hwnd, L"Xenolith");
	if (!view) {
    	return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	switch (uMsg) {
	case WM_MOVE:
    	return DefWindowProc(hwnd, uMsg, wParam, lParam);
		break;
	case WM_SIZE: {
		XL_WIN32_LOG("WM_SIZE");
		uint32_t width = LOWORD(lParam);
		uint32_t height = HIWORD(lParam);
		bool maximized = (wParam == SIZE_MAXIMIZED);
		bool minimized = (wParam == SIZE_MINIMIZED);

		if (!view->handleSize(width, height, maximized, minimized)) {
	    	return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}

		break;
	}
	case WM_TIMER:
		//XL_WIN32_LOG("WM_TIMER");
		view->handleTimer();
		break;
	case WM_USER:
		//XL_WIN32_LOG("WM_USER");
		view->handleWakeup();
		break;
	case WM_CLOSE:
		XL_WIN32_LOG("WM_CLOSE");
		if (!view->handleClose()) {
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
		break;
	case WM_DESTROY:
		XL_WIN32_LOG("WM_DESTROY");
		PostQuitMessage(0);
		break;
	case WM_ERASEBKGND:
		XL_WIN32_LOG("WM_ERASEBKGND");
		return 1;
		break;
	case WM_SETCURSOR:
		//XL_WIN32_LOG("WM_SETCURSOR");
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
		break;
	case WM_SIZING:
		XL_WIN32_LOG("WM_SIZING");
		return 1;
		break;
	case WM_ENTERSIZEMOVE:
		XL_WIN32_LOG("WM_ENTERSIZEMOVE");
		view->handleEnterSizeMove();
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
		break;
	case WM_EXITSIZEMOVE:
		XL_WIN32_LOG("WM_EXITSIZEMOVE");
		view->handleExitSizeMove();
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
		break;
	case WM_WINDOWPOSCHANGING:
		XL_WIN32_LOG("WM_WINDOWPOSCHANGING");
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
		break;
	case WM_WINDOWPOSCHANGED:
		XL_WIN32_LOG("WM_WINDOWPOSCHANGED");
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
		break;
	case WM_PAINT:
		view->handlePaint();
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
		break;
	case WM_SYSCOMMAND:
	case WM_GETMINMAXINFO:
	case WM_NCHITTEST:
	case WM_NCCALCSIZE:
	case WM_NCPAINT:
	case WM_NCMOUSEHOVER:
	case WM_NCMOUSELEAVE:
	case WM_NCMOUSEMOVE:
	case WM_NCDESTROY:
	case WM_NCACTIVATE:
	case WM_IME_SETCONTEXT:
	case WM_IME_NOTIFY:
	case WM_ACTIVATE:
	case WM_ACTIVATEAPP:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
		break;
	case WM_MOUSEMOVE:
		view->handleMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_MOUSELEAVE:
		view->handleMouseLeave();
		break;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP: {
		core::InputKeyCode key;
		int scancode;
		WORD keyFlags = HIWORD(lParam);
		scancode = (HIWORD(lParam) & (KF_EXTENDED | 0xff));
		if (!scancode) {
			// NOTE: Some synthetic key messages have a scancode of zero
			// HACK: Map the virtual key back to a usable scancode
			scancode = MapVirtualKeyW((UINT) wParam, MAPVK_VK_TO_VSC);
		}

		// HACK: Alt+PrtSc has a different scancode than just PrtSc
		if (scancode == 0x54)
			scancode = 0x137;

		// HACK: Ctrl+Pause has a different scancode than just Pause
		if (scancode == 0x146)
			scancode = 0x45;

		// HACK: CJK IME sets the extended bit for right Shift
		if (scancode == 0x136)
			scancode = 0x36;

		key = view->getWin32()->getKeycodes()[scancode];

		MSG next;
		bool hasNext = PeekMessageW(&next, hwnd, 0, 0, PM_NOREMOVE);

		// The Ctrl keys require special handling
		if (wParam == VK_CONTROL) {
			if (HIWORD(lParam) & KF_EXTENDED) {
				// Right side keys have the extended key bit set
				key = core::InputKeyCode::RIGHT_CONTROL;
			} else {
				// NOTE: Alt Gr sends Left Ctrl followed by Right Alt
				// HACK: We only want one event for Alt Gr, so if we detect
				//       this sequence we discard this Left Ctrl message now
				//       and later report Right Alt normally
				const DWORD time = GetMessageTime();
				if (hasNext && (next.message == WM_KEYDOWN || next.message == WM_SYSKEYDOWN
						|| next.message == WM_KEYUP || next.message == WM_SYSKEYUP)) {
					if (next.wParam == VK_MENU && (HIWORD(next.lParam) & KF_EXTENDED) && next.time == time) {
						// Next message is Right Alt down so discard this
						return DefWindowProcW(hwnd, uMsg, wParam, lParam);
					}
				}

				// This is a regular Left Ctrl message
				key = core::InputKeyCode::LEFT_CONTROL;
			}
		} else if (wParam == VK_PROCESSKEY) {
			// IME notifies that keys have been filtered by setting the
			// virtual key-code to VK_PROCESSKEY}
	    	return DefWindowProc(hwnd, uMsg, wParam, lParam);
			break;
		}

		char32_t c = 0;
		if (hasNext && (next.message == WM_CHAR || next.message == WM_SYSCHAR)) {
			c = next.wParam;
			PeekMessageW(&next, hwnd, 0, 0, PM_REMOVE);
			TranslateMessage(&next);
			if (next.wParam >= 0xd800 && next.wParam <= 0xdbff) {
				auto highSurrogate = (WCHAR) wParam;
				// remove without processing
				hasNext = PeekMessageW(&next, hwnd, 0, 0, PM_NOREMOVE);
				if (hasNext && (next.message == WM_CHAR || next.message == WM_SYSCHAR)) {
					PeekMessageW(&next, hwnd, 0, 0, PM_REMOVE);
					TranslateMessage(&next);
					if (next.wParam >= 0xdc00 && next.wParam <= 0xdfff) {
						c += (highSurrogate - 0xd800) << 10;
						c += (WCHAR) next.wParam - 0xdc00;
						c += 0x10000;
					}
				}
			}
		}

		/*if (action == GLFW_RELEASE && wParam == VK_SHIFT) {
			// HACK: Release both Shift keys on Shift up event, as when both
			//       are pressed the first release does not emit any event
			// NOTE: The other half of this is in _glfwPollEventsWin32
			_glfwInputKey(window, GLFW_KEY_LEFT_SHIFT, scancode, action, mods);
			_glfwInputKey(window, GLFW_KEY_RIGHT_SHIFT, scancode, action, mods);
		} else*/ if (wParam == VK_SNAPSHOT) {
			// HACK: Key down is not reported for the Print Screen key
			view->handleKeyPress(key, scancode, c);
			view->handleKeyRelease(key, scancode, c);
		} else {
			if ((HIWORD(lParam) & KF_UP)) {
				view->handleKeyRelease(key, scancode, c);
			} else {
				if ((keyFlags & KF_REPEAT) == KF_REPEAT) {
					view->handleKeyRepeat(key, scancode, c, LOWORD(lParam));
				} else {
					view->handleKeyPress(key, scancode, c);
				}
			}
		}
    	return DefWindowProc(hwnd, uMsg, wParam, lParam);
		break;
	}
    case WM_SETFOCUS:
		view->handleFocus(true);
		break;
    case WM_KILLFOCUS:
		view->handleFocus(false);
		break;
	case WM_CHAR:
	case WM_SYSCHAR:
		view->handleChar((WCHAR)wParam);
		return 0;
		break;
	 case WM_UNICHAR:
		if (wParam == UNICODE_NOCHAR) {
			return TRUE;
		}

		view->handleChar((uint32_t)wParam);
		return 0;
		break;

     case WM_LBUTTONDOWN:
     case WM_RBUTTONDOWN:
     case WM_MBUTTONDOWN:
     case WM_XBUTTONDOWN:
     case WM_LBUTTONUP:
     case WM_RBUTTONUP:
     case WM_MBUTTONUP:
     case WM_XBUTTONUP: {
		core::InputMouseButton button = core::InputMouseButton::None;
		core::InputEventName action = core::InputEventName::Begin;

		if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP) {
			button = core::InputMouseButton::MouseLeft;
		} else if (uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONUP) {
			button = core::InputMouseButton::MouseRight;
		} else if (uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONUP) {
			button = core::InputMouseButton::MouseMiddle;
		} else if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) {
			button = core::InputMouseButton::Mouse8;
		} else {
			button = core::InputMouseButton::Mouse9;
		}

		if (uMsg == WM_LBUTTONDOWN || uMsg == WM_RBUTTONDOWN || uMsg == WM_MBUTTONDOWN || uMsg == WM_XBUTTONDOWN) {
			action = core::InputEventName::Begin;
		} else {
			action = core::InputEventName::End;
		}

		view->handleMouseEvent(button, action);

		if (uMsg == WM_XBUTTONDOWN || uMsg == WM_XBUTTONUP)
			return TRUE;

		return 0;
	}
	case WM_MOUSEWHEEL: {
		auto val = (SHORT) HIWORD(wParam);
		view->handleMouseWheel(0.0f, val / (float) WHEEL_DELTA);
		return 0;
		break;
	}

	case WM_MOUSEHWHEEL: {
		auto val = (SHORT) HIWORD(wParam);
		// This message is only sent on Windows Vista and later
		// NOTE: The X-axis is inverted for consistency with macOS and X11
		view->handleMouseWheel(-(val / (float) WHEEL_DELTA), 0.0f);
		return 0;
		break;
	}

	default:
		XL_WIN32_LOG("Event: ", uMsg);
    	return DefWindowProc(hwnd, uMsg, wParam, lParam);
		break;
	}

	return 0;
}

}
