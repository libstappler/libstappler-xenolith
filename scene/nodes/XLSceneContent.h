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

#ifndef XENOLITH_SCENE_NODES_XLSCENECONTENT_H_
#define XENOLITH_SCENE_NODES_XLSCENECONTENT_H_

#include "XLDynamicStateNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class SP_PUBLIC SceneContent : public DynamicStateNode {
public:
	virtual ~SceneContent();

	virtual bool init() override;

	virtual void onEnter(Scene *) override;
	virtual void onExit() override;

	virtual void onContentSizeDirty() override;

	virtual bool onBackButton();

	virtual void setHandlesViewDecoration(bool);
	virtual bool isHandlesViewDecoration() const { return _handlesViewDecoration; }

	virtual void showViewDecoration();
	virtual void hideViewDecoration();

	Padding getDecorationPadding() const { return _decorationPadding; }

protected:
	friend class Scene;

	virtual void setDecorationPadding(Padding);

	virtual void updateBackButtonStatus();

	virtual void handleBackgroundTransition(bool value);

	Padding _decorationPadding;
	InputListener *_inputListener = nullptr;

	bool _retainBackButton = false;
	bool _backButtonRetained = false;
	bool _handlesViewDecoration = true;
	bool _decorationVisible = true;
};

}

#endif /* XENOLITH_SCENE_NODES_XLSCENECONTENT_H_ */
