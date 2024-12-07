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

#ifndef XENOLITH_SCENE_NODES_XLCOMPONENT_H_
#define XENOLITH_SCENE_NODES_XLCOMPONENT_H_

#include "XLNodeInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

struct FrameInfo;
class Node;
class Scene;

class SP_PUBLIC Component : public Ref {
public:
	static uint64_t GetNextComponentId();

	Component();
	virtual ~Component();
	virtual bool init();

	virtual void handleAdded(Node *owner);
	virtual void handleRemoved();

	virtual void handleEnter(Scene *);
	virtual void handleExit();

	virtual void visitSelf(FrameInfo &, NodeFlags parentFlags);

	virtual void update(const UpdateTime &time);

	virtual void handleContentSizeDirty();
	virtual void handleTransformDirty(const Mat4 &);
	virtual void handleReorderChildDirty();

	virtual bool isRunning() const;

	virtual bool isEnabled() const;
	virtual void setEnabled(bool b);

	bool isScheduled() const;
	void scheduleUpdate();
	void unscheduleUpdate();

	Node* getOwner() const { return _owner; }

	void setFrameTag(uint64_t);
	uint64_t getFrameTag() const { return _frameTag; }

protected:
	Node *_owner = nullptr;
	bool _enabled = true;
	bool _running = false;
	bool _scheduled = false;
	uint64_t _frameTag = InvalidTag;
};

}

#endif /* XENOLITH_SCENE_NODES_XLCOMPONENT_H_ */
