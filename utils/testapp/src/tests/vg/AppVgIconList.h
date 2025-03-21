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

#ifndef TEST_SRC_TESTS_VG_APPVGICONLIST_H_
#define TEST_SRC_TESTS_VG_APPVGICONLIST_H_

#include "AppLayoutTest.h"
#include "XL2dScrollView.h"

namespace stappler::xenolith::app {

class VgIconListPopup : public Layer {
public:
	virtual ~VgIconListPopup() { }

	virtual bool init() override;

	virtual void handleContentSizeDirty() override;

	void setString(StringView);

protected:
	using Layer::init;

	Label *_label = nullptr;
};

class VgIconList : public LayoutTest {
public:
	virtual ~VgIconList() { }

	virtual bool init() override;

	virtual void handleContentSizeDirty() override;

protected:
	using LayoutTest::init;

	bool rebuildScroll(ScrollController *);
	void updatePopupLocation(const Vec2 &);

	uint32_t _nCols = 0;
	ScrollView *_scrollView = nullptr;
	VgIconListPopup *_popup = nullptr;
};

}

#endif /* TEST_SRC_TESTS_VG_APPVGICONLIST_H_ */
