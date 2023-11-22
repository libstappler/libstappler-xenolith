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

#ifndef XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLSDFDESCRIPTORS_H_
#define XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLSDFDESCRIPTORS_H_

#if SP_GLSL

layout (set = 0, binding = 0) uniform ShadowDataBuffer {
	ShadowData shadowData;
};

#define INPUT_BUFFER_LAYOUT_SIZE 7
#define OUTPUT_BUFFER_LAYOUT_SIZE 3

layout (set = 0, binding = 1) readonly buffer TriangleIndexes {
	Triangle2DIndex indexes[];
} indexBuffer[INPUT_BUFFER_LAYOUT_SIZE];

layout (set = 0, binding = 1) buffer Vertices {
	vec4 vertices[];
} vertexBuffer[INPUT_BUFFER_LAYOUT_SIZE];

layout (set = 0, binding = 1) readonly buffer TransformObjects {
	TransformData transforms[];
} transformObjectBuffer[INPUT_BUFFER_LAYOUT_SIZE];

layout (set = 0, binding = 1) readonly buffer CircleIndexes {
	Circle2DIndex circles[];
} circleIndexBuffer[INPUT_BUFFER_LAYOUT_SIZE];

layout (set = 0, binding = 1) readonly buffer RectIndexes {
	Rect2DIndex rects[];
} rectIndexBuffer[INPUT_BUFFER_LAYOUT_SIZE];

layout (set = 0, binding = 1) readonly buffer RoundedRectIndexes {
	RoundedRect2DIndex rects[];
} roundedRectIndexBuffer[INPUT_BUFFER_LAYOUT_SIZE];

layout (set = 0, binding = 1) readonly buffer PolygonIndexes {
	Polygon2DIndex polygons[];
} polygonIndexBuffer[INPUT_BUFFER_LAYOUT_SIZE];

#define TRIANGLE_INDEX_BUFFER indexBuffer[0].indexes
#define CIRCLE_INDEX_BUFFER circleIndexBuffer[3].circles
#define RECT_INDEX_BUFFER rectIndexBuffer[4].rects
#define ROUNDED_RECT_INDEX_BUFFER roundedRectIndexBuffer[5].rects
#define POLYGON_INDEX_BUFFER polygonIndexBuffer[6].polygons
#define VERTEX_BUFFER vertexBuffer[1].vertices
#define TRANSFORM_BUFFER transformObjectBuffer[2].transforms

layout(set = 0, binding = 2) buffer ObjectsBuffer {
	Sdf2DObjectData objects[];
} objectsBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 2) buffer GridSizeBuffer {
	uint grid[];
} gridSizeBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 2) buffer GridIndexesBuffer {
	uint index[];
} gridIndexBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

#define OBJECTS_DATA_BUFFER objectsBuffer[0].objects
#define GRID_SIZE_BUFFER gridSizeBuffer[1].grid
#define GRID_INDEX_BUFFER gridIndexBuffer[2].index

void emplaceIntoGrid(const in int i, const in int j, const in uint gID) {
	if (i >= 0 && i < shadowData.gridWidth && j >= 0 && j < shadowData.gridHeight) {
		uint gridId = j * shadowData.gridWidth + i;
		uint target = atomicAdd(GRID_SIZE_BUFFER[gridId], 1);
		GRID_INDEX_BUFFER[gridId * shadowData.objectsCount + target] = gID;
	}
}

#endif

#endif /* XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLSDFDESCRIPTORS_H_ */
