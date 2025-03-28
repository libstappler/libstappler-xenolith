#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "SPGlslInit.h"
#include "XL2dGlslVertexData.h"

layout (constant_id = 0) const int SAMPLERS_ARRAY_SIZE = 2;
layout (constant_id = 1) const int IMAGES_ARRAY_SIZE = 128;

layout (set = 0, binding = 0) readonly buffer Vertices {
	Vertex vertices[];
} vertexBuffer[2];

layout (set = 0, binding = 1) readonly buffer Materials {
	MaterialData materials[];
};

layout (set = 1, binding = 0) uniform sampler immutableSamplers[SAMPLERS_ARRAY_SIZE];
layout (set = 1, binding = 1) uniform texture2DArray images[IMAGES_ARRAY_SIZE];

#include "XL2dGlslGradient.h"

layout (location = 0) in vec4 fragColor;
layout (location = 1) in vec4 fragTexCoord;
layout (location = 2) in vec4 shadowColor;
layout (location = 3) in vec4 outlineColor;
layout (location = 4) in vec2 fragPosition;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 outShadow;

layout (push_constant) uniform pcb {
	uint materialIdx;
	uint imageIdx;
	uint samplerIdx;
	uint gradientOffset;
	uint gradientCount;
	float outlineOffset; // 5
	uint padding1;
	uint padding2;
} pushConstants;

void main() {
	vec4 textureColor = texture(
		sampler2DArray(
			images[pushConstants.imageIdx],
			immutableSamplers[pushConstants.samplerIdx]
		), fragTexCoord.xyz);
	outColor = fragColor * textureColor;

	if (pushConstants.gradientCount > 0) {
		outColor = applyGradient(outColor, pushConstants.gradientOffset, pushConstants.gradientCount);
	}

	outShadow = shadowColor;
}
