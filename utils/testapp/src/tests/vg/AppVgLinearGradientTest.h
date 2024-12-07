/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#ifndef SRC_TESTS_VG_APPVGLINEARGRADIENTTEST_H_
#define SRC_TESTS_VG_APPVGLINEARGRADIENTTEST_H_

#include "AppLayoutTest.h"
#include "XLIcons.h"
#include "XL2dVectorSprite.h"
#include "XL2dLayerRounded.h"
#include "AppCheckbox.h"
#include "AppSlider.h"

namespace stappler::xenolith::app {

class VgLinearGradientTest : public LayoutTest {
public:
	virtual ~VgLinearGradientTest() { }

	virtual bool init() override;

	virtual void handleContentSizeDirty() override;

protected:
	using LayoutTest::init;

	void updateAngle(float val);

	float _angle = 0.0f;
	IconName _currentName = IconName::Action_text_rotate_vertical_solid;

	Label *_label = nullptr;
	Label *_info = nullptr;
	Sprite *_sprite = nullptr;

	SliderWithLabel *_sliderAngle = nullptr;
};

}

#endif /* SRC_TESTS_VG_APPVGLINEARGRADIENTTEST_H_ */
