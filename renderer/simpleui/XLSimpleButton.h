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

#ifndef XENOLITH_RENDERER_SIMPLEUI_XLSIMPLEBUTTON_H_
#define XENOLITH_RENDERER_SIMPLEUI_XLSIMPLEBUTTON_H_

#include "XLSimpleUiConfig.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::simpleui {

class Button : public Layer {
public:
	virtual ~Button() { }

	bool init(Function<void()> &&);

	void setEnabled(bool);
	bool isEnabled() const { return _enabled; }

	void setCallback(Function<void()> &&);

protected:
	using Layer::init;

	virtual void handleFocusEnter();
	virtual void handleFocusLeave();
	virtual void handleTouch();
	virtual void updateEnabled();

	Function<void()> _callback;
	InputListener *_listener = nullptr;
	bool _focus = false;
	bool _enabled = false;
};

class ButtonWithLabel : public Button {
public:
	virtual ~ButtonWithLabel() { }

	bool init(StringView, Function<void()> && = nullptr);

	virtual void handleContentSizeDirty() override;

	void setString(StringView);

protected:
	Label *_label = nullptr;
};

} // namespace stappler::xenolith::simpleui

#endif /* XENOLITH_RENDERER_SIMPLEUI_XLSIMPLEBUTTON_H_ */
