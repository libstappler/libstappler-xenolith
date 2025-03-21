// Наносит затенение на основе SDF-карты

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include "XL2dGlslVertexData.h"

layout (set = 0, binding = 0) readonly buffer Vertices {
	Vertex vertices[];
} vertexBuffer[2];

layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec2 fragTexCoord;

void main() {
	gl_Position = vertexBuffer[0].vertices[gl_VertexIndex].pos;
	fragColor = vertexBuffer[0].vertices[gl_VertexIndex].color;
	fragTexCoord = vertexBuffer[0].vertices[gl_VertexIndex].tex;
}
