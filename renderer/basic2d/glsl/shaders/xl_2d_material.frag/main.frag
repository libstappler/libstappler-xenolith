#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "XL2dGlslVertexData.h"

layout (constant_id = 0) const int SAMPLERS_ARRAY_SIZE = 2;
layout (constant_id = 1) const int IMAGES_ARRAY_SIZE = 128;

layout (push_constant) uniform pcb {
	uint materialIdx;
	uint imageIdx;
	uint samplerIdx;
	uint gradientOffset;
	uint gradientCount;
} pushConstants;

layout (set = 0, binding = 0) readonly buffer Vertices {
	Vertex vertices[];
} vertexBuffer[2];

layout (set = 0, binding = 1) readonly buffer Materials {
	MaterialData materials[];
};

layout (set = 1, binding = 0) uniform sampler immutableSamplers[SAMPLERS_ARRAY_SIZE];
layout (set = 1, binding = 1) uniform texture2D images[IMAGES_ARRAY_SIZE];

layout (location = 0) in vec4 fragColor;
layout (location = 1) in vec2 fragTexCoord;
layout (location = 2) in vec4 shadowColor;
layout (location = 3) in vec2 fragPosition;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 outShadow;

#define GRAD_COLORS(n) vertexBuffer[0].vertices[pushConstants.gradientOffset + 2 + n].color
#define GRAD_STEPS(n) vertexBuffer[0].vertices[pushConstants.gradientOffset + 2 + n].pos.z
#define GRAD_FACTOR(n) vertexBuffer[0].vertices[pushConstants.gradientOffset + 2 + n].pos.w

void main() {
	vec4 textureColor = texture(
		sampler2D(
			images[pushConstants.imageIdx],
			immutableSamplers[pushConstants.samplerIdx]
		), fragTexCoord);
	outColor = fragColor * textureColor;

	if (pushConstants.gradientCount > 0) {
		Vertex start = vertexBuffer[0].vertices[pushConstants.gradientOffset];
		Vertex end = vertexBuffer[0].vertices[pushConstants.gradientOffset + 1];

		float grad_loc;
		if (end.pos.x == start.pos.x) {
			grad_loc = (gl_FragCoord.y - start.pos.y) / (end.pos.y - start.pos.y);
		} else if (end.pos.y == start.pos.y) {
			grad_loc = (gl_FragCoord.x - start.pos.x) / (end.pos.x - start.pos.x);
		} else {
			float nx = (gl_FragCoord.x - start.pos.x) / (end.pos.x - start.pos.x);
			float ny = (gl_FragCoord.y - start.pos.y) / (end.pos.y - start.pos.y);

			grad_loc = nx + (ny - nx) * start.tex.y;
		}

		float gradFactor = mix(
			GRAD_FACTOR(0),
			GRAD_FACTOR(1),
			smoothstep(
				GRAD_STEPS(0),
				GRAD_STEPS(1), grad_loc ) );

		vec4 gradColor = mix(
			GRAD_COLORS(0),
			GRAD_COLORS(1),
			smoothstep(
				GRAD_STEPS(0),
				GRAD_STEPS(1), grad_loc ) );
	    for ( int i = 2; i < pushConstants.gradientCount; ++i ) {
	        gradColor = mix(
	        	gradColor,
	        	GRAD_COLORS(i),
	        	smoothstep(
	        		GRAD_STEPS(i-1),
	        		GRAD_STEPS(i), grad_loc ) );
	        gradFactor = mix(
	        	gradFactor,
				GRAD_FACTOR(i),
	        	smoothstep(
	        		GRAD_STEPS(i-1),
	        		GRAD_STEPS(i), grad_loc ) );
	    }
	    
	    float a1 = outColor.a;
	    float a2 = gradColor.a;
	    outColor = outColor * gradColor * (1.0 - gradFactor) + gradFactor * (outColor * (1.0 - a2) + gradColor * a2);
	    outColor.a = (1.0 - gradFactor) * a1 * a2 + gradFactor * a1;
	}

	outShadow = shadowColor;
}
