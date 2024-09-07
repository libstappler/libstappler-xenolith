/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2D_H_
#define XENOLITH_RENDERER_BASIC2D_XL2D_H_

#include "XLCommon.h"
#include "XLCoreMaterial.h"
#include "XLNodeInfo.h"
#include "XLSceneConfig.h"
#include "XL2dConfig.h"

#include "glsl/include/XL2dGlslVertexData.h"
#include "glsl/include/XL2dGlslShadowData.h"
#include "glsl/include/XL2dGlslSdfData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

using Vertex = glsl::Vertex;
using MaterialData = glsl::MaterialData;
using TransformData = glsl::TransformData;
using ShadowData = glsl::ShadowData;

using DataAtlasIndex = glsl::DataAtlasIndex;
using DataAtlasValue = glsl::DataAtlasValue;
using AmbientLightData = glsl::AmbientLightData;
using DirectLightData = glsl::DirectLightData;

using Sdf2DObjectData = glsl::Sdf2DObjectData;
using Circle2DIndex = glsl::Circle2DIndex;
using Triangle2DIndex = glsl::Triangle2DIndex;
using Rect2DIndex = glsl::Rect2DIndex;
using RoundedRect2DIndex = glsl::RoundedRect2DIndex;
using Polygon2DIndex = glsl::Polygon2DIndex;

struct Triangle {
	Vertex a;
	Vertex b;
	Vertex c;
};

struct Quad {
	Vertex tl;
	Vertex bl;
	Vertex tr;
	Vertex br;
};

struct VertexSpan {
	core::MaterialId material = 0;
	uint32_t indexCount = 0;
	uint32_t instanceCount = 0;
	uint32_t firstIndex = 0;
	uint32_t vertexOffset = 0;
	StateId state = 0;
	uint32_t gradientOffset = 0;
	uint32_t gradientCount = 0;
};

struct alignas(16) VertexData : public Ref {
	Vector<Vertex> data;
	Vector<uint32_t> indexes;
};

struct alignas(16) TransformVertexData {
	Mat4 transform;
	Rc<VertexData> data;
	uint32_t fillIndexes = 0;
	uint32_t strokeIndexes = 0;
	uint32_t sdfIndexes = 0;
};

enum class SdfShape {
	Circle2D,
	Rect2D,
	RoundedRect2D,
	Triangle2D,
	Polygon2D,
};

struct SdfPrimitive2D {
	Vec2 origin;
};

struct SdfCircle2D : SdfPrimitive2D {
	float radius;
};

struct SdfRect2D : SdfPrimitive2D {
	Size2 size;
};

struct SdfRoundedRect2D : SdfPrimitive2D {
	Size2 size;
	Vec4 radius;
};

struct SdfTriangle2D : SdfPrimitive2D {
	Vec2 a;
	Vec2 b;
	Vec2 c;
};

struct SdfPolygon2D {
	SpanView<Vec2> points;
};

struct SdfPrimitive2DHeader {
	SdfShape type;
	BytesView bytes;
};

class SP_PUBLIC DeferredVertexResult : public Ref {
public:
	virtual ~DeferredVertexResult() { }

	virtual SpanView<TransformVertexData> getData() = 0;

	bool isReady() const { return _isReady.load(); }
	bool isWaitOnReady() const { return _waitOnReady; }

	virtual void handleReady() { _isReady.store(true); }

protected:
	bool _waitOnReady = true;
	std::atomic<bool> _isReady;
};

struct SP_PUBLIC ShadowLightInput {
	Color4F globalColor = Color4F::BLACK;
	uint32_t ambientLightCount = 0;
	uint32_t directLightCount = 0;
	float sceneDensity = 1.0f;
	float shadowDensity = 1.0f;
	float luminosity = nan();
	float padding0 = 0.0f;
	AmbientLightData ambientLights[config::MaxAmbientLights];
	DirectLightData directLights[config::MaxDirectLights];

	bool addAmbientLight(const Vec4 &, const Color4F &, bool softShadow);
	bool addDirectLight(const Vec4 &, const Color4F &, const Vec4 &);

	Extent2 getShadowExtent(Size2 frameSize) const;
	Size2 getShadowSize(Size2 frameSize) const;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_XL2D_H_ */
