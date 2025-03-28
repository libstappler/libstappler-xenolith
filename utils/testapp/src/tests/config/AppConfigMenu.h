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

#ifndef TEST_SRC_TESTS_CONFIG_APPCONFIGMENU_H_
#define TEST_SRC_TESTS_CONFIG_APPCONFIGMENU_H_

#include "AppLayoutTest.h"
#include "XL2dScrollView.h"

namespace stappler::xenolith::app {

class AppDelegate;

class ConfigMenu : public LayoutTest {
public:
	enum ApplyMask {
		ApplyPresentMode
	};

	virtual ~ConfigMenu() { }

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleContentSizeDirty() override;

protected:
	using LayoutTest::init;

	void makeScrollList(ScrollController *c);

	void updateAppData(AppDelegate *);
	void updatePresentMode(core::PresentMode);
	void updateApplyButton();

	void applyConfig();

	ScrollView *_scrollView = nullptr;

	uint64_t _currentRate = maxOf<uint64_t>();
	core::PresentMode _currentMode = core::PresentMode::Unsupported;

	Map<ApplyMask, uint32_t> _applyData;
};

}

#endif /* TEST_SRC_TESTS_CONFIG_APPCONFIGMENU_H_ */
