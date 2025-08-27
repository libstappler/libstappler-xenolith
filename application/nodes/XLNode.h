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

#ifndef XENOLITH_APPLICATION_NODES_XLNODE_H_
#define XENOLITH_APPLICATION_NODES_XLNODE_H_

#include "XLNodeInfo.h"
#include "XLComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Component;
class DynamicStateComponent;
class Scene;
class Scheduler;
class InputListener;
class Action;
class ActionManager;
class Director;
class FrameContext;

struct SP_PUBLIC ActionStorage : public Ref {
	Vector<Rc<Action>> actionToStart;

	void addAction(Rc<Action> &&a);
	void removeAction(Action *a);
	void removeAllActions();
	void removeActionByTag(uint32_t);
	void removeAllActionsByTag(uint32_t);
	Action *getActionByTag(uint32_t);
};

class SP_PUBLIC Node : public Ref {
public:
	/* Nodes with transparent zOrder will not be added into zPath */
	static constexpr ZOrder ZOrderTransparent = ZOrder::min();
	static constexpr ZOrder ZOrderMax = ZOrder::max();
	static constexpr ZOrder ZOrderMin = ZOrder::min() + ZOrder(1);

	static bool isParent(Node *parent, Node *node);
	static Mat4 getChainNodeToParentTransform(Node *parent, Node *node, bool withParent);
	static Mat4 getChainParentToNodeTransform(Node *parent, Node *node, bool withParent);

	Node();
	virtual ~Node();

	virtual bool init();

	virtual void setLocalZOrder(ZOrder localZOrder);
	virtual ZOrder getLocalZOrder() const { return _zOrder; }

	virtual void setScale(float scale);
	virtual void setScale(const Vec2 &);
	virtual void setScale(const Vec3 &);
	virtual void setScaleX(float scaleX);
	virtual void setScaleY(float scaleY);
	virtual void setScaleZ(float scaleZ);

	virtual Vec3 getScale() const { return _scale; }

	virtual void setPosition(const Vec2 &position);
	virtual void setPosition(const Vec3 &position);
	virtual void setPositionX(float);
	virtual void setPositionY(float);
	virtual void setPositionZ(float);

	virtual Vec3 getPosition() const { return _position; }

	virtual void setSkewX(float skewX);
	virtual void setSkewY(float skewY);

	virtual Vec2 getSkew() const { return _skew; }

	/**
	 * Sets the anchor point in percent.
	 *
	 * anchorPoint is the point around which all transformations and positioning manipulations take place.
	 * It's like a pin in the node where it is "attached" to its parent.
	 * The anchorPoint is normalized, like a percentage. (0,0) means the bottom-left corner and (1,1) means the top-right corner.
	 * But you can use values higher than (1,1) and lower than (0,0) too.
	 * The default anchorPoint is (0.5,0.5), so it starts in the center of the node.
	 * @note If node has a physics body, the anchor must be in the middle, you cann't change this to other value.
	 *
	 * @param anchorPoint   The anchor point of node.
	 */
	virtual void setAnchorPoint(const Vec2 &anchorPoint);
	virtual Vec2 getAnchorPoint() const { return _anchorPoint; }

	/**
	 * Sets the untransformed size of the node.
	 *
	 * The contentSize remains the same no matter the node is scaled or rotated.
	 * All nodes has a size. Layer and Scene has the same size of the screen.
	 *
	 * @param contentSize   The untransformed size of the node.
	 */
	virtual void setContentSize(const Size2 &contentSize);
	virtual Size2 getContentSize() const { return _contentSize; }

	virtual void setVisible(bool visible);
	virtual bool isVisible() const { return _visible; }

	virtual void setRotation(float rotationInRadians);
	virtual void setRotation(const Vec3 &rotationInRadians);
	virtual void setRotation(const Quaternion &quat);

	virtual float getRotation() const { return _rotation.z; }
	virtual Vec3 getRotation3D() const { return _rotation; }
	virtual Quaternion getRotationQuat() const { return _rotationQuat; }

	template <typename N, typename... Args>
	auto addChild(N *child, Args &&...args) -> N * {
		addChildNode(child, std::forward<Args>(args)...);
		return child;
	}

	template <typename N, typename... Args>
	auto addChild(const Rc<N> &child, Args &&...args) -> N * {
		addChildNode(child.get(), std::forward<Args>(args)...);
		return child.get();
	}

	virtual void addChildNode(Node *child);
	virtual void addChildNode(Node *child, ZOrder localZOrder);
	virtual void addChildNode(Node *child, ZOrder localZOrder, uint64_t tag);

	virtual Node *getChildByTag(uint64_t tag) const;

	virtual SpanView<Rc<Node>> getChildren() const { return _children; }
	virtual size_t getChildrenCount() const { return _children.size(); }

	virtual void setParent(Node *parent);
	virtual Node *getParent() const { return _parent; }

	virtual void removeFromParent(bool cleanup = true);
	virtual void removeChild(Node *child, bool cleanup = true);
	virtual void removeChildByTag(uint64_t tag, bool cleanup = true);
	virtual void removeAllChildren(bool cleanup = true);

	virtual void reorderChild(Node *child, ZOrder localZOrder);

	/**
	 * Sorts the children array once before drawing, instead of every time when a child is added or reordered.
	 * This appraoch can improves the performance massively.
	 * @note Don't call this manually unless a child added needs to be removed in the same frame.
	 */
	virtual void sortAllChildren();

	template <typename A>
	auto runAction(A *action) -> A * {
		runActionObject(action);
		return action;
	}

	template <typename A>
	auto runAction(A *action, uint32_t tag) -> A * {
		runActionObject(action, tag);
		return action;
	}

	template <typename A>
	auto runAction(const Rc<A> &action) -> A * {
		runActionObject(action.get());
		return action.get();
	}

	template <typename A>
	auto runAction(const Rc<A> &action, uint32_t tag) -> A * {
		runActionObject(action.get(), tag);
		return action.get();
	}

	void runActionObject(Action *);
	void runActionObject(Action *, uint32_t tag);

	void stopAllActions();

	void stopAction(Action *action);
	void stopActionByTag(uint32_t tag);
	void stopAllActionsByTag(uint32_t tag);

	Action *getActionByTag(uint32_t tag);
	size_t getNumberOfRunningActions() const;


	template <typename C>
	auto addComponent(C *component) -> C * {
		if (addComponentItem(component)) {
			return component;
		}
		return nullptr;
	}

	template <typename C>
	auto addComponent(const Rc<C> &component) -> C * {
		if (addComponentItem(component.get())) {
			return component.get();
		}
		return nullptr;
	}

	virtual bool addComponentItem(Component *);
	virtual bool removeComponent(Component *);
	virtual bool removeComponentByTag(uint64_t);
	virtual bool removeAllComponentByTag(uint64_t);
	virtual void removeAllComponents();

	template <typename T>
	T *getComponentByType() const;

	template <typename T>
	T *getComponentByType(uint32_t tag) const;

	virtual StringView getName() const { return _name; }
	virtual void setName(StringView str) { _name = str.str<Interface>(); }

	virtual const Value &getDataValue() const { return _dataValue; }
	virtual void setDataValue(Value &&val) { _dataValue = move(val); }

	virtual uint64_t getTag() const { return _tag; }
	virtual void setTag(uint64_t tag);

	virtual bool isRunning() const { return _running; }

	// Node was added to scene
	virtual void handleEnter(Scene *);

	// Node was removed from scene
	virtual void handleExit();

	// New ContentSize applied for the node
	// There you can setup Node's appearance and layout it's subnodes
	virtual void handleContentSizeDirty();

	// New Transform applied for the node
	// Node was repositioned or scaled within it's parent
	virtual void handleTransformDirty(const Mat4 &);

	// Node was repositioned or scaled within scene
	// There global parameters (like pixel density) can be recalculated
	virtual void handleGlobalTransformDirty(const Mat4 &);

	// Children array was updated somehow
	virtual void handleReorderChildDirty();

	// Node should be positioned within parent
	virtual void handleLayout(Node *);

	virtual void cleanup();

	virtual Rect getBoundingBox() const;

	virtual void resume();
	virtual void pause();

	virtual void update(const UpdateTime &time);

	virtual const Mat4 &getNodeToParentTransform() const;

	virtual void setNodeToParentTransform(const Mat4 &transform);
	virtual const Mat4 &getParentToNodeTransform() const;

	virtual Mat4 getNodeToWorldTransform() const;
	virtual Mat4 getWorldToNodeTransform() const;

	Vec2 convertToNodeSpace(const Vec2 &worldPoint) const;
	Vec2 convertToWorldSpace(const Vec2 &nodePoint) const;
	Vec2 convertToNodeSpaceAR(const Vec2 &worldPoint) const;
	Vec2 convertToWorldSpaceAR(const Vec2 &nodePoint) const;

	virtual bool isCascadeOpacityEnabled() const { return _cascadeOpacityEnabled; }
	virtual bool isCascadeColorEnabled() const { return _cascadeColorEnabled; }

	virtual void setCascadeOpacityEnabled(bool cascadeOpacityEnabled);
	virtual void setCascadeColorEnabled(bool cascadeColorEnabled);

	virtual float getOpacity() const { return _realColor.a; }
	virtual float getDisplayedOpacity() const { return _displayedColor.a; }
	virtual void setOpacity(float opacity);
	virtual void setOpacity(OpacityValue);
	virtual void updateDisplayedOpacity(float parentOpacity);

	virtual Color4F getColor() const { return _realColor; }
	virtual Color4F getDisplayedColor() const { return _displayedColor; }
	virtual void setColor(const Color4F &color, bool withOpacity = false);
	virtual void updateDisplayedColor(const Color4F &parentColor);

	virtual void setDepthIndex(float value) { _depthIndex = value; }
	virtual float getDepthIndex() const { return _depthIndex; }

	virtual void draw(FrameInfo &, NodeFlags flags);

	// visit on unsorted nodes, commit most of geometry changes
	// on this step, we process child-to-parent changes (like nodes, based on label's size)
	virtual bool visitGeometry(FrameInfo &, NodeFlags parentFlags);

	// visit on sorted nodes, push draw commands
	// on this step, we also process parent-to-child geometry changes
	virtual bool visitDraw(FrameInfo &, NodeFlags parentFlags);

	virtual void visitSelf(FrameInfo &, NodeFlags flags, bool visibleByCamera);

	void scheduleUpdate();
	void unscheduleUpdate();

	virtual bool isTouched(const Vec2 &location, float padding = 0.0f);
	virtual bool isTouchedNodeSpace(const Vec2 &location, float padding = 0.0f);

	virtual void setEnterCallback(Function<void(Scene *)> &&);
	virtual void setExitCallback(Function<void()> &&);
	virtual void setContentSizeDirtyCallback(Function<void()> &&);
	virtual void setTransformDirtyCallback(Function<void(const Mat4 &)> &&);
	virtual void setReorderChildDirtyCallback(Function<void()> &&);
	virtual void setLayoutCallback(Function<void(Node *)> &&);

	float getInputDensity() const { return _inputDensity; }

	Scene *getScene() const { return _scene; }
	Director *getDirector() const { return _director; }
	Scheduler *getScheduler() const { return _scheduler; }
	ActionManager *getActionManager() const { return _actionManager; }
	FrameContext *getFrameContext() const { return _frameContext; }

	virtual float getMaxDepthIndex() const;

	virtual void retainFocus();
	virtual void releaseFocus();
	virtual void clearFocus();

	virtual uint32_t getFocus() const { return _focus; }

protected:
	struct VisitInfo {
		void (*visitBegin)(const VisitInfo &) = nullptr;
		void (*visitNodesBelow)(const VisitInfo &, SpanView<Rc<Node>>) = nullptr;
		void (*visitSelf)(const VisitInfo &, Node *) = nullptr;
		void (*visitNodesAbove)(const VisitInfo &, SpanView<Rc<Node>>) = nullptr;
		void (*visitEnd)(const VisitInfo &) = nullptr;
		Node *node = nullptr;

		mutable NodeFlags flags = NodeFlags::None;
		mutable FrameInfo *frameInfo = nullptr;
		mutable bool visibleByCamera = true;
		mutable Vector<Rc<Component>> visitableComponents;
	};

	virtual void updateCascadeOpacity();
	virtual void disableCascadeOpacity();
	virtual void updateCascadeColor();
	virtual void disableCascadeColor();
	virtual void updateColor() { }

	Mat4 transform(const Mat4 &parentTransform);
	virtual NodeFlags processParentFlags(FrameInfo &info, NodeFlags parentFlags);

	virtual bool wrapVisit(FrameInfo &, NodeFlags flags, const VisitInfo &, bool useContext);

	bool _is3d = false;
	bool _running = false;
	bool _visible = true;
	bool _scheduled = false;
	bool _paused = false;

	bool _cascadeColorEnabled = false;
	bool _cascadeOpacityEnabled = true;

	bool _contentSizeDirty = true;
	bool _reorderChildDirty = true;
	mutable bool _transformCacheDirty = true; // dynamic value
	mutable bool _transformInverseDirty = true; // dynamic value
	bool _transformDirty = true;

	String _name;
	Value _dataValue;

	uint64_t _tag = InvalidTag;
	ZOrder _zOrder = ZOrder(0);
	uint32_t _focus = 0;

	Vec2 _skew;
	Vec2 _anchorPoint;
	Size2 _contentSize;

	Vec3 _position;
	Vec3 _scale = Vec3(1.0f, 1.0f, 1.0f);
	Vec3 _rotation;
	float _inputDensity = 1.0f;
	float _depthIndex = 0.0f;

	// to support HDR, we use float colors;
	Color4F _displayedColor = Color4F::WHITE;
	Color4F _realColor = Color4F::WHITE;

	Quaternion _rotationQuat;

	mutable Mat4 _transform = Mat4::IDENTITY;
	mutable Mat4 _inverse = Mat4::IDENTITY;
	Mat4 _modelViewTransform = Mat4::IDENTITY;

	Vector<Rc<Node>> _children;
	Node *_parent = nullptr;

	Function<void(Scene *)> _enterCallback;
	Function<void()> _exitCallback;
	Function<void()> _contentSizeDirtyCallback;
	Function<void(const Mat4 &)> _transformDirtyCallback;
	Function<void()> _reorderChildDirtyCallback;
	Function<void(Node *)> _layoutCallback;

	Vector<Rc<Component>> _components;

	Scene *_scene = nullptr;
	Director *_director = nullptr;
	Scheduler *_scheduler = nullptr;
	ActionManager *_actionManager = nullptr;
	FrameContext *_frameContext = nullptr;

	Rc<ActionStorage> _actionStorage;
};


template <typename T>
auto Node::getComponentByType() const -> T * {
	for (auto &it : _components) {
		if (auto ret = dynamic_cast<T *>(it.get())) {
			return ret;
		}
	}
	return nullptr;
}

template <typename T>
auto Node::getComponentByType(uint32_t tag) const -> T * {
	for (auto &it : _components) {
		if (it->getFrameTag() == tag) {
			if (auto ret = dynamic_cast<T *>(it.get())) {
				return ret;
			}
		}
	}
	return nullptr;
}


} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_NODES_XLNODE_H_ */
