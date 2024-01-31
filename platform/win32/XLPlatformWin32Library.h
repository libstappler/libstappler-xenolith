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

#ifndef XENOLITH_PLATFORM_WIN32_XLPLATFORMWIN32LIBRARY_H_
#define XENOLITH_PLATFORM_WIN32_XLPLATFORMWIN32LIBRARY_H_

#include "XLCommon.h"
#include "XLCoreInput.h"
#include "XLPlatformNetwork.h"

#include "SPPlatformUnistd.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct Win32Display {
	WideString name;
	WideString adapterName;
	WideString displayName;
	int widthMM;
	int heightMM;
	bool modesPruned;
	bool isPrimary;
	DEVMODEW dm;
	RECT rect;
};

class Win32Library : public Ref {
public:
	static Win32Library *getInstance();

	Win32Library() { }

	virtual ~Win32Library();

	bool init();

	const core::InputKeyCode *getKeycodes() const { return _keycodes; }
	const uint16_t *getScancodes() const { return _scancodes; }

	Vector<Win32Display> pollMonitors();

	void addNetworkConnectionCallback(void *key, Function<void(const NetworkCapabilities &)> &&);
	void removeNetworkConnectionCallback(void *key);

protected:
	struct Data;

	void loadKeycodes();

	core::InputKeyCode _keycodes[512];
	uint16_t _scancodes[toInt(core::InputKeyCode::Max)];
	Data *_data = nullptr;
};

}

#endif /* XENOLITH_PLATFORM_WIN32_XLPLATFORMWIN32LIBRARY_H_ */
