/**
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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DFRAMECONTEXT_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DFRAMECONTEXT_H_

#include "XLFrameContext.h"
#include "XL2dCommandList.h"
#include "XL2dLinearGradient.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class FrameContext2d : public FrameContext {
public:
	static constexpr StringView MaterialAttachmentName = "MaterialInput2d";
	static constexpr StringView VertexAttachmentName = "VertexInput2d";
	static constexpr StringView LightDataAttachmentName = "ShadowLightDataAttachment";
	static constexpr StringView ShadowVertexAttachmentName = "ShadowVertexAttachment";
	static constexpr StringView SdfImageAttachmentName = "SdfImageAttachment";

	virtual ~FrameContext2d() { }

	virtual void onEnter(xenolith::Scene *) override;
	virtual void onExit() override;

	virtual Rc<FrameContextHandle> makeHandle(FrameInfo &) override;
	virtual void submitHandle(FrameInfo &, FrameContextHandle *) override;

protected:
	bool initWithQueue(core::Queue *);

	const core::AttachmentData *_materialAttachmentData = nullptr;
	const core::AttachmentData *_vertexAttachmentData = nullptr;
	const core::AttachmentData *_lightAttachmentData = nullptr;
	const core::AttachmentData *_shadowVertexAttachmentData = nullptr;
	const core::AttachmentData *_sdfImageAttachmentData = nullptr;

	bool _init = false;
};

struct StateData : public Ref {
	virtual ~StateData() = default;

	virtual bool init();
	virtual bool init(StateData *);

	Mat4 transform;
	Rc<LinearGradientData> gradient;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DFRAMECONTEXT_H_ */
