#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "XL2dGlslVertexData.h"
#include "XL2dGlslShadowData.h"
#include "XL2dGlslSdfData.h"

#define OUTPUT_BUFFER_LAYOUT_SIZE 3

layout (constant_id = 0) const int SAMPLERS_ARRAY_SIZE = 2;

layout (push_constant) uniform pcb {
	uint samplerIdx;
	uint padding0;
	uint padding1;
} pushConstants;

layout (set = 0, binding = 2) uniform ShadowDataBuffer {
	ShadowData shadowData;
};

layout(set = 0, binding = 3) buffer ObjectsBuffer {
	Sdf2DObjectData objects[];
} objectsBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 3) buffer GridSizeBuffer {
	uint grid[];
} gridSizeBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 3) buffer GridIndexesBuffer {
	uint index[];
} gridIndexBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(input_attachment_index = 0, set = 0, binding = 4) uniform subpassInput inputDepth;

layout(set = 0, binding = 5) uniform texture2D sdfImage;

layout(set = 1, binding = 0) uniform sampler immutableSamplers[SAMPLERS_ARRAY_SIZE];

layout (location = 0) in vec4 fragColor;
layout (location = 0) out vec4 outColor;
layout (location = 1) in vec2 fragTexCoord;

uint s_cellIdx;

#define GAUSSIAN_CONST -6.2383246250

uint hit(in vec2 p, float h) {
	uint idx;
	uint targetOffset = s_cellIdx * shadowData.objectsCount;
	uint cellSize = gridSizeBuffer[1].grid[s_cellIdx];

	for (uint i = 0; i < cellSize; ++ i) {
		idx = gridIndexBuffer[2].index[targetOffset + i];
		// value кодируется как f32, а h как f16, без коррекции точности будет мерцать
		if (objectsBuffer[0].objects[idx].value > h + 0.1) {
			if (all(greaterThan(p, objectsBuffer[0].objects[idx].bbMin)) && all(lessThan(p, objectsBuffer[0].objects[idx].bbMax))) {
				return 1;
			}
		}
	}

	return 0;
}

void main() {
	vec2 coords = (vec2(fragTexCoord.x, 1.0 - fragTexCoord.y) / shadowData.pix) * shadowData.shadowDensity;
	uvec2 cellIdx = uvec2(floor(coords)) / shadowData.gridSize;
	s_cellIdx = cellIdx.y * (shadowData.gridWidth) + cellIdx.x;

	float depth = subpassLoad(inputDepth).r;
	float isect = texture(sampler2D(sdfImage, immutableSamplers[pushConstants.samplerIdx]), fragTexCoord).y;
	vec2 sdfValue = texture(sampler2D(sdfImage, immutableSamplers[pushConstants.samplerIdx]), fragTexCoord).xw;

	//outColor = float(int(sdfValue.x) % 20).xxxx / 20.0f;
	//outColor.r = sdfValue.y;
	//return;

	if (sdfValue.y < depth + 0.1) {
		outColor = shadowData.discardColor;
		//outColor = vec4(1, sdfValue.y / 20.0f, 1, 1);
		return;
	}

	if (hit(coords, depth) == 1) {
		vec4 textureColor = shadowData.globalColor;
		for (uint i = 0; i < shadowData.ambientLightCount; ++ i) {
			float k = shadowData.ambientLights[i].normal.w;

			vec4 targetSdf = texture(sampler2D(sdfImage, immutableSamplers[pushConstants.samplerIdx]),
				fragTexCoord + shadowData.ambientLights[i].normal.xy * (sdfValue.y - depth));
			const float v = targetSdf.x;
			const float h = shadowData.maxValue - targetSdf.w;
			const float h2 = targetSdf.w - depth;
			const float targetValue = sqrt((v * v - h * h) + h2 * h2);
			float sdf = clamp( (max(targetValue, h2) - h2 * 0.5) / ((h2 * 0.55) * k * k), 0.0, 1.0);

			textureColor += shadowData.ambientLights[i].color
				* shadowData.ambientLights[i].color.a.xxxx
				* (1.0 - exp(sdf * sdf * sdf * GAUSSIAN_CONST)).xxxx
				//* (mod(targetValue, 12.0) / 12.0).xxxx
				//* isect / 8.0
				* shadowData.luminosity;
		}

		outColor = vec4(textureColor.xyz, 1.0);
		//outColor = vec4((mod(sdfValue.x, 10.0) / 20.0), floor(sdfValue.x / 16.0) / 4.0, clamp(sdfValue.y / 6.0, 0.0, 1.0), 1.0);
	} else {
		outColor = shadowData.discardColor;
		//outColor = vec4(1, 1, 0.75, 1);
	}
}
