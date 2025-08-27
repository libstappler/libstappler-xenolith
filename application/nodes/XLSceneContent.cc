/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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
#include "XLDynamicStateComponent.h"
#include "XLAppWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

SceneContent::~SceneContent() { }

bool SceneContent::init() {
	if (!Node::init()) {
		return false;
	}

	_inputListener = addComponent(Rc<InputListener>::create());
	_inputListener->setPriority(-1);
	_inputListener->setDedicatedFocus(maxOf<uint32_t>());
	_inputListener->addKeyRecognizer([this](GestureData data) {
		if (data.event == GestureEvent::Ended) {
			return onBackButton();
		}
		return data.event == GestureEvent::Began;
	}, InputListener::makeKeyMask({InputKeyCode::ESCAPE}));

	_inputListener->setWindowStateCallback([this](WindowState state, WindowState changes) -> bool {
		if (hasFlag(changes, WindowState::Background)) {
			handleBackgroundTransition(hasFlag(state, WindowState::Background));
		}
		return true;
	});

	_scissorComponent = addComponent(Rc<DynamicStateComponent>::create());

	return true;
}

void SceneContent::handleEnter(Scene *scene) {
	Node::handleEnter(scene);

	if (_retainBackButton && !_backButtonRetained) {
		_director->getWindow()->retainExitGuard();
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

void SceneContent::handleExit() {
	if (_retainBackButton && _backButtonRetained) {
		_director->getWindow()->releaseExitGuard();
		_backButtonRetained = false;
	}

	Node::handleExit();
}

void SceneContent::handleContentSizeDirty() { Node::handleContentSizeDirty(); }

SP_COVERAGE_TRIVIAL
void SceneContent::updateBackButtonStatus() { }

void SceneContent::handleBackgroundTransition(bool val) { }

SP_COVERAGE_TRIVIAL
bool SceneContent::onBackButton() { return false; }

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
			_director->getWindow()->setInsetDecorationVisible(true);
		}
		_decorationVisible = true;
	}
}

void SceneContent::hideViewDecoration() {
	if (_decorationVisible != false) {
		if (_running && _handlesViewDecoration) {
			_director->getWindow()->setInsetDecorationVisible(false);
		}
		_decorationVisible = false;
	}
}

void SceneContent::enableScissor() {
	_scissorComponent->enableScissor();
	_scissorComponent->setStateApplyMode(DynamicStateApplyMode::ApplyForAll);
}

void SceneContent::disableScissor() {
	_scissorComponent->disableScissor();
	_scissorComponent->setStateApplyMode(DynamicStateApplyMode::DoNotApply);
}

bool SceneContent::isScissorEnabled() const { return _scissorComponent->isScissorEnabled(); }

void SceneContent::setDecorationPadding(Padding padding) {
	if (padding != _decorationPadding) {
		_decorationPadding = padding;
		_contentSizeDirty = true;
	}
}

} // namespace stappler::xenolith
