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

#ifndef TEST_SRC_WIDGETS_APPSLIDER_H_
#define TEST_SRC_WIDGETS_APPSLIDER_H_

#include "AppWidgets.h"
#include "XL2dLayer.h"
#include "XL2dLabel.h"

namespace stappler::xenolith::app {

class Slider : public Layer {
public:
	virtual ~Slider() { }

	virtual bool init(float, Function<void(float)> &&);

	virtual void handleContentSizeDirty() override;

	virtual void setValue(float);
	virtual float getValue() const;

	virtual void setForegroundColor(const Color4F &);
	virtual Color4F getForegroundColor() const;

	virtual void setBackgroundColor(const Color4F &);
	virtual Color4F getBackgroundColor() const;

protected:
	using Layer::init;

	virtual void updateValue();

	float _value = 0.0f;
	Function<void(float)> _callback;
	Layer *_foreground = nullptr;

	InputListener *_input = nullptr;
};

class SliderWithLabel : public Slider {
public:
	virtual ~SliderWithLabel() { }

	virtual bool init(StringView, float, Function<void(float)> &&);

	virtual void handleContentSizeDirty() override;

	virtual void setString(StringView);
	virtual StringView getString() const;

	virtual void setPrefix(StringView);
	virtual StringView getPrefix() const;

	virtual void setFontSize(font::FontSize);
	virtual void setFontSize(uint16_t);

protected:
	using Slider::init;

	Label *_prefix = nullptr;
	Label *_label = nullptr;
};

}

#endif /* TEST_SRC_WIDGETS_APPSLIDER_H_ */
