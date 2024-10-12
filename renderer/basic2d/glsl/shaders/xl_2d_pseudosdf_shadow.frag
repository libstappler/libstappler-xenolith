// Наносит затенение на основе SDF-карты

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

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

layout(input_attachment_index = 0, set = 0, binding = 3) uniform subpassInput inputDepth;
layout(input_attachment_index = 1, set = 0, binding = 4) uniform subpassInput inputSdf;

layout(set = 1, binding = 0) uniform sampler immutableSamplers[SAMPLERS_ARRAY_SIZE];

layout (location = 0) in vec4 fragColor;
layout (location = 0) out vec4 outColor;
layout (location = 1) in vec2 fragTexCoord;

#define M_1_PI 0.318309886183790671538
#define GAUSSIAN_CONST -6.2383246250

vec2 project(in vec2 target, in vec2 norm) {
	return norm * (dot(target, norm) / dot(norm, norm));
}

void main() {
	const vec4 sdfSource = subpassLoad(inputSdf);
	const float depth = subpassLoad(inputDepth).r;

	if (sdfSource.x > 200.0) {
		outColor = shadowData.discardColor;
	}

	if (sdfSource.w <= depth) {
		outColor = shadowData.discardColor;
		//outColor = vec4(1.0, 0.0, 1.0, 1.0);
		return;
	}

	// Направление нормали в SDF
	const vec2 norm = sdfSource.yz;

	vec4 textureColor = shadowData.globalColor;
	for (uint i = 0; i < shadowData.ambientLightCount; ++ i) {
		// Расстояние по высоте от текущего фрагмента до источника SDF
		const float dh = sdfSource.w - depth;

		const float offset = dot(shadowData.ambientLights[i].normal.xy * dh, norm);

		// Значение SDF
		const float v = sdfSource.x;

		// Значение SDF в плоскости объекта
		const float d = v * mix( 0.8, 1.0, clamp( dh / 40.0f, 0.0, 1.0))
			* mix( 0.2, 1.0, clamp( dh / 4.0f, 0.0, 1.0))
			+ 0.25
			// + 0.4 * clamp(1.0 - dh / 3.0f, 0.0, 0.5)
			+ offset;

		const float k = shadowData.ambientLights[i].normal.w;

		// Радиус конуса выбора цвета для плоскости объекта
		const float R = dh * k;

		if (d < R) {
			// Полуугол незасвеченного сектора
			const float alpha = 2.0 * acos(d / R);

			// Отноление площади незасвеченного сектора к площади сечения конуса
			// Для реальных данных коэффициент будет 0.5, параметры распределения подобраны эмпирически
			const float S = 1.0 - (alpha - sin(alpha)) * M_1_PI * mix(0.85, 0.7, clamp(k / 2.0, 0.0, 1.0));

			textureColor += shadowData.ambientLights[i].color
				* (shadowData.ambientLights[i].color.a
				//* S
				* (1.0 - exp(S * S * S * GAUSSIAN_CONST))
				* shadowData.luminosity);
		} else {
			textureColor += shadowData.ambientLights[i].color
				* (shadowData.ambientLights[0].color.a * shadowData.luminosity);
		}
	}

	outColor = vec4(textureColor.xyz, 1.0);
}
