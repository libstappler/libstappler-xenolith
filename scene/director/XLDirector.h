/**
 Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_SCENE_DIRECTOR_XLDIRECTOR_H_
#define XENOLITH_SCENE_DIRECTOR_XLDIRECTOR_H_

#include "XLResourceCache.h"
#include "XLApplication.h"
#include "XLInput.h"
#include "SPMovingAverage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Scene;
class Scheduler;
class InputDispatcher;
class TextInputManager;
class ActionManager;
class View;

class SP_PUBLIC Director : public Ref {
public:
	using FrameRequest = core::FrameRequest;

	virtual ~Director();

	Director();

	bool init(Application *, const core::FrameConstraints &, View *view);

	void setFrameConstraints(const core::FrameConstraints &);

	void runScene(Rc<Scene> &&);

	bool acquireFrame(FrameRequest *);

	void update(uint64_t t);

	void end();

	Application *getApplication() const { return _mainLoop; }
	core::Loop *getGlLoop() const;
	View *getView() const { return _view; }

	Scheduler *getScheduler() const { return _scheduler; }
	ActionManager *getActionManager() const { return _actionManager; }
	InputDispatcher *getInputDispatcher() const { return _inputDispatcher; }
	TextInputManager *getTextInputManager() const;

	const Rc<Scene> &getScene() const { return _scene; }
	const Rc<ResourceCache> &getResourceCache() const;
	const Mat4 &getGeneralProjection() const { return _generalProjection; }

	const core::FrameConstraints &getFrameConstraints() const { return _constraints; }

	void pushDrawStat(const DrawStat &);

	const UpdateTime &getUpdateTime() const { return _time; }
	const DrawStat &getDrawStat() const { return _drawStat; }

	float getFps() const;
	float getAvgFps() const;
	float getSpf() const; // in milliseconds
	float getFenceFrameTime() const;
	float getTimestampFrameTime() const;

	float getDirectorFrameTime() const { return _avgFrameTimeValue / 1000.0f; }

	void autorelease(Ref *);

protected:
	// Vk Swaphain was invalidated, drop all dependent resources;
	void invalidate();

	void updateGeneralTransform();

	bool hasActiveInteractions();

	Rc<Application> _mainLoop;
	Rc<View> _view;
	Rc<core::PresentationEngine> _engine;

	core::FrameConstraints _constraints;

	uint64_t _startTime = 0;
	UpdateTime _time;
	DrawStat _drawStat;

	Rc<Scene> _scene;
	Rc<Scene> _nextScene;

	Mat4 _generalProjection;

	Rc<PoolRef> _pool;
	Rc<Scheduler> _scheduler;
	Rc<ActionManager> _actionManager;
	Rc<InputDispatcher> _inputDispatcher;

	Vector<Rc<Ref>> _autorelease;

	math::MovingAverage<20, uint64_t> _avgFrameTime;
	uint64_t _avgFrameTimeValue = 0;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_SCENE_DIRECTOR_XLDIRECTOR_H_ */
