/**
 Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLDirector.h"
#include "XLScheduler.h"
#include "XLScene.h"
#include "XLInputDispatcher.h"
#include "XLTextInputManager.h"
#include "XLActionManager.h"
#include "XLResourceCache.h"
#include "XLCoreLoop.h"
#include "XLCoreFrameRequest.h"
#include "XLView.h"

namespace stappler::xenolith {

Director::Director() { }

Director::~Director() { }

bool Director::init(MainLoop *main, const core::FrameContraints &constraints, View *view) {
	_mainLoop = main;
	_view = view;
	_pool = Rc<PoolRef>::alloc();
	_pool->perform([&] {
		_scheduler = Rc<Scheduler>::create();
		_actionManager = Rc<ActionManager>::create();
		_inputDispatcher = Rc<InputDispatcher>::create(_pool, view);
	});
	_startTime = _mainLoop->getClock();
	_time.global = 0;
	_time.app = 0;
	_time.delta = 0;

	_constraints = constraints;

	updateGeneralTransform();

	return true;
}

TextInputManager *Director::getTextInputManager() const {
	return _inputDispatcher->getTextInputManager();
}

const Rc<ResourceCache> &Director::getResourceCache() const {
	return _mainLoop->getResourceCache();
}

bool Director::acquireFrame(const Rc<FrameRequest> &req) {
	if (!_scene) {
		return false;
	}

	auto t = _mainLoop->getClock();

	if (_constraints != req->getFrameConstraints()) {
		_constraints = req->getFrameConstraints();
		if (_scene) {
			_scene->setFrameConstraints(_constraints);
		}

		updateGeneralTransform();
	}

	update(t);
	if (_scene) {
		req->setQueue(_scene->getQueue());
	}

	_mainLoop->performOnMainThread([this, req] {
		req->getPool()->perform([&] {
			_scene->renderRequest(req);

			if (hasActiveInteractions()) {
				_view->setReadyForNextFrame();
			}
		});
	}, this, true);

	_avgFrameTime.addValue(_mainLoop->getClock() - t);
	_avgFrameTimeValue = _avgFrameTime.getAverage(true);
	return true;
}

void Director::update(uint64_t t) {
	if (_time.global) {
		_time.delta = t - _time.global;
	} else {
		_time.delta = 0;
	}

	_time.global = t;
	_time.app = t - _startTime;

    // If we are debugging our code, prevent big delta time
    if (_time.delta && _time.delta > config::MaxDirectorDeltaTime) {
    	_time.delta = config::MaxDirectorDeltaTime;
    }

    _time.dt = float(_time.delta) / 1'000'000;

	if (_nextScene) {
		if (_scene) {
			_scene->onFinished(this);
		}
		_scene = _nextScene;

		_scene->setFrameConstraints(_constraints);
		_scene->onPresented(this);
		_nextScene = nullptr;
	}

	_inputDispatcher->update(_time);
	_scheduler->update(_time);
	_actionManager->update(_time);

	_mainLoop->getResourceCache()->update(_time);

	_autorelease.clear();
}

void Director::end() {
	if (_scene) {
		_scene->onFinished(this);
		_scene = nullptr;
		_nextScene = nullptr;
	}
}

core::Loop *Director::getGlLoop() const {
	return _mainLoop->getGlLoop();
}

void Director::runScene(Rc<Scene> &&scene) {
	if (!scene) {
		return;
	}

	auto linkId = retain();
	auto &queue = scene->getQueue();
	getGlLoop()->compileQueue(queue, [this, scene = move(scene), linkId, view = Rc<View>(_view)] (bool success) mutable {
		if (success) {
			_mainLoop->performOnMainThread([this, scene = move(scene), view] {
				_nextScene = scene;
				if (!_scene) {
					_scene = _nextScene;
					_nextScene = nullptr;
					_scene->setFrameConstraints(_constraints);
					updateGeneralTransform();
					_scene->onPresented(this);
					getGlLoop()->performOnGlThread([view, scene = _scene] {
						auto &q = scene->getQueue();
						view->runWithQueue(q);
					}, this);
				}
			}, this);
		}
		release(linkId);
	});
}

void Director::pushDrawStat(const DrawStat &stat) {
	_mainLoop->performOnMainThread([this, stat] {
		_drawStat = stat;
	}, this);
}

float Director::getFps() const {
	return 1.0f / (_view->getLastFrameInterval() / 1000000.0f);
}

float Director::getAvgFps() const {
	return 1.0f / (_view->getAvgFrameInterval() / 1000000.0f);
}

float Director::getSpf() const {
	return _view->getLastFrameTime() / 1000.0f;
}

float Director::getLocalFrameTime() const {
	return _view->getAvgFenceTime() / 1000.0f;
}

void Director::autorelease(Ref *ref) {
	_autorelease.emplace_back(ref);
}

void Director::invalidate() {

}

void Director::updateGeneralTransform() {
	Mat4 proj;
	switch (_constraints.transform) {
	case core::SurfaceTransformFlags::Rotate90: proj = Mat4::ROTATION_Z_90; break;
	case core::SurfaceTransformFlags::Rotate180: proj = Mat4::ROTATION_Z_180; break;
	case core::SurfaceTransformFlags::Rotate270: proj = Mat4::ROTATION_Z_270; break;
	case core::SurfaceTransformFlags::Mirror: break;
	case core::SurfaceTransformFlags::MirrorRotate90: break;
	case core::SurfaceTransformFlags::MirrorRotate180: break;
	case core::SurfaceTransformFlags::MirrorRotate270: break;
	default: proj = Mat4::IDENTITY; break;
	}
	proj.scale(2.0f / _constraints.extent.width, -2.0f / _constraints.extent.height, -1.0);
	proj.m[12] = -1.0;
	proj.m[13] = 1.0f;
	proj.m[14] = 0.0f;
	proj.m[15] = 1.0f;

	switch (_constraints.transform) {
	case core::SurfaceTransformFlags::Rotate90: proj.m[13] = -1.0f; break;
	case core::SurfaceTransformFlags::Rotate180: proj.m[12] = 1.0f; proj.m[13] = -1.0f; break;
	case core::SurfaceTransformFlags::Rotate270: proj.m[12] = 1.0f; break;
	case core::SurfaceTransformFlags::Mirror: break;
	case core::SurfaceTransformFlags::MirrorRotate90: break;
	case core::SurfaceTransformFlags::MirrorRotate180: break;
	case core::SurfaceTransformFlags::MirrorRotate270: break;
	default: break;
	}

	_generalProjection = proj;
}

bool Director::hasActiveInteractions() {
	return !_actionManager->empty();
}

}
