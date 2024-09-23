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

// Excluded from documentation/codegen tool
///@ SP_EXCLUDE

#include "XL2dShaders.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::shaders {

#include "xl_2d_material.frag.h"
#include "xl_2d_material.vert.h"
#include "xl_2d_pseudosdf.frag.h"
#include "xl_2d_pseudosdf.vert.h"
#include "xl_2d_pseudosdf_shadow.frag.h"
#include "xl_2d_pseudosdf_shadow.vert.h"

SpanView<uint32_t> MaterialFrag(reinterpret_cast<const uint32_t *>(xl_2d_material_frag), xl_2d_material_frag_len / sizeof(uint32_t));
SpanView<uint32_t> MaterialVert(reinterpret_cast<const uint32_t *>(xl_2d_material_vert), xl_2d_material_vert_len / sizeof(uint32_t));
SpanView<uint32_t> PseudoSdfFrag(reinterpret_cast<const uint32_t *>(xl_2d_pseudosdf_frag), xl_2d_pseudosdf_frag_len / sizeof(uint32_t));
SpanView<uint32_t> PseudoSdfVert(reinterpret_cast<const uint32_t *>(xl_2d_pseudosdf_vert), xl_2d_pseudosdf_vert_len / sizeof(uint32_t));
SpanView<uint32_t> PseudoSdfShadowFrag(reinterpret_cast<const uint32_t *>(xl_2d_pseudosdf_shadow_frag), xl_2d_pseudosdf_shadow_frag_len / sizeof(uint32_t));
SpanView<uint32_t> PseudoSdfShadowVert(reinterpret_cast<const uint32_t *>(xl_2d_pseudosdf_shadow_vert), xl_2d_pseudosdf_shadow_vert_len / sizeof(uint32_t));

}
