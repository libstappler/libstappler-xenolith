#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#ifndef SP_GLSL
#define SP_GLSL
#endif

#include "SPGlslInit.h"
#include "XL2dGlslVertexData.h"

layout (constant_id = 0) const int BUFFERS_ARRAY_SIZE = 8;

layout (push_constant) uniform pcb {
	uint materialIdx;
	uint padding0;
	uint padding1;
	uint padding2;
	uint padding3;
} pushConstants;

layout (set = 0, binding = 0) readonly buffer Vertices {
	Vertex vertices[];
} vertexBuffer[3];

layout (set = 0, binding = 0) readonly buffer TransformObjects {
	TransformData objects[];
} transformObjectBuffer[3];

layout (set = 0, binding = 1) readonly buffer Materials {
	MaterialData materials[];
};

layout (set = 1, binding = 2) readonly buffer DataAtlasIndexBuffer {
	DataAtlasIndex indexes[];
} dataAtlasIndexes[BUFFERS_ARRAY_SIZE];

layout (set = 1, binding = 2) readonly buffer DataAtlasValueBuffer {
	DataAtlasValue values[];
} dataAtlasValues[BUFFERS_ARRAY_SIZE];

layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec2 fragTexCoord;
layout (location = 2) out vec4 shadowColor;
layout (location = 3) out vec2 fragPosition;

uint hash(uint k, uint capacity) {
	k ^= k >> 16;
	k *= 0x85ebca6b;
	k ^= k >> 13;
	k *= 0xc2b2ae35;
	k ^= k >> 16;
	return k & (capacity - 1);
}

void main() {
	const Vertex vertex = vertexBuffer[0].vertices[gl_VertexIndex];
	const TransformData transform = transformObjectBuffer[1].objects[vertex.material >> 16];
	const TransformData instance = transformObjectBuffer[1].objects[gl_InstanceIndex];
	const MaterialData mat = materials[pushConstants.materialIdx];

	vec4 pos = vertex.pos;
	vec4 color = vertex.color;
	vec2 tex = vertex.tex;

	if (vertex.object != 0 && (mat.flags & XL_GLSL_MATERIAL_FLAG_HAS_ATLAS) != 0) {
		uint size = 1 << (mat.flags >> XL_GLSL_MATERIAL_FLAG_ATLAS_POW2_INDEX_BIT_OFFSET);
		uint slot = hash(vertex.object, size);
		uint dataAtlasIndex = mat.atlasIdx & 0xFFFF;
		uint dataAtlasValue = mat.atlasIdx >> 16;
		uint counter = 0;
		while (counter < size) {
			const DataAtlasIndex prev = dataAtlasIndexes[dataAtlasIndex].indexes[slot];
			if (prev.key == vertex.object) {
				const DataAtlasValue value = dataAtlasValues[dataAtlasValue].values[dataAtlasIndexes[dataAtlasIndex].indexes[slot].value];

				pos += vec4(value.pos, 0, 0);
				tex = value.tex;
				break;
			} else if (prev.key == uint(0xffffffff)) {
				color = vec4(1, 0, 0, 1);
				break;
			}
			slot = (slot + 1) & (size - 1);
			++ counter;
		}
		if (counter == size) {
			color = vec4(0, 1, 0, 1);
		}
	}

	gl_Position = transform.transform * instance.transform * pos * transform.mask * instance.mask + transform.offset + instance.offset;
	fragPosition = gl_Position.xy;
	fragColor = color * instance.color;
	fragTexCoord = tex;
	shadowColor = transform.shadow;
}
