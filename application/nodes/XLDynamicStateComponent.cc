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

#include "XLDynamicStateComponent.h"
#include "XLNode.h"
#include "XLFrameContext.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

bool DynamicStateComponent::init() { return init(DynamicStateApplyMode::DoNotApply); }

bool DynamicStateComponent::init(DynamicStateApplyMode value) {
	_applyMode = DynamicStateApplyMode::DoNotApply;
	_componentFlags = ComponentFlags::HandleOwnerEvents | ComponentFlags::HandleSceneEvents;

	setStateApplyMode(value);
	return Component::init();
}

void DynamicStateComponent::handleVisitBegin(FrameInfo &frameInfo) { _isStateValuesActual = false; }

void DynamicStateComponent::handleVisitNodesBelow(FrameInfo &frameInfo, SpanView<Rc<Node>> nodes,
		NodeFlags flags) {
	if (!nodes.empty() && hasFlag(_applyMode, DynamicStateApplyMode::ApplyForNodesBelow)) {
		pushState(frameInfo);
	}
}

void DynamicStateComponent::handleVisitSelf(FrameInfo &frameInfo, Node *, NodeFlags flags) {
	if (hasFlag(_applyMode, DynamicStateApplyMode::ApplyForSelf)) {
		pushState(frameInfo);
	} else {
		popState(frameInfo);
	}
}

void DynamicStateComponent::handleVisitNodesAbove(FrameInfo &frameInfo, SpanView<Rc<Node>> nodes,
		NodeFlags flags) {
	if (!nodes.empty() && hasFlag(_applyMode, DynamicStateApplyMode::ApplyForNodesAbove)) {
		pushState(frameInfo);
	} else {
		popState(frameInfo);
	}
}

void DynamicStateComponent::handleVisitEnd(FrameInfo &frameInfo) {
	popState(frameInfo);
	_isStateValuesActual = false;
}

void DynamicStateComponent::setStateApplyMode(DynamicStateApplyMode value) {
	if (value != _applyMode) {
		_applyMode = value;
		if (_applyMode != DynamicStateApplyMode::DoNotApply) {
			setComponentFlags(ComponentFlags::HandleVisitSelf | ComponentFlags::HandleVisitControl
					| ComponentFlags::HandleOwnerEvents | ComponentFlags::HandleSceneEvents);
		} else {
			setComponentFlags(
					ComponentFlags::HandleOwnerEvents | ComponentFlags::HandleSceneEvents);
		}
	}
}

void DynamicStateComponent::setIgnoreParentState(bool val) { _ignoreParentState = val; }

void DynamicStateComponent::enableScissor(Padding outline) {
	_scissorEnabled = true;
	_scissorOutline = outline;
}

void DynamicStateComponent::disableScissor() { _scissorEnabled = false; }

DrawStateValues DynamicStateComponent::updateDynamicState(const DrawStateValues &values) const {
	auto getViewRect = [&, this] {
		auto contentSize = _owner->getContentSize();
		Vec2 bottomLeft =
				_owner->convertToWorldSpace(Vec2(-_scissorOutline.left, -_scissorOutline.bottom));
		Vec2 topRight = _owner->convertToWorldSpace(Vec2(contentSize.width + _scissorOutline.right,
				contentSize.height + _scissorOutline.top));

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
			uint32_t(roundf(topRight.x - bottomLeft.x)),
			uint32_t(roundf(topRight.y - bottomLeft.y))};
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

void DynamicStateComponent::pushState(FrameInfo &info) {
	if (_isStateActive) {
		return;
	}

	auto &ctx = info.contextStack.back();
	auto prevStateId = ctx->getCurrentState();

	if (!_isStateValuesActual) {
		_currentStateId = rebuildState(*ctx);
	}

	if (_currentStateId == prevStateId) {
		// no need to enable anything
		_isStateActive = true;
		return;
	}

	ctx->stateStack.emplace_back(_currentStateId, static_cast<FrameStateOwnerInterface *>(this));
	_isStateActive = true;
	_isStatePushed = true;
}

void DynamicStateComponent::popState(FrameInfo &info) {
	if (!_isStateActive) {
		return;
	}

	if (!_isStatePushed) {
		_isStateActive = false;
		return;
	}

	auto &ctx = info.contextStack.back();

	if (ctx->stateStack.back().second == this) {
		ctx->stateStack.pop_back();
	} else {
		Vector<Pair<StateId, FrameStateOwnerInterface *>> owners;
		while (!ctx->stateStack.empty() && ctx->stateStack.back().second != this) {
			owners.emplace_back(ctx->stateStack.back());
			ctx->stateStack.pop_back();
		}

		if (ctx->stateStack.back().second == this) {
			ctx->stateStack.pop_back();
		}

		std::reverse(owners.begin(), owners.end());

		for (auto &it : owners) {
			if (it.second) {
				ctx->stateStack.emplace_back(it.second->rebuildState(*ctx), it.second);
			} else {
				ctx->stateStack.emplace_back(it);
			}
		}
	}
	_isStatePushed = false;
	_isStateActive = false;
}

StateId DynamicStateComponent::rebuildState(FrameContextHandle &ctx) {
	auto prevStateId = ctx.getCurrentState();
	auto currentState = ctx.getState(prevStateId);

	_stateValues = updateDynamicState(currentState ? *currentState : DrawStateValues());
	_isStateValuesActual = true;

	if (_stateValues.enabled == core::DynamicState::None) {
		_currentStateId = maxOf<StateId>();
	} else {
		_currentStateId = ctx.addState(_stateValues);
	}
	return _currentStateId;
}

} // namespace stappler::xenolith
