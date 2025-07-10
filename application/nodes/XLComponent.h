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

#ifndef XENOLITH_APPLICATION_NODES_XLCOMPONENT_H_
#define XENOLITH_APPLICATION_NODES_XLCOMPONENT_H_

#include "XLNodeInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

struct FrameInfo;
class Node;
class Scene;

enum class ComponentFlags {
	None,
	HandleOwnerEvents = 1 << 0, // Added/Removed
	HandleSceneEvents = 1 << 1, // Enter/Exit
	HandleNodeEvents = 1 << 2, // ContentSize/Transform/Reorder
	HandleVisitSelf = 1 << 3, // VisitSelf
	HandleVisitControl = 1 << 4, // VisitBegin/VisitNodesBelow/VisitNodesAbove/VisitEnd

	Default = HandleOwnerEvents | HandleSceneEvents | HandleNodeEvents | HandleVisitSelf
};

SP_DEFINE_ENUM_AS_MASK(ComponentFlags)

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

	virtual void handleVisitBegin(FrameInfo &);
	virtual void handleVisitNodesBelow(FrameInfo &, SpanView<Rc<Node>>, NodeFlags flags);
	virtual void handleVisitSelf(FrameInfo &, Node *, NodeFlags flags);
	virtual void handleVisitNodesAbove(FrameInfo &, SpanView<Rc<Node>>, NodeFlags flags);
	virtual void handleVisitEnd(FrameInfo &);

	virtual void update(const UpdateTime &time);

	virtual void handleContentSizeDirty();
	virtual void handleTransformDirty(const Mat4 &);
	virtual void handleReorderChildDirty();

	virtual bool isRunning() const;

	virtual bool isEnabled() const;
	virtual void setEnabled(bool b);

	virtual void setComponentFlags(ComponentFlags);
	virtual ComponentFlags getComponentFlags() const { return _componentFlags; }

	bool isScheduled() const;
	void scheduleUpdate();
	void unscheduleUpdate();

	Node *getOwner() const { return _owner; }

	void setFrameTag(uint64_t);
	uint64_t getFrameTag() const { return _frameTag; }

protected:
	Node *_owner = nullptr;
	bool _enabled = true;
	bool _running = false;
	bool _scheduled = false;
	uint64_t _frameTag = InvalidTag;
	ComponentFlags _componentFlags = ComponentFlags::Default;
};

class SP_PUBLIC CallbackComponent : public Component {
public:
	virtual ~CallbackComponent() = default;

	CallbackComponent();

	virtual void handleAdded(Node *owner) override;
	virtual void handleRemoved() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	virtual void handleVisitBegin(FrameInfo &) override;
	virtual void handleVisitNodesBelow(FrameInfo &, SpanView<Rc<Node>>, NodeFlags flags) override;
	virtual void handleVisitSelf(FrameInfo &, Node *, NodeFlags flags) override;
	virtual void handleVisitNodesAbove(FrameInfo &, SpanView<Rc<Node>>, NodeFlags flags) override;
	virtual void handleVisitEnd(FrameInfo &) override;

	virtual void update(const UpdateTime &time) override;

	virtual void handleContentSizeDirty() override;
	virtual void handleTransformDirty(const Mat4 &) override;
	virtual void handleReorderChildDirty() override;

	virtual void setUserdata(Rc<Ref> &&d) { _userdata = move(d); }
	virtual Ref *getUserdata() const { return _userdata; }

	virtual void setAddedCallback(Function<void(CallbackComponent *, Node *)> &&);
	virtual auto getAddedCallback() -> const Function<void(CallbackComponent *, Node *)> & {
		return _handleAdded;
	}

	virtual void setRemovedCallback(Function<void(CallbackComponent *, Node *)> &&);
	virtual auto getRemovedCallback() -> const Function<void(CallbackComponent *, Node *)> & {
		return _handleRemoved;
	}

	virtual void setEnterCallback(Function<void(CallbackComponent *, Scene *)> &&);
	virtual auto getEnterCallback() -> const Function<void(CallbackComponent *, Scene *)> & {
		return _handleEnter;
	}

	virtual void setExitCallback(Function<void(CallbackComponent *)> &&);
	virtual auto getExitCallback() -> const Function<void(CallbackComponent *)> & {
		return _handleExit;
	}

	virtual void setVisitBeginCallback(Function<void(CallbackComponent *, FrameInfo &)> &&);
	virtual auto getVisitBeginCallback()
			-> const Function<void(CallbackComponent *, FrameInfo &)> & {
		return _handleVisitBegin;
	}

	virtual void setVisitNodesBelowCallback(
			Function<void(CallbackComponent *, FrameInfo &, SpanView<Rc<Node>>, NodeFlags)> &&);
	virtual auto getVisitNodesBelowCallback() -> const
			Function<void(CallbackComponent *, FrameInfo &, SpanView<Rc<Node>>, NodeFlags)> & {
		return _handleVisitNodesBelow;
	}

	virtual void setVisitSelfCallback(
			Function<void(CallbackComponent *, FrameInfo &, Node *, NodeFlags)> &&);
	virtual auto getVisitSelfCallback()
			-> const Function<void(CallbackComponent *, FrameInfo &, Node *, NodeFlags)> & {
		return _handleVisitSelf;
	}

	virtual void setVisitNodesAboveCallback(
			Function<void(CallbackComponent *, FrameInfo &, SpanView<Rc<Node>>, NodeFlags)> &&);
	virtual auto getVisitNodesAboveCallback() -> const
			Function<void(CallbackComponent *, FrameInfo &, SpanView<Rc<Node>>, NodeFlags)> & {
		return _handleVisitNodesAbove;
	}

	virtual void setVisitEndCallback(Function<void(CallbackComponent *, FrameInfo &)> &&);
	virtual auto getVisitEndCallback() -> const Function<void(CallbackComponent *, FrameInfo &)> & {
		return _handleVisitEnd;
	}

	virtual void setUpdateCallback(Function<void(CallbackComponent *, const UpdateTime &)> &&);
	virtual auto getUpdateCallback()
			-> const Function<void(CallbackComponent *, const UpdateTime &)> & {
		return _handleUpdate;
	}

	virtual void setContentSizeDirtyCallback(Function<void(CallbackComponent *)> &&);
	virtual auto getContentSizeDirtyCallback() -> const Function<void(CallbackComponent *)> & {
		return _handleContentSizeDirty;
	}

	virtual void setTransformDirtyCallback(Function<void(CallbackComponent *, const Mat4 &)> &&);
	virtual auto getTransformDirtyCallback()
			-> const Function<void(CallbackComponent *, const Mat4 &)> & {
		return _handleTransformDirty;
	}

	virtual void setReorderChildDirtyCallback(Function<void(CallbackComponent *)> &&);
	virtual auto getReorderChildDirtyCallback() -> const Function<void(CallbackComponent *)> & {
		return _handleReorderChildDirty;
	}

protected:
	virtual void updateFlags();

	Rc<Ref> _userdata;

	Function<void(CallbackComponent *, Node *)> _handleAdded;
	Function<void(CallbackComponent *, Node *)> _handleRemoved;
	Function<void(CallbackComponent *, Scene *)> _handleEnter;
	Function<void(CallbackComponent *)> _handleExit;
	Function<void(CallbackComponent *, FrameInfo &)> _handleVisitBegin;
	Function<void(CallbackComponent *, FrameInfo &, SpanView<Rc<Node>>, NodeFlags flags)>
			_handleVisitNodesBelow;
	Function<void(CallbackComponent *, FrameInfo &, Node *, NodeFlags flags)> _handleVisitSelf;
	Function<void(CallbackComponent *, FrameInfo &, SpanView<Rc<Node>>, NodeFlags flags)>
			_handleVisitNodesAbove;
	Function<void(CallbackComponent *, FrameInfo &)> _handleVisitEnd;
	Function<void(CallbackComponent *, const UpdateTime &time)> _handleUpdate;
	Function<void(CallbackComponent *)> _handleContentSizeDirty;
	Function<void(CallbackComponent *, const Mat4 &)> _handleTransformDirty;
	Function<void(CallbackComponent *)> _handleReorderChildDirty;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_NODES_XLCOMPONENT_H_ */
