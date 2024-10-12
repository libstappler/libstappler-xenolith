// Наносит объекты на SDF-карту, записывает только значения SDF

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include "XL2dGlslVertexData.h"
#include "XL2dGlslShadowData.h"

#define PSEUDOSDF_MODE_SOLID 0
#define PSEUDOSDF_MODE_SDF 1
#define PSEUDOSDF_MODE_BACKREAD 2

layout (constant_id = 0) const int PSEUDOSDF_MODE = PSEUDOSDF_MODE_SOLID;

layout (set = 0, binding = 0) readonly buffer Vertices {
	Vertex vertices[];
} vertexBuffer[2];

layout (push_constant) uniform pcb {
	float pseudoSdfInset;
	float pseudoSdfOffset;
	float pseudoSdfMax;
} pushConstants;

layout (set = 0, binding = 0) readonly buffer TransformObjects {
	TransformData objects[];
} transformObjectBuffer[2];

layout (set = 0, binding = 1) uniform ShadowDataBuffer {
	ShadowData shadowData;
};

layout (location = 0) out vec4 fragColor;

void main() {
	const Vertex vertex = vertexBuffer[0].vertices[gl_VertexIndex];
	const uint transformIdx = vertex.material >> 16;
	const TransformData transform = transformObjectBuffer[1].objects[transformIdx];

	const vec4 pos = vertex.pos;
	gl_Position = transform.transform * pos * transform.mask + transform.offset;
	gl_Position.z = transform.shadow.x / shadowData.maxValue;

	const float pseudoSdfInset = vertex.tex.x;
	const float pseudoSdfOffset = vertex.tex.y;

	if (PSEUDOSDF_MODE != PSEUDOSDF_MODE_SOLID) {
		const float sdfValue = (1.0 - vertex.color.a);
		const float pseudoSdfScale = (pseudoSdfInset + pseudoSdfOffset) / shadowData.density;
		const float height = shadowData.maxValue - transform.shadow.x;

		float preudoSdfValue = sdfValue * pseudoSdfScale;
		if (preudoSdfValue > 0.0) {
			preudoSdfValue = preudoSdfValue;
			//preudoSdfValue = sqrt(preudoSdfValue * preudoSdfValue + height * height);
		} else {
			preudoSdfValue = preudoSdfValue;
			//preudoSdfValue = preudoSdfValue + height;
		}

		fragColor = vec4(preudoSdfValue, vertex.color.g, vertex.color.b, transform.shadow.x);
	} else {
		fragColor = vec4(pushConstants.pseudoSdfMax, 0.0, 0.0, 1.0);
	}
}
