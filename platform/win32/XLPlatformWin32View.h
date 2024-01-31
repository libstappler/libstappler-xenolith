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

#ifndef XENOLITH_PLATFORM_WIN32_XLPLATFORMWIN32VIEW_H_
#define XENOLITH_PLATFORM_WIN32_XLPLATFORMWIN32VIEW_H_

#include "XLPlatformWin32Library.h"
#include "XLPlatformViewInterface.h"
#include "SPPlatformUnistd.h"

#if WIN32

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct Win32ViewInfo {
	StringView bundleId;
	StringView name;
	URect rect;

	Function<void(ViewInterface *)> captureView;
	Function<void(ViewInterface *)> releaseView;
	Function<void(ViewInterface *)> handlePaint;
};

class Win32View : public Ref {
public:
	static LRESULT wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	Win32View();
	virtual ~Win32View();

	bool init(ViewInterface *view, Win32Library *win32, Win32ViewInfo &&);

	uint64_t getScreenFrameInterval() const;

	void dispose();

	void close();

	void mapWindow();

	void schedule(uint64_t);

	HWND getWindow() const { return _window; }
	HMODULE getInstance() const { return _winInstance; }
	Win32Library *getWin32() const { return _win32; }

	bool shouldQuit() const { return _shouldQuit; }

	bool shouldUpdate() {
		if (_shouldUpdate) {
			_shouldUpdate = false;
			return true;
		}
		return false;
	}

	void wakeup();

	void handleTimer();
	void handleWakeup();
	bool handleClose();
	bool handleSize(uint32_t w, uint32_t h, bool maximized, bool minimized);
	void handleEnterSizeMove();
	void handleExitSizeMove();
	void handlePaint();
	void handleMouseMove(int x, int y);
	void handleMouseLeave();

	void handleKeyPress(core::InputKeyCode, int scancode, char32_t c);
	void handleKeyRepeat(core::InputKeyCode, int scancode, char32_t c, int count);
	void handleKeyRelease(core::InputKeyCode, int scancode, char32_t c);

	void handleFocus(bool);

	void handleChar(char32_t);
	void handleMouseEvent(core::InputMouseButton, core::InputEventName);
	void handleMouseWheel(float x, float y);

protected:
	char32_t makeKeyChar(char32_t);

	uint32_t _width = 0;
	uint32_t _height = 0;
	uint32_t _rate = 60;
	Win32ViewInfo _info;
	ViewInterface *_view = nullptr;
	Rc<Win32Library> _win32;
	WideString _className;
	WideString _windowName;
	HMODULE _winInstance = nullptr;
	WNDCLASSW _windowClass;
	HWND _window = nullptr;
	bool _shouldQuit = false;
	bool _shouldUpdate = false;
	bool _inSizeMove = false;
	bool _mouseTracked = false;
	bool _iconified = false;
	Vec2 _cursorPos;
	core::InputModifier _enabledModifiers = core::InputModifier::None;
	char32_t _highSurrogate = 0;
	uint32_t _pointerButtonCapture = 0;

	Vector<Win32Display> _monitors;
};

}

#endif

#endif /* XENOLITH_PLATFORM_WIN32_XLPLATFORMWIN32VIEW_H_ */
