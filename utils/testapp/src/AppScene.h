/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#ifndef SRC_APPSCENE_H_
#define SRC_APPSCENE_H_

#include "AppTests.h"
#include "XL2dScene.h"
#include "XL2dSprite.h"
#include "XL2dLayer.h"

namespace stappler::xenolith::app {

class AppScene : public Scene2d {
public:
	virtual ~AppScene() { }

	virtual bool init(Application *, const core::FrameConstraints &constraints) override;

	virtual void handlePresented(Director *) override;
	virtual void handleFinished(Director *) override;

	virtual void update(const UpdateTime &) override;
	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	virtual void render(FrameInfo &info) override;

	void runLayout(LayoutName l, Rc<SceneLayout2d> &&);

	void setActiveLayoutId(StringView, Value && = Value());

protected:
	using Scene::init;
};

} // namespace stappler::xenolith::app

#endif /* SRC_APPSCENE_H_ */
