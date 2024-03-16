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

#ifndef XENOLITH_SCENE_NODES_XLDYNAMICSTATENODE_H_
#define XENOLITH_SCENE_NODES_XLDYNAMICSTATENODE_H_

#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class DynamicStateNode : public Node {
public:
	enum StateApplyMode {
		DoNotApply,
		ApplyForAll,
		ApplyForNodesBelow,
		ApplyForNodesAbove
	};

	virtual bool init() override;

	virtual StateApplyMode getStateApplyMode() const { return _applyMode; }
	virtual void setStateApplyMode(StateApplyMode value);

	virtual bool isIgnoreParentState() const { return _ignoreParentState; }
	virtual void setIgnoreParentState(bool);

	virtual bool visitDraw(FrameInfo &, NodeFlags parentFlags) override;

	virtual void enableScissor(Padding outline = Padding());
	virtual void disableScissor();
	virtual bool isScissorEnabled() const { return _scissorEnabled; }

	virtual void setScissorOutlone(Padding value) { _scissorOutline = value; }
	virtual Padding getScissorOutline() const { return _scissorOutline; }

protected:
	using Node::init;

	virtual DrawStateValues updateDynamicState(const DrawStateValues &) const;

	StateApplyMode _applyMode = ApplyForAll;

	bool _ignoreParentState = false;
	bool _scissorEnabled = false;
	Padding _scissorOutline;
};

}

#endif /* XENOLITH_SCENE_NODES_XLDYNAMICSTATENODE_H_ */
