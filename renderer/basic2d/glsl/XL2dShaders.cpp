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

#include "XL2dShaders.h"

namespace stappler::xenolith::basic2d::shaders {

#include "xl_2d_material.frag"
#include "xl_2d_material.vert"
#include "xl_sdf_triangles.comp"
#include "xl_sdf_circles.comp"
#include "xl_sdf_rects.comp"
#include "xl_sdf_rounded_rects.comp"
#include "xl_sdf_polygons.comp"
#include "xl_sdf_shadows.frag"
#include "xl_sdf_shadows.vert"
#include "xl_sdf_image.comp"

SpanView<uint32_t> MaterialFrag(reinterpret_cast<const uint32_t *>(xl_2d_material_frag), xl_2d_material_frag_len / sizeof(uint32_t));
SpanView<uint32_t> MaterialVert(reinterpret_cast<const uint32_t *>(xl_2d_material_vert), xl_2d_material_vert_len / sizeof(uint32_t));
SpanView<uint32_t> SdfTrianglesComp(reinterpret_cast<const uint32_t *>(xl_sdf_triangles_comp), xl_sdf_triangles_comp_len / sizeof(uint32_t));
SpanView<uint32_t> SdfCirclesComp(reinterpret_cast<const uint32_t *>(xl_sdf_circles_comp), xl_sdf_circles_comp_len / sizeof(uint32_t));
SpanView<uint32_t> SdfRectsComp(reinterpret_cast<const uint32_t *>(xl_sdf_rects_comp), xl_sdf_rects_comp_len / sizeof(uint32_t));
SpanView<uint32_t> SdfRoundedRectsComp(reinterpret_cast<const uint32_t *>(xl_sdf_rounded_rects_comp), xl_sdf_rounded_rects_comp_len / sizeof(uint32_t));
SpanView<uint32_t> SdfPolygonsComp(reinterpret_cast<const uint32_t *>(xl_sdf_polygons_comp), xl_sdf_polygons_comp_len / sizeof(uint32_t));
SpanView<uint32_t> SdfShadowsFrag(reinterpret_cast<const uint32_t *>(xl_sdf_shadows_frag), xl_sdf_shadows_frag_len / sizeof(uint32_t));
SpanView<uint32_t> SdfShadowsVert(reinterpret_cast<const uint32_t *>(xl_sdf_shadows_vert), xl_sdf_shadows_vert_len / sizeof(uint32_t));
SpanView<uint32_t> SdfImageComp(reinterpret_cast<const uint32_t *>(xl_sdf_image_comp), xl_sdf_image_comp_len / sizeof(uint32_t));

}
