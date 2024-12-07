/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLDynamicStateNode.h"
#include "XLFrameInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

bool DynamicStateNode::init() {
	return Node::init();
}

void DynamicStateNode::setStateApplyMode(StateApplyMode value) {
	_applyMode = value;
}

void DynamicStateNode::setIgnoreParentState(bool val) {
	_ignoreParentState = val;
}

bool DynamicStateNode::visitDraw(FrameInfo &info, NodeFlags parentFlags) {
	if (_applyMode == DoNotApply || info.contextStack.empty()) {
		return Node::visitDraw(info, parentFlags);
	}

	if (!_visible) {
		return false;
	}

	auto &ctx = info.contextStack.back();
	auto prevStateId = ctx->getCurrentState();
	auto currentState = ctx->getState(prevStateId);
	/*if (!currentState) {
		// invalid state
		return Node::visitDraw(info, parentFlags);
	}*/

	auto newState = updateDynamicState(currentState ? *currentState : DrawStateValues());

	_currentStateId = maxOf<StateId>();

	if (newState.enabled == core::DynamicState::None) {
		// no need to enable anything, drop to 0
		if (prevStateId == maxOf<StateId>()) {
			return Node::visitDraw(info, parentFlags);
		}
	} else {
		_currentStateId = ctx->addState(newState);
	}

	VisitInfo visitInfo;
	visitInfo.visitNodesBelow = [] (const VisitInfo &visitInfo, SpanView<Rc<Node>> nodes) {
		auto applyMode = static_cast<DynamicStateNode *>(visitInfo.node)->getStateApplyMode();
		auto stateId = static_cast<DynamicStateNode *>(visitInfo.node)->getCurrentStateId();

		auto &ctx = visitInfo.frameInfo->contextStack.back();

		switch (applyMode) {
		case ApplyForAll:
		case ApplyForNodesBelow:
			ctx->stateStack.push_back(stateId);
			break;
		default: break;
		}

		for (auto &it : nodes) {
			it->visitDraw(*visitInfo.frameInfo, visitInfo.flags);
		}

		switch (applyMode) {
		case ApplyForNodesAbove:
			ctx->stateStack.push_back(stateId);
			break;
		default: break;
		}
	};

	visitInfo.visitSelf = [] (const VisitInfo &visitInfo, Node *node) {
		node->visitSelf(*visitInfo.frameInfo, visitInfo.flags, visitInfo.visibleByCamera);
	};

	visitInfo.visitNodesAbove = [] (const VisitInfo &visitInfo, SpanView<Rc<Node>> nodes) {
		auto applyMode = static_cast<DynamicStateNode *>(visitInfo.node)->getStateApplyMode();

		auto &ctx = visitInfo.frameInfo->contextStack.back();

		switch (applyMode) {
		case ApplyForNodesBelow:
			ctx->stateStack.pop_back();
			break;
		default: break;
		}

		for (auto &it : nodes) {
			it->visitDraw(*visitInfo.frameInfo, visitInfo.flags);
		}

		switch (applyMode) {
		case ApplyForAll:
		case ApplyForNodesAbove:
			ctx->stateStack.pop_back();
			break;
		default: break;
		}
	};

	visitInfo.node = this;

	auto ret = wrapVisit(info, parentFlags, visitInfo, true);

	_currentStateId = maxOf<StateId>();

	return ret;
}

void DynamicStateNode::enableScissor(Padding outline) {
	_scissorEnabled = true;
	_scissorOutline = outline;
}

void DynamicStateNode::disableScissor() {
	_scissorEnabled = false;
}

DrawStateValues DynamicStateNode::updateDynamicState(const DrawStateValues &values) const {
	auto getViewRect = [&, this] {
	    Vec2 bottomLeft = convertToWorldSpace(Vec2(-_scissorOutline.left, -_scissorOutline.bottom));
		Vec2 topRight = convertToWorldSpace(Vec2(_contentSize.width + _scissorOutline.right, _contentSize.height + _scissorOutline.top));

		if (bottomLeft.x > topRight.x) {
			float b = topRight.x;
			topRight.x = bottomLeft.x;
			bottomLeft.x = b;
		}

		if (bottomLeft.y > topRight.y) {
			float b = topRight.y;
			topRight.y = bottomLeft.y;
			bottomLeft.y = b;
		}

		return URect{uint32_t(roundf(bottomLeft.x)), uint32_t(roundf(bottomLeft.y)),
			uint32_t(roundf(topRight.x - bottomLeft.x)), uint32_t(roundf(topRight.y - bottomLeft.y))};
	};


	DrawStateValues ret(_ignoreParentState ? DrawStateValues() : values);
	if (_scissorEnabled) {
		auto viewRect = getViewRect();
		if ((ret.enabled & core::DynamicState::Scissor) == core::DynamicState::None) {
			ret.enabled |= core::DynamicState::Scissor;
			ret.scissor = viewRect;
		} else if (ret.scissor.intersectsRect(viewRect)) {
			ret.scissor = URect{
				std::max(ret.scissor.x, viewRect.x),
				std::max(ret.scissor.y, viewRect.y),
				std::min(ret.scissor.width, viewRect.width),
				std::min(ret.scissor.height, viewRect.height),
			};
		}
	}
	return ret;
}

}
