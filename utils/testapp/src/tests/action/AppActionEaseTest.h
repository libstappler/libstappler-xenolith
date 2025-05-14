/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef TEST_SRC_TESTS_ACTION_APPACTIONEASETEST_H_
#define TEST_SRC_TESTS_ACTION_APPACTIONEASETEST_H_

#include "AppLayoutTest.h"
#include "AppSlider.h"
#include "AppButton.h"
#include "XLActionEase.h"
#include "XLInterpolation.h"
#include "XLCurveBuffer.h"

namespace stappler::xenolith::app {

class ActionEaseNode : public Node {
public:
	virtual ~ActionEaseNode() = default;

	bool init(StringView, Function<Rc<ActionInterval>(Rc<ActionInterval> &&)> &&,
			Function<void()> &&onActivated = nullptr);

	virtual void handleContentSizeDirty() override;

	void run();

	void setTime(float value) { _time = value; }

protected:
	using Node::init;

	float _time = 0.1f;
	Layer *_layer = nullptr;
	Label *_label = nullptr;
	Function<Rc<ActionInterval>(Rc<ActionInterval> &&)> _callback;
	Function<void()> _activated;
};

class ActionEaseTest : public LayoutTest {
public:
	enum class Mode {
		InOut,
		Out,
		In,
	};

	virtual ~ActionEaseTest() = default;

	virtual bool init() override;

	virtual void handleContentSizeDirty() override;

protected:
	using LayoutTest::init;

	Rc<ActionInterval> makeAction(interpolation::Type, Rc<ActionInterval> &&) const;
	interpolation::Type getSelectedType(interpolation::Type) const;

	void setSelected(interpolation::Type);

	Mode _mode = Mode::InOut;
	interpolation::Type _selectedType = interpolation::Type::Linear;
	interpolation::Type _selectedSubType = interpolation::Type::Linear;
	SliderWithLabel *_slider = nullptr;
	ButtonWithLabel *_button = nullptr;
	ButtonWithLabel *_modeButton = nullptr;
	Label *_selectedLabel = nullptr;
	Sprite *_curveSprite = nullptr;
	Vector<ActionEaseNode *> _nodes;

	Rc<CurveBuffer> _curveBuffer;
};

} // namespace stappler::xenolith::app

#endif /* TEST_SRC_TESTS_ACTION_APPACTIONEASETEST_H_ */
