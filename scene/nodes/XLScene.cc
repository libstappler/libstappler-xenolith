/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLScene.h"

#include "XLFrameInfo.h"
#include "XLInputDispatcher.h"
#include "XLDirector.h"
#include "XLSceneContent.h"
#include "XLCoreFrameRequest.h"

namespace stappler::xenolith {

Scene::~Scene() {
	_queue = nullptr;
}

bool Scene::init(Queue::Builder &&builder, const core::FrameContraints &constraints) {
	if (!Node::init()) {
		return false;
	}

	setLocalZOrder(ZOrderTransparent);

	_queue = makeQueue(move(builder));

	setFrameConstraints(_constraints);

	return true;
}

void Scene::renderRequest(const Rc<FrameRequest> &req) {
	if (!_director) {
		return;
	}

	Size2 scaledExtent(_constraints.extent.width / _constraints.density, _constraints.extent.height / _constraints.density);

	FrameInfo info;
	info.request = req;
	info.pool = req->getPool();

	render(info);

	if (info.resolvedInputs.size() != _queue->getInputAttachments().size()) {
		for (auto &it : _queue->getInputAttachments()) {
			auto iit = info.resolvedInputs.find(it);
			if (iit == info.resolvedInputs.end()) {
				log::vtext("Scene", "No input defined for attachment: ", it->key);
			}
		}
	}
}

void Scene::render(FrameInfo &info) {
	info.director = _director;
	info.scene = this;
	info.zPath.reserve(8);

	info.viewProjectionStack.reserve(2);
	info.viewProjectionStack.push_back(_director->getGeneralProjection());

	info.modelTransformStack.reserve(8);
	info.modelTransformStack.push_back(Mat4::IDENTITY);

	info.depthStack.reserve(4);
	info.depthStack.push_back(0.0f);

	auto eventDispatcher = _director->getInputDispatcher();

	info.input = eventDispatcher->acquireNewStorage();

	visitGeometry(info, NodeFlags::None);
	visitDraw(info, NodeFlags::None);

	eventDispatcher->commitStorage(move(info.input));
}

void Scene::onEnter(Scene *scene) {
	Node::onEnter(scene);
}

void Scene::onExit() {
	Node::onExit();
}

void Scene::onContentSizeDirty() {
	Node::onContentSizeDirty();

	setAnchorPoint(Anchor::Middle);
	setPosition(Vec2((_contentSize * _constraints.density) / 2.0f));

	updateContentNode(_content);

#if DEBUG
	log::vtext("Scene", "ContentSize: ", _contentSize, " density: ", _constraints.density);
#endif
}

void Scene::setContent(SceneContent *content) {
	if (_content) {
		_content->removeFromParent(true);
	}
	_content = nullptr;
	if (content) {
		_content = addChild(content);
		updateContentNode(_content);
	}
}

void Scene::onPresented(Director *dir) {
	_director = dir;
	if (getContentSize() == Size2::ZERO) {
		setContentSize(_constraints.getScreenSize() / _constraints.density);
	}

	if (auto res = _queue->getInternalResource()) {
		dir->getResourceCache()->addResource(res);
	}

	onEnter(this);
}

void Scene::onFinished(Director *dir) {
	onExit();

	if (_director == dir) {
		if (auto res = _queue->getInternalResource()) {
			auto cache = dir->getResourceCache();
			if (cache) {
				cache->removeResource(res->getName());
			}
		}

		_director = nullptr;
	}
}

void Scene::onFrameStarted(FrameRequest &req) {
	req.setSceneId(retain());
}

void Scene::onFrameEnded(FrameRequest &req) {
	release(req.getSceneId());
}

void Scene::setFrameConstraints(const core::FrameContraints &constraints) {
	if (_constraints != constraints) {
		auto size = constraints.getScreenSize();

		_constraints = constraints;

		setContentSize(size / _constraints.density);
		setScale(_constraints.density);
		_contentSizeDirty = true;

		setPosition(Vec2((_contentSize * _constraints.density) / 2.0f));

		updateContentNode(_content);
	}
}

const Size2& Scene::getContentSize() const {
	return _content ? _content->getContentSize() : _contentSize;
}

void Scene::setClipContent(bool value) {
	if (_content) {
		if (isClipContent() != value) {
			if (value) {
				_content->enableScissor();
			} else {
				_content->disableScissor();
			}
		}
	}
}

bool Scene::isClipContent() const {
	return _content ? _content->isScissorEnabled() : false;
}

auto Scene::makeQueue(Queue::Builder &&builder) -> Rc<Queue> {
	builder.setBeginCallback([this] (FrameRequest &frame) {
		onFrameStarted(frame);
	});
	builder.setEndCallback([this] (FrameRequest &frame) {
		onFrameEnded(frame);
	});

	return Rc<Queue>::create(move(builder));
}

void Scene::updateContentNode(SceneContent *content) {
	if (content) {
		auto padding = _constraints.contentPadding / _constraints.density;

		content->setPosition(Vec2(padding.left, padding.bottom));
		content->setContentSize(Size2(_contentSize.width - padding.horizontal(), _contentSize.height - padding.vertical()));
		content->setAnchorPoint(Anchor::BottomLeft);
		content->setDecorationPadding(padding);
	}
}

}