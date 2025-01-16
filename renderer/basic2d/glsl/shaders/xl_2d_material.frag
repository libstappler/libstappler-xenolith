#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable

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
layout (set = 1, binding = 1) uniform texture2D images[IMAGES_ARRAY_SIZE];

#include "XL2dGlslGradient.h"

layout (location = 0) in vec4 fragColor;
layout (location = 1) in vec4 fragTexCoord;
layout (location = 2) in vec4 shadowColor;
layout (location = 3) in vec4 outlineColor;
layout (location = 4) in vec2 fragPosition;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 outShadow;

layout (push_constant) uniform pcb {
	uint materialIdx; // 0
	uint imageIdx; // 1
	uint samplerIdx; // 2
	uint gradientOffset; // 3
	uint gradientCount; // 4
	float outlineOffset; // 5
	uint padding1; // 6
	uint padding2; // 6
} pushConstants;

vec4 getSample(in sampler2D s, vec2 coord) {
	return texture(s, coord);
}

#define M_SQRT1_2 0.70710678118654752440
#define SAMPLER2D(image, sampler) sampler2D( images[image], immutableSamplers[sampler] )

float getOutlineSample(in vec2 coord, float initColor) {
	const uint nsamples = min(4, max(uint(ceil(pushConstants.outlineOffset)), 2));

	const vec2 offset = pushConstants.outlineOffset / textureSize(images[pushConstants.imageIdx], 0);
	const vec2 step = offset / float(nsamples - 1);
	const vec2 origin = coord - offset / 2.0;

	float accum = initColor;

	for (uint i = 0; i < nsamples; ++ i) {
		for (uint j = 0; j < nsamples; ++ j) {
			accum += texture(SAMPLER2D(pushConstants.imageIdx, pushConstants.samplerIdx), origin + vec2(step.x * i, step.y * j)).a;
		}
	}

	const float result = (0.5 - abs(0.5 - accum / (nsamples * nsamples + 1))) * 2.0 - 1.0;

	return -(result * result * result * result - 1);
}

void main() {
	vec4 textureColor = texture(SAMPLER2D(pushConstants.imageIdx, pushConstants.samplerIdx), fragTexCoord.xy);
	if (pushConstants.outlineOffset > 0.0) {
		float outlineSample = getOutlineSample(fragTexCoord.xy, textureColor.a);

		outColor = outlineColor * outlineSample + textureColor * (1.0 - outlineSample);
	} else {
		outColor = fragColor * textureColor;
	}

	if (pushConstants.gradientCount > 0) {
		outColor = applyGradient(outColor, pushConstants.gradientOffset, pushConstants.gradientCount);
	}

	outShadow = shadowColor;
}
