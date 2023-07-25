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

#ifndef XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLDATA_H_
#define XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLDATA_H_

#include "SPGlsl.h"

#ifndef SP_GLSL
namespace stappler::glsl {
#endif

struct Vertex {
	vec4 pos;
	vec4 color;
	vec2 tex;
	uint material;
	uint object;
};

struct MaterialData {
	uint samplerImageIdx;
	uint setIdx;
	uint atlasIdx;
	uint flags;
};

struct TransformData {
	mat4 transform;
	vec4 mask;
	vec4 offset;
	vec4 shadow;
	vec4 padding;

#ifndef SP_GLSL
	TransformData() :
	transform(mat4::IDENTITY),
	mask(vec4(1.0f, 1.0f, 0.0f, 0.0f)),
	offset(vec4(0.0f, 0.0f, 0.0f, 1.0f)),
	shadow(vec4::ZERO)
	{ }

	TransformData(const mat4 &m) :
	transform(m),
	mask(vec4(1.0f, 1.0f, 0.0f, 0.0f)),
	offset(vec4(0.0f, 0.0f, 0.0f, 1.0f)),
	shadow(vec4::ZERO)
	{ }
#endif
};

struct DataAtlasIndex {
	uint key;
	uint value;
};

struct DataAtlasValue {
	vec2 pos;
	vec2 tex;
};

#ifndef SP_GLSL
}
#endif

#endif /* XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLDATA_H_ */
