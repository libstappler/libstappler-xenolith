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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DVECTORRESULT_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DVECTORRESULT_H_

#include "XL2d.h"
#include "SPVectorImage.h"
#include <future>

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

using VectorImageData = vg::VectorImageData;
using VectorImage = vg::VectorImage;
using VectorPathRef = vg::VectorPathRef;
using PathWriter = vg::PathWriter;

struct VectorCanvasConfig {
	geom::Tesselator::RelocateRule relocateRule = geom::Tesselator::RelocateRule::Auto;
	float quality = 1.0f;
	float boundaryOffset = config::VGAntialiasFactor;
	float boundaryInset = config::VGAntialiasFactor;
	float sdfBoundaryOffset = config::VGPseudoSdfOffset;
	float sdfBoundaryInset = config::VGPseudoSdfInset;
	Color4F color = Color4F::WHITE;
	uint32_t fillMaterial = 0;
	uint32_t strokeMaterial = 1;
	uint32_t sdfMaterial = config::VGPseudoSdfMaterial;
	bool forcePseudoSdf = false;
	bool verbose = false;
};

struct VectorCanvasResult : public Ref {
	Vector<TransformVertexData> data;
	Vector<TransformVertexData> mut;
	VectorCanvasConfig config;
	Size2 targetSize;
	Mat4 targetTransform;

	void updateColor(const Color4F &);
};

class VectorCanvasDeferredResult : public DeferredVertexResult {
public:
	virtual ~VectorCanvasDeferredResult();

	bool init(std::future<Rc<VectorCanvasResult>> &&, bool waitOnReady);

	virtual SpanView<TransformVertexData> getData() override;

	virtual void handleReady(Rc<VectorCanvasResult> &&);
	virtual void handleReady() override { DeferredVertexResult::handleReady(); }

	void updateColor(const Color4F &);

	Rc<VectorCanvasResult> getResult() const;

protected:
	mutable Mutex _mutex;
	Rc<VectorCanvasResult> _result;
	std::future<Rc<VectorCanvasResult>> *_future = nullptr;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DVECTORRESULT_H_ */
