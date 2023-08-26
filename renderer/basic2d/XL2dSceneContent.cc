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

#include "XL2dSceneContent.h"
#include "XL2dFrameContext.h"
#include "XL2dSceneLayout.h"
#include "XL2dSceneLight.h"
#include "XLDirector.h"
#include "XLView.h"

namespace stappler::xenolith::basic2d {

SceneContent2d::~SceneContent2d() { }

bool SceneContent2d::init() {
	if (!SceneContent::init()) {
		return false;
	}

	_2dContext = Rc<FrameContext2d>::create();
	_frameContext = _2dContext;

	return true;
}

void SceneContent2d::onContentSizeDirty() {
	SceneContent::onContentSizeDirty();

	for (auto &node : _layouts) {
		updateLayoutNode(node);
	}

	for (auto &overlay : _overlays) {
		updateLayoutNode(overlay);
	}
}

void SceneContent2d::replaceLayout(SceneLayout2d *node) {
	if (!node || node->isRunning()) {
		return;
	}

	if (_layouts.empty()) {
		pushLayout(node);
		return;
	}

	updateLayoutNode(node);

	ZOrder zIndex = -ZOrder(_layouts.size()) - ZOrder(2);
	for (auto n : _layouts) {
		n->setLocalZOrder(zIndex);
		zIndex ++;
	}

	_layouts.push_back(node);
	addChild(node, zIndex);

	for (auto &it : _layouts) {
		if (it != node) {
			it->onPopTransitionBegan(this, true);
		} else {
			it->onPush(this, true);
		}
	}

	auto fn = [this, node] {
		for (auto &it : _layouts) {
			if (it != node) {
				it->onPop(this, true);
			} else {
				it->onPushTransitionEnded(this, true);
			}
		}
		replaceNodes();
		updateBackButtonStatus();
	};

	if (auto enter = node->makeEnterTransition(this)) {
		node->runAction(Rc<Sequence>::create(enter, fn));
	} else {
		fn();
	}
}

void SceneContent2d::pushLayout(SceneLayout2d *node) {
	if (!node || node->isRunning()) {
		return;
	}

	updateLayoutNode(node);
	pushNodeInternal(node, nullptr);
}

void SceneContent2d::replaceTopLayout(SceneLayout2d *node) {
	if (!node || node->isRunning()) {
		return;
	}

	if (!_layouts.empty()) {
		auto back = _layouts.back();
		_layouts.pop_back();
		back->onPopTransitionBegan(this, false);

		// just push node, then silently remove previous

		pushNodeInternal(node, [this, back] {
			eraseLayout(back);
			back->onPop(this, false);
		});
	}
}

void SceneContent2d::popLayout(SceneLayout2d *node) {
	auto it = std::find(_layouts.begin(), _layouts.end(), node);
	if (it == _layouts.end()) {
		return;
	}

	auto linkId = node->retain();
	_layouts.erase(it);

	node->onPopTransitionBegan(this, false);
	if (!_layouts.empty()) {
		_layouts.back()->onForegroundTransitionBegan(this, node);
	}

	auto fn = [this, node, linkId] {
		eraseLayout(node);
		node->onPop(this, false);
		if (!_layouts.empty()) {
			_layouts.back()->onForeground(this, node);
		}
		node->release(linkId);
	};

	if (auto exit = node->makeExitTransition(this)) {
		node->runAction(Rc<Sequence>::create(move(exit), fn));
	} else {
		fn();
	}
}

void SceneContent2d::pushNodeInternal(SceneLayout2d *node, Function<void()> &&cb) {
	if (!_layouts.empty()) {
		ZOrder zIndex = -ZOrder(_layouts.size()) - ZOrder(2);
		for (auto &n : _layouts) {
			n->setLocalZOrder(zIndex);
			zIndex ++;
		}
	}

	_layouts.push_back(node);

	updateLayoutNode(node);
	addChild(node, ZOrder(-1));

	if (_layouts.size() > 1) {
		_layouts.at(_layouts.size() - 2)->onBackground(this, node);
	}
	node->onPush(this, false);

	auto fn = [this, node, cb = move(cb)] {
		updateNodesVisibility();
		updateBackButtonStatus();
		if (_layouts.size() > 1) {
			_layouts.at(_layouts.size() - 2)->onBackgroundTransitionEnded(this, node);
		}
		node->onPushTransitionEnded(this, false);
		if (cb) {
			cb();
		}
	};

	if (auto enter = node->makeEnterTransition(this)) {
		node->runAction(Rc<Sequence>::create(move(enter), move(fn)));
	} else {
		fn();
	}
}

void SceneContent2d::eraseLayout(SceneLayout2d *node) {
	node->removeFromParent();
	if (!_layouts.empty()) {
		ZOrder zIndex = -ZOrder(_layouts.size()) - ZOrder(2);
		for (auto n : _layouts) {
			n->setLocalZOrder(zIndex);
			n->setVisible(false);
			zIndex ++;
		}
		updateNodesVisibility();
	}
	updateBackButtonStatus();
}

void SceneContent2d::eraseOverlay(SceneLayout2d *l) {
	l->removeFromParent();
	if (!_overlays.empty()) {
		ZOrder zIndex = ZOrder(1);
		for (auto n : _overlays) {
			n->setLocalZOrder(zIndex);
			zIndex ++;
		}
		updateNodesVisibility();
	}
	updateBackButtonStatus();
}

void SceneContent2d::replaceNodes() {
	if (!_layouts.empty()) {
		auto vec = _layouts;
		for (auto &node : vec) {
			if (node != _layouts.back()) {
				node->removeFromParent();
			}
		}

		_layouts.erase(_layouts.begin(), _layouts.begin() + _layouts.size() - 1);
	}
}

void SceneContent2d::updateNodesVisibility() {
	for (size_t i = 0; i < _layouts.size(); i++) {
		if (i == _layouts.size() - 1) {
			_layouts.at(i)->setVisible(true);
		} else {
			_layouts.at(i)->setVisible(false);
		}
	}
}

void SceneContent2d::updateBackButtonStatus() {
	if (!_overlays.empty() || _layouts.size() > 1 || _layouts.back()->hasBackButtonAction()) {
		if (!_retainBackButton) {
			_retainBackButton = true;
			if (_director && !_backButtonRetained) {
				_director->getView()->retainBackButton();
				_backButtonRetained = true;
			}
		}
	} else {
		if (_retainBackButton) {
			if (_director && _backButtonRetained) {
				_director->getView()->releaseBackButton();
				_backButtonRetained = false;
			}
			_retainBackButton = false;
		}
	}
}

SceneLayout2d *SceneContent2d::getTopLayout() {
	if (!_layouts.empty()) {
		return _layouts.back();
	} else {
		return nullptr;
	}
}

SceneLayout2d *SceneContent2d::getPrevLayout() {
	if (_layouts.size() > 1) {
		return *(_layouts.end() - 2);
	}
	return nullptr;
}

bool SceneContent2d::popTopLayout() {
	if (_layouts.size() > 1) {
		popLayout(_layouts.back());
		return true;
	}
	return false;
}

bool SceneContent2d::isActive() const {
	return !_layouts.empty();
}

bool SceneContent2d::onBackButton() {
	if (_layouts.empty()) {
		return false;
	} else {
		if (!_layouts.back()->onBackButton()) {
			if (!popTopLayout()) {
				return false;
			}
		}
		return true;
	}
}

size_t SceneContent2d::getLayoutsCount() const {
	return _layouts.size();
}

const Vector<Rc<SceneLayout2d>> &SceneContent2d::getLayouts() const {
	return _layouts;
}

const Vector<Rc<SceneLayout2d>> &SceneContent2d::getOverlays() const {
	return _overlays;
}

bool SceneContent2d::pushOverlay(SceneLayout2d *l) {
	if (!l || l->isRunning()) {
		return false;
	}

	updateLayoutNode(l);

	ZOrder zIndex = ZOrder(_overlays.size() + 1);

	_overlays.push_back(l);

	addChild(l, zIndex);

	l->onPush(this, false);

	auto fn = [this, l] {
		l->onPushTransitionEnded(this, false);
		updateBackButtonStatus();
	};

	if (auto enter = l->makeEnterTransition(this)) {
		l->runAction(Rc<Sequence>::create(move(enter), move(fn)), "ContentLayer.Transition"_tag);
	} else {
		fn();
	}

	return true;
}

bool SceneContent2d::popOverlay(SceneLayout2d *l) {
	auto it = std::find(_overlays.begin(), _overlays.end(), l);
	if (it == _overlays.end()) {
		return false;
	}

	auto linkId = l->retain();
	_overlays.erase(it);
	l->onPopTransitionBegan(this, false);

	auto fn = [this, l, linkId] {
		eraseOverlay(l);
		l->onPop(this, false);
		l->release(linkId);
		updateBackButtonStatus();
	};

	if (auto exit = l->makeExitTransition(this)) {
		l->runAction(Rc<Sequence>::create(move(exit), move(fn)));
	} else {
		fn();
	}

	return true;
}

void SceneContent2d::updateLayoutNode(SceneLayout2d *node) {
	auto mask = node->getDecodationMask();

	Vec2 pos = Vec2::ZERO;
	Size2 size = _contentSize;
	Padding effectiveDecorations;

	if ((mask & DecorationMask::Top) != DecorationMask::None) {
		size.height += _decorationPadding.top;
		effectiveDecorations.top = _decorationPadding.top;
	}
	if ((mask & DecorationMask::Right) != DecorationMask::None) {
		size.width += _decorationPadding.right;
		effectiveDecorations.right = _decorationPadding.right;
	}
	if ((mask & DecorationMask::Left) != DecorationMask::None) {
		size.width += _decorationPadding.left;
		pos.x -= _decorationPadding.left;
		effectiveDecorations.left = _decorationPadding.left;
	}
	if ((mask & DecorationMask::Bottom) != DecorationMask::None) {
		size.height += _decorationPadding.bottom;
		pos.y -= _decorationPadding.bottom;
		effectiveDecorations.bottom = _decorationPadding.bottom;
	}

	node->setAnchorPoint(Anchor::BottomLeft);
	node->setDecorationPadding(effectiveDecorations);
	node->setPosition(pos);
	node->setContentSize(size);
}

bool SceneContent2d::addLight(SceneLight *light, uint64_t tag, StringView name) {
	if (tag != InvalidTag) {
		if (_lightsByTag.find(tag) != _lightsByTag.end()) {
			log::warn("Scene", "Light with tag ", tag, " is already defined");
			return false;
		}
	}
	if (!name.empty()) {
		if (_lightsByName.find(name) != _lightsByName.end()) {
			log::warn("Scene", "Light with name ", name, " is already defined");
			return false;
		}
	}

	if (light->getScene()) {
		log::warn("Scene", "Light is already on scene");
		return false;
	}

	switch (light->getType()) {
	case SceneLightType::Ambient:
		if (_lightsAmbientCount >= config::MaxAmbientLights) {
			log::warn("Scene", "Too many ambient lights");
			return false;
		}
		break;
	case SceneLightType::Direct:
		if (_lightsDirectCount >= config::MaxDirectLights) {
			log::warn("Scene", "Too many direct lights");
			return false;
		}
		break;
	}

	_lights.emplace_back(light);

	switch (light->getType()) {
	case SceneLightType::Ambient: ++ _lightsAmbientCount; break;
	case SceneLightType::Direct: ++ _lightsDirectCount; break;
	}

	if (tag != InvalidTag) {
		light->setTag(tag);
		_lightsByTag.emplace(tag, light);
	}

	if (!name.empty()) {
		light->setName(name);
		_lightsByName.emplace(light->getName(), light);
	}

	if (_running) {
		light->onEnter(_scene);
	} else {
		light->_scene = _scene;
	}

	return true;
}

SceneLight *SceneContent2d::getLightByTag(uint64_t tag) const {
	auto it = _lightsByTag.find(tag);
	if (it != _lightsByTag.end()) {
		return it->second;
	}
	return nullptr;
}

SceneLight *SceneContent2d::getLightByName(StringView name) const {
	auto it = _lightsByName.find(name);
	if (it != _lightsByName.end()) {
		return it->second;
	}
	return nullptr;
}

void SceneContent2d::removeLight(SceneLight *light) {
	if (light->getScene() != _scene) {
		return;
	}

	auto it = _lights.begin();
	while (it != _lights.end()) {
		if (*it == light) {
			removeLight(it);
			return;
		}
		++ it;
	}
}

void SceneContent2d::removeLightByTag(uint64_t tag) {
	if (auto l = getLightByTag(tag)) {
		removeLight(l);
	}
}

void SceneContent2d::removeLightByName(StringView name) {
	if (auto l = getLightByName(name)) {
		removeLight(l);
	}
}

void SceneContent2d::removeAllLights() {
	auto it = _lights.begin();
	while (it != _lights.end()) {
		it = removeLight(it);
	}
}

void SceneContent2d::removeAllLightsByType(SceneLightType type) {
	auto it = _lights.begin();
	while (it != _lights.end()) {
		if ((*it)->getType() == type) {
			it = removeLight(it);
		} else {
			++ it;
		}
	}
}

void SceneContent2d::setGlobalLight(const Color4F &color) {
	_globalLight = color;
}

const Color4F & SceneContent2d::getGlobalLight() const {
	return _globalLight;
}

void SceneContent2d::draw(FrameInfo &info, NodeFlags flags) {
	SceneContent::draw(info, flags);

	FrameContextHandle2d *ctx = reinterpret_cast<FrameContextHandle2d *>(info.currentContext);

	auto &constraints = _scene->getFrameConstraints();

	Size2 scaledExtent(constraints.extent.width / constraints.density, constraints.extent.height / constraints.density);

	ctx->lights.sceneDensity = constraints.density;
	ctx->lights.shadowDensity = _shadowDensity;
	ctx->lights.globalColor = _globalLight;

	for (auto &it : _lights) {
		switch (it->getType()) {
		case SceneLightType::Ambient:
			ctx->lights.addAmbientLight(Vec4(it->getNormal().x / scaledExtent.width, -it->getNormal().y / scaledExtent.height,
					it->getNormal().z, it->getNormal().w), it->getColor(), it->isSoftShadow());
			break;
		case SceneLightType::Direct:
			ctx->lights.addDirectLight(Vec4(it->getNormal().x / scaledExtent.width, -it->getNormal().y / scaledExtent.height,
					it->getNormal().z, it->getNormal().w), it->getColor(), it->getData());
			break;
		}
	}
}

Vector<Rc<SceneLight>>::iterator SceneContent2d::removeLight(Vector<Rc<SceneLight>>::iterator itVec) {
	if ((*itVec)->isRunning()) {
		(*itVec)->onExit();
	}

	if (!(*itVec)->getName().empty()) {
		auto it = _lightsByName.find((*itVec)->getName());
		if (it != _lightsByName.end()) {
			_lightsByName.erase(it);
		}
	}

	if ((*itVec)->getTag() != InvalidTag) {
		auto it = _lightsByTag.find((*itVec)->getTag());
		if (it != _lightsByTag.end()) {
			_lightsByTag.erase(it);
		}
	}

	switch ((*itVec)->getType()) {
	case SceneLightType::Ambient: -- _lightsAmbientCount; break;
	case SceneLightType::Direct: -- _lightsDirectCount; break;
	}

	return _lights.erase(itVec);
}

}
