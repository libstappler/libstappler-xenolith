/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_SCENE_NODES_XLSCENE_H_
#define XENOLITH_SCENE_NODES_XLSCENE_H_

#include "XLNode.h"
#include "XLCoreResource.h"
#include "XLCoreQueue.h"
#include "XLCoreAttachment.h"
#include "XLCoreMaterial.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Application;
class SceneContent;

class SP_PUBLIC Scene : public Node {
public:
	using Queue = core::Queue;
	using FrameRequest = core::FrameRequest;
	using FrameQueue = core::FrameQueue;
	using FrameHandle = core::FrameHandle;

	virtual ~Scene();

	virtual bool init(Queue::Builder &&, const core::FrameConstraints &);

	virtual void renderRequest(const Rc<FrameRequest> &, PoolRef *pool);
	virtual void render(FrameInfo &info);

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	virtual void handleContentSizeDirty() override;

	const Rc<Queue> &getQueue() const { return _queue; }
	Director *getDirector() const { return _director; }

	virtual void setContent(SceneContent *);
	virtual SceneContent *getContent() const { return _content; }

	virtual void onPresented(Director *);
	virtual void onFinished(Director *);

	virtual void onFrameStarted(FrameRequest &);
	virtual void onFrameEnded(FrameRequest &);

	virtual void onFrameAttached(const FrameHandle *);
	virtual void onFrameDetached(const FrameHandle *);

	virtual void setFrameConstraints(const core::FrameConstraints &);
	const core::FrameConstraints & getFrameConstraints() const { return _constraints; }

	virtual const Size2& getContentSize() const override;

	virtual void setClipContent(bool);
	virtual bool isClipContent() const;

protected:
	using Node::init;
	using Node::addChild; // запрет добавлять ноды напрямую на сцену

	virtual Rc<Queue> makeQueue(Queue::Builder &&);

	virtual void updateContentNode(SceneContent *);

#if SP_REF_DEBUG
	virtual bool isRetainTrackerEnabled() const override {
		return true;
	}
#endif

	Director *_director = nullptr;
	SceneContent *_content = nullptr;

	Rc<Queue> _queue;

	core::FrameConstraints _constraints;
};

}

#endif /* XENOLITH_SCENE_NODES_XLSCENE_H_ */
