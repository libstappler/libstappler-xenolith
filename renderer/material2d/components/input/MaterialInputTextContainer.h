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

#ifndef XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_INPUT_MATERIALINPUTTEXTCONTAINER_H_
#define XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_INPUT_MATERIALINPUTTEXTCONTAINER_H_

#include "MaterialLabel.h"
#include "XLTextInputManager.h"

namespace stappler::xenolith::material2d {

class InputTextContainer : public DynamicStateNode {
public:
	virtual ~InputTextContainer();

	virtual bool init() override;
	virtual void onContentSizeDirty() override;

	virtual bool visitDraw(FrameInfo &, NodeFlags parentFlags) override;

	TypescaleLabel *getLabel() const { return _label; }

	virtual void setEnabled(bool);
	virtual bool isEnabled() const { return _enabled; }

	virtual void setCursor(TextCursor);

	virtual void handleLabelChanged();
	virtual void handleLabelPositionChanged();

protected:
	virtual void updateCursorPosition();
	virtual void runAdjustLabel(float pos);

	TypescaleLabel *_label = nullptr;
	Vec2 _adjustment = Vec2::ZERO;
	Layer *_caret = nullptr;
	TextCursor _cursor = TextCursor::InvalidCursor;
	bool _enabled = false;
	bool _cursorDirty = false;
};

}

#endif /* XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_INPUT_MATERIALINPUTTEXTCONTAINER_H_ */