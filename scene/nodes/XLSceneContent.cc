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

#include "XLSceneContent.h"
#include "XLInputListener.h"
#include "XLDirector.h"
#include "XLView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

SceneContent::~SceneContent() { }

bool SceneContent::init() {
	if (!DynamicStateNode::init()) {
		return false;
	}

	_inputListener = addInputListener(Rc<InputListener>::create());
	_inputListener->setPriority(-1);
	_inputListener->setDedicatedFocus(maxOf<uint32_t>());
	_inputListener->addKeyRecognizer([this] (GestureData data) {
		if (data.event == GestureEvent::Ended) {
			return onBackButton();
		}
		return data.event == GestureEvent::Began;
	}, InputListener::makeKeyMask({InputKeyCode::ESCAPE}));

	_inputListener->setBackgroudCallback([this] (bool val) -> bool {
		handleBackgroundTransition(val);
		return true;
	});

	return true;
}

void SceneContent::onEnter(Scene *scene) {
	DynamicStateNode::onEnter(scene);

	if (_retainBackButton && !_backButtonRetained) {
		_director->getView()->retainBackButton();
		_backButtonRetained = true;
	}

	if (_handlesViewDecoration) {
		if (_decorationVisible) {
			showViewDecoration();
		} else {
			hideViewDecoration();
		}
	}
}

void SceneContent::onExit() {
	if (_retainBackButton && _backButtonRetained) {
		_director->getView()->releaseBackButton();
		_backButtonRetained = false;
	}

	DynamicStateNode::onExit();
}

void SceneContent::onContentSizeDirty() {
	Node::onContentSizeDirty();
}

void SceneContent::updateBackButtonStatus() { }

void SceneContent::handleBackgroundTransition(bool val) { }

bool SceneContent::onBackButton() {
	return false;
}

void SceneContent::setHandlesViewDecoration(bool value) {
	if (_handlesViewDecoration != value) {
		_handlesViewDecoration = value;
		if (_decorationVisible) {
			showViewDecoration();
		} else {
			hideViewDecoration();
		}
	}
}

void SceneContent::showViewDecoration() {
	if (_decorationVisible != true) {
		if (_running && _handlesViewDecoration) {
			_director->getView()->setDecorationVisible(true);
		}
		_decorationVisible = true;
	}
}

void SceneContent::hideViewDecoration() {
	if (_decorationVisible != false) {
		if (_running && _handlesViewDecoration) {
			_director->getView()->setDecorationVisible(false);
		}
		_decorationVisible = false;
	}
}

void SceneContent::setDecorationPadding(Padding padding) {
	if (padding != _decorationPadding) {
		_decorationPadding = padding;
		_contentSizeDirty = true;
	}
}

}
