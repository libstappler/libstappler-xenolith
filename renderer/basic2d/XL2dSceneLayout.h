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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DSCENELAYOUT_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DSCENELAYOUT_H_

#include "XLDynamicStateNode.h"
#include "XLAction.h"

namespace stappler::xenolith::basic2d {

class SceneContent2d;

enum class DecorationMask {
	None,
	Top = 1 << 0,
	Bottom = 1 << 1,
	Left = 1 << 2,
	Right = 1 << 3,
	Vertical = Top | Bottom,
	Horizontal = Left | Right,
	All = Vertical | Horizontal
};

SP_DEFINE_ENUM_AS_MASK(DecorationMask)

enum class DecorationStatus {
	DontCare,
	Visible,
	Hidden
};

class SceneLayout2d : public DynamicStateNode {
public:
	using BackButtonCallback = Function<bool()>;
	using Transition = ActionInterval;

	virtual ~SceneLayout2d();

	virtual void setDecorationMask(DecorationMask mask);
	virtual DecorationMask getDecodationMask() const { return _decorationMask; }

	virtual void setDecorationPadding(Padding);
	virtual Padding getDecorationPadding() const { return _decorationPadding; }

	virtual bool onBackButton();

	virtual void setBackButtonCallback(const BackButtonCallback &);
	virtual const BackButtonCallback &getBackButtonCallback() const;

	virtual void onPush(SceneContent2d *l, bool replace);
	virtual void onPushTransitionEnded(SceneContent2d *l, bool replace);

	virtual void onPopTransitionBegan(SceneContent2d *l, bool replace);
	virtual void onPop(SceneContent2d *l, bool replace);

	virtual void onBackground(SceneContent2d *l, SceneLayout2d *overlay);
	virtual void onBackgroundTransitionEnded(SceneContent2d *l, SceneLayout2d *overlay);

	virtual void onForegroundTransitionBegan(SceneContent2d *l, SceneLayout2d *overlay);
	virtual void onForeground(SceneContent2d *l, SceneLayout2d *overlay);

	virtual Rc<Transition> makeEnterTransition(SceneContent2d *) const;
	virtual Rc<Transition> makeExitTransition(SceneContent2d *) const;

	virtual bool hasBackButtonAction() const;

	virtual void setLayoutName(StringView);
	virtual StringView getLayoutName() const;

	virtual DecorationStatus getDecorationStatus() const { return DecorationStatus::DontCare; }

protected:
	DecorationMask _decorationMask = DecorationMask::None;
	Padding _decorationPadding;
	bool _inTransition = false;
	BackButtonCallback _backButtonCallback;
	SceneContent2d *_sceneContent = nullptr;
	String _name;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DSCENELAYOUT_H_ */
