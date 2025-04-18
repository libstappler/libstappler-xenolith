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

#include "stappler-buildconfig.h"

#if MODULE_XENOLITH_CORE

#include "XLCommon.h"
#include "XLSnnVkShaders.h"

namespace stappler::xenolith::vk::shadernn {

#include "gen_f32.comp.h"
#include "norm_f32.comp.h"
#include "vk_activation_f32.comp.h"
#include "vk_add_f32.comp.h"
#include "vk_avgpool2d_f32.comp.h"
#include "vk_batchnorm_f32.comp.h"
#include "vk_concat_f32.comp.h"
#include "vk_conv2d_1x1_f32.comp.h"
#include "vk_conv2d_f32.comp.h"
#include "vk_dense_f32.comp.h"
#include "vk_depthwise_f32.comp.h"
#include "vk_flatten_f32.comp.h"
#include "vk_instancenorm_f32.comp.h"
#include "vk_maxpool2d_f32.comp.h"
#include "vk_pad_f32.comp.h"
#include "vk_resize_f32.comp.h"
#include "vk_subpixel_f32.comp.h"
#include "vk_unary_f32.comp.h"
#include "vk_upsampling2d_bilinear_f32.comp.h"
#include "vk_upsampling2d_nearest_f32.comp.h"

#include "gen_f16.comp.h"
#include "norm_f16.comp.h"
#include "vk_activation_f16.comp.h"
#include "vk_add_f16.comp.h"
#include "vk_avgpool2d_f16.comp.h"
#include "vk_batchnorm_f16.comp.h"
#include "vk_concat_f16.comp.h"
#include "vk_conv2d_1x1_f16.comp.h"
#include "vk_conv2d_f16.comp.h"
#include "vk_dense_f16.comp.h"
#include "vk_depthwise_f16.comp.h"
#include "vk_flatten_f16.comp.h"
#include "vk_instancenorm_f16.comp.h"
#include "vk_maxpool2d_f16.comp.h"
#include "vk_pad_f16.comp.h"
#include "vk_resize_f16.comp.h"
#include "vk_subpixel_f16.comp.h"
#include "vk_unary_f16.comp.h"
#include "vk_upsampling2d_bilinear_f16.comp.h"
#include "vk_upsampling2d_nearest_f16.comp.h"

#include "AddVectorToMatrixRows.comp.h"
#include "BufferNorm.comp.h"
#include "MultiplyMatrixByMatrix.comp.h"
#include "MultiplyMatrixByMatrixBorders.comp.h"
#include "MultiplyMatrixByTransposedMatrix.comp.h"
#include "MultiplyMatrixByTransposedMatrixBorders.comp.h"
#include "MultiplyTransposedMatrixByMatrix.comp.h"
#include "MultiplyTransposedMatrixByMatrixBorders.comp.h"
#include "MatrixSoftmaxByRows.comp.h"
#include "VectorAddFloat1.comp.h"
#include "VectorAddFloat4.comp.h"
#include "VectorReLU.comp.h"
#include "VectorReLU4.comp.h"
#include "VectorReLUDiff.comp.h"
#include "VectorLog.comp.h"
#include "VectorDotProduct.comp.h"
#include "VectorMultiplyFloat.comp.h"
#include "VectorMultiplyAndAdd.comp.h"
#include "VectorSubFloat.comp.h"
#include "SumMatrixColumns.comp.h"
#include "SumMatrixRows.comp.h"
#include "MultiplyDiagMatrixByMatrix.comp.h"

#include "StatNorm.comp.h"
#include "StatClassMap.comp.h"
#include "StatClassPercent.comp.h"
#include "StatAnalysis.comp.h"

SpanView<uint32_t> GenF32Comp(gen_f32_comp);
SpanView<uint32_t> NormF32Comp(norm_f32_comp);
SpanView<uint32_t> ActivationF32Comp(vk_activation_f32_comp);
SpanView<uint32_t> AddF32Comp(vk_add_f32_comp);
SpanView<uint32_t> Avgpool2dF32Comp(vk_avgpool2d_f32_comp);
SpanView<uint32_t> BatchnormF32Comp(vk_batchnorm_f32_comp);
SpanView<uint32_t> ConcatF32Comp(vk_concat_f32_comp);
SpanView<uint32_t> Conv2d1x1F32Comp(vk_conv2d_1x1_f32_comp);
SpanView<uint32_t> Conv2dF32Comp(vk_conv2d_f32_comp);
SpanView<uint32_t> DenseF32Comp(vk_dense_f32_comp);
SpanView<uint32_t> DepthwiseF32Comp(vk_depthwise_f32_comp);
SpanView<uint32_t> FlattenF32Comp(vk_flatten_f32_comp);
SpanView<uint32_t> InstancenormF32Comp(vk_instancenorm_f32_comp);
SpanView<uint32_t> Maxpool2dF32Comp(vk_maxpool2d_f32_comp);
SpanView<uint32_t> PadF32Comp(vk_pad_f32_comp);
SpanView<uint32_t> ResizeF32Comp(vk_resize_f32_comp);
SpanView<uint32_t> SubpixelF32Comp(vk_subpixel_f32_comp);
SpanView<uint32_t> UnaryF32Comp(vk_unary_f32_comp);
SpanView<uint32_t> Upsampling2dBilinearF32Comp(vk_upsampling2d_bilinear_f32_comp);
SpanView<uint32_t> Upsampling2dNearestF32Comp(vk_upsampling2d_nearest_f32_comp);

SpanView<uint32_t> GenF16Comp(gen_f16_comp);
SpanView<uint32_t> NormF16Comp(norm_f16_comp);
SpanView<uint32_t> ActivationF16Comp(vk_activation_f16_comp);
SpanView<uint32_t> AddF16Comp(vk_add_f16_comp);
SpanView<uint32_t> Avgpool2dF16Comp(vk_avgpool2d_f16_comp);
SpanView<uint32_t> BatchnormF16Comp(vk_batchnorm_f16_comp);
SpanView<uint32_t> ConcatF16Comp(vk_concat_f16_comp);
SpanView<uint32_t> Conv2d1x1F16Comp(vk_conv2d_1x1_f16_comp);
SpanView<uint32_t> Conv2dF16Comp(vk_conv2d_f16_comp);
SpanView<uint32_t> DenseF16Comp(vk_dense_f16_comp);
SpanView<uint32_t> DepthwiseF16Comp(vk_depthwise_f16_comp);
SpanView<uint32_t> FlattenF16Comp(vk_flatten_f16_comp);
SpanView<uint32_t> InstancenormF16Comp(vk_instancenorm_f16_comp);
SpanView<uint32_t> Maxpool2dF16Comp(vk_maxpool2d_f16_comp);
SpanView<uint32_t> PadF16Comp(vk_pad_f16_comp);
SpanView<uint32_t> ResizeF16Comp(vk_resize_f16_comp);
SpanView<uint32_t> SubpixelF16Comp(vk_subpixel_f16_comp);
SpanView<uint32_t> UnaryF16Comp(vk_unary_f16_comp);
SpanView<uint32_t> Upsampling2dBilinearF16Comp(vk_upsampling2d_bilinear_f16_comp);
SpanView<uint32_t> Upsampling2dNearestF16Comp(vk_upsampling2d_nearest_f16_comp);

SpanView<uint32_t> AddVectorToMatrixRowsComp(AddVectorToMatrixRows_comp);
SpanView<uint32_t> BufferNormComp(BufferNorm_comp);
SpanView<uint32_t> MultiplyMatrixByMatrixComp(MultiplyMatrixByMatrix_comp);
SpanView<uint32_t> MultiplyMatrixByMatrixBordersComp(MultiplyMatrixByMatrixBorders_comp);
SpanView<uint32_t> MultiplyMatrixByTransposedMatrixComp(MultiplyMatrixByTransposedMatrix_comp);
SpanView<uint32_t> MultiplyMatrixByTransposedMatrixBordersComp(MultiplyMatrixByTransposedMatrixBorders_comp);
SpanView<uint32_t> MultiplyTransposedMatrixByMatrixComp(MultiplyTransposedMatrixByMatrix_comp);
SpanView<uint32_t> MultiplyTransposedMatrixByMatrixBordersComp(MultiplyTransposedMatrixByMatrixBorders_comp);
SpanView<uint32_t> MatrixSoftmaxByRowsComp(MatrixSoftmaxByRows_comp);
SpanView<uint32_t> VectorAddFloat1Comp(VectorAddFloat1_comp);
SpanView<uint32_t> VectorAddFloat4Comp(VectorAddFloat4_comp);
SpanView<uint32_t> VectorReLUComp(VectorReLU_comp);
SpanView<uint32_t> VectorReLU4Comp(VectorReLU4_comp);
SpanView<uint32_t> VectorReLUDiffComp(VectorReLUDiff_comp);
SpanView<uint32_t> VectorLogComp(VectorLog_comp);
SpanView<uint32_t> VectorDotProductComp(VectorDotProduct_comp);
SpanView<uint32_t> VectorMultiplyFloatComp(VectorMultiplyFloat_comp);
SpanView<uint32_t> VectorMultiplyAndAddComp(VectorMultiplyAndAdd_comp);
SpanView<uint32_t> VectorSubComp(VectorSubFloat_comp);
SpanView<uint32_t> SumMatrixColumnsComp(SumMatrixColumns_comp);
SpanView<uint32_t> SumMatrixRowsComp(SumMatrixRows_comp);
SpanView<uint32_t> MultiplyDiagMatrixByMatrixComp(MultiplyDiagMatrixByMatrix_comp);

SpanView<uint32_t> StatNormComp(StatNorm_comp);
SpanView<uint32_t> StatClassMapComp(StatClassMap_comp);
SpanView<uint32_t> StatClassPercentComp(StatClassPercent_comp);
SpanView<uint32_t> StatAnalysisComp(StatAnalysis_comp);

Precision getAttachmentPrecision(const core::AttachmentData *data) {
	if (data->type == core::AttachmentType::Image) {
		auto img = static_cast<core::ImageAttachment *>(data->attachment.get());
		auto fmt = img->getImageInfo().format;
		switch (fmt) {
		case core::ImageFormat::R8_UNORM:
		case core::ImageFormat::R8_SNORM:
		case core::ImageFormat::R8_USCALED:
		case core::ImageFormat::R8_SSCALED:
		case core::ImageFormat::R8_UINT:
		case core::ImageFormat::R8_SINT:
		case core::ImageFormat::R8_SRGB:
		case core::ImageFormat::R8G8_UNORM:
		case core::ImageFormat::R8G8_SNORM:
		case core::ImageFormat::R8G8_USCALED:
		case core::ImageFormat::R8G8_SSCALED:
		case core::ImageFormat::R8G8_UINT:
		case core::ImageFormat::R8G8_SINT:
		case core::ImageFormat::R8G8_SRGB:
		case core::ImageFormat::R8G8B8_UNORM:
		case core::ImageFormat::R8G8B8_SNORM:
		case core::ImageFormat::R8G8B8_USCALED:
		case core::ImageFormat::R8G8B8_SSCALED:
		case core::ImageFormat::R8G8B8_UINT:
		case core::ImageFormat::R8G8B8_SINT:
		case core::ImageFormat::R8G8B8_SRGB:
		case core::ImageFormat::B8G8R8_UNORM:
		case core::ImageFormat::B8G8R8_SNORM:
		case core::ImageFormat::B8G8R8_USCALED:
		case core::ImageFormat::B8G8R8_SSCALED:
		case core::ImageFormat::B8G8R8_UINT:
		case core::ImageFormat::B8G8R8_SINT:
		case core::ImageFormat::B8G8R8_SRGB:
		case core::ImageFormat::R8G8B8A8_UNORM:
		case core::ImageFormat::R8G8B8A8_SNORM:
		case core::ImageFormat::R8G8B8A8_USCALED:
		case core::ImageFormat::R8G8B8A8_SSCALED:
		case core::ImageFormat::R8G8B8A8_UINT:
		case core::ImageFormat::R8G8B8A8_SINT:
		case core::ImageFormat::R8G8B8A8_SRGB:
		case core::ImageFormat::B8G8R8A8_UNORM:
		case core::ImageFormat::B8G8R8A8_SNORM:
		case core::ImageFormat::B8G8R8A8_USCALED:
		case core::ImageFormat::B8G8R8A8_SSCALED:
		case core::ImageFormat::B8G8R8A8_UINT:
		case core::ImageFormat::B8G8R8A8_SINT:
		case core::ImageFormat::B8G8R8A8_SRGB:
		case core::ImageFormat::A8B8G8R8_UNORM_PACK32:
		case core::ImageFormat::A8B8G8R8_SNORM_PACK32:
		case core::ImageFormat::A8B8G8R8_USCALED_PACK32:
		case core::ImageFormat::A8B8G8R8_SSCALED_PACK32:
		case core::ImageFormat::A8B8G8R8_UINT_PACK32:
		case core::ImageFormat::A8B8G8R8_SINT_PACK32:
		case core::ImageFormat::A8B8G8R8_SRGB_PACK32:
			return Precision::F8;
			break;
		case core::ImageFormat::A2R10G10B10_UNORM_PACK32:
		case core::ImageFormat::A2R10G10B10_SNORM_PACK32:
		case core::ImageFormat::A2R10G10B10_USCALED_PACK32:
		case core::ImageFormat::A2R10G10B10_SSCALED_PACK32:
		case core::ImageFormat::A2R10G10B10_UINT_PACK32:
		case core::ImageFormat::A2R10G10B10_SINT_PACK32:
		case core::ImageFormat::A2B10G10R10_UNORM_PACK32:
		case core::ImageFormat::A2B10G10R10_SNORM_PACK32:
		case core::ImageFormat::A2B10G10R10_USCALED_PACK32:
		case core::ImageFormat::A2B10G10R10_SSCALED_PACK32:
		case core::ImageFormat::A2B10G10R10_UINT_PACK32:
		case core::ImageFormat::A2B10G10R10_SINT_PACK32:
		case core::ImageFormat::R16_UNORM:
		case core::ImageFormat::R16_SNORM:
		case core::ImageFormat::R16_USCALED:
		case core::ImageFormat::R16_SSCALED:
		case core::ImageFormat::R16_UINT:
		case core::ImageFormat::R16_SINT:
		case core::ImageFormat::R16_SFLOAT:
		case core::ImageFormat::R16G16_UNORM:
		case core::ImageFormat::R16G16_SNORM:
		case core::ImageFormat::R16G16_USCALED:
		case core::ImageFormat::R16G16_SSCALED:
		case core::ImageFormat::R16G16_UINT:
		case core::ImageFormat::R16G16_SINT:
		case core::ImageFormat::R16G16_SFLOAT:
		case core::ImageFormat::R16G16B16_UNORM:
		case core::ImageFormat::R16G16B16_SNORM:
		case core::ImageFormat::R16G16B16_USCALED:
		case core::ImageFormat::R16G16B16_SSCALED:
		case core::ImageFormat::R16G16B16_UINT:
		case core::ImageFormat::R16G16B16_SINT:
		case core::ImageFormat::R16G16B16_SFLOAT:
		case core::ImageFormat::R16G16B16A16_UNORM:
		case core::ImageFormat::R16G16B16A16_SNORM:
		case core::ImageFormat::R16G16B16A16_USCALED:
		case core::ImageFormat::R16G16B16A16_SSCALED:
		case core::ImageFormat::R16G16B16A16_UINT:
		case core::ImageFormat::R16G16B16A16_SINT:
		case core::ImageFormat::R16G16B16A16_SFLOAT:
			return Precision::F16;
			break;
		case core::ImageFormat::R32_UINT:
		case core::ImageFormat::R32_SINT:
		case core::ImageFormat::R32_SFLOAT:
		case core::ImageFormat::R32G32_UINT:
		case core::ImageFormat::R32G32_SINT:
		case core::ImageFormat::R32G32_SFLOAT:
		case core::ImageFormat::R32G32B32_UINT:
		case core::ImageFormat::R32G32B32_SINT:
		case core::ImageFormat::R32G32B32_SFLOAT:
		case core::ImageFormat::R32G32B32A32_UINT:
		case core::ImageFormat::R32G32B32A32_SINT:
		case core::ImageFormat::R32G32B32A32_SFLOAT:
			return Precision::F32;
			break;
		case core::ImageFormat::R64_UINT:
		case core::ImageFormat::R64_SINT:
		case core::ImageFormat::R64_SFLOAT:
		case core::ImageFormat::R64G64_UINT:
		case core::ImageFormat::R64G64_SINT:
		case core::ImageFormat::R64G64_SFLOAT:
		case core::ImageFormat::R64G64B64_UINT:
		case core::ImageFormat::R64G64B64_SINT:
		case core::ImageFormat::R64G64B64_SFLOAT:
		case core::ImageFormat::R64G64B64A64_UINT:
		case core::ImageFormat::R64G64B64A64_SINT:
		case core::ImageFormat::R64G64B64A64_SFLOAT:
			return Precision::F32;
			break;
		default:
			return Precision::Unknown;
			break;
		}
	}
	return Precision::Unknown;
}

SpanView<uint32_t> getShader(LayerShader sh, Precision p) {
	switch (p) {
	case Precision::F16:
		switch (sh) {
		case LayerShader::Gen: return GenF16Comp; break;
		case LayerShader::Norm: return NormF16Comp; break;
		case LayerShader::Activation: return ActivationF16Comp; break;
		case LayerShader::Add: return AddF16Comp; break;
		case LayerShader::Avgpool2d: return Avgpool2dF16Comp; break;
		case LayerShader::Batchnorm: return BatchnormF16Comp; break;
		case LayerShader::Concat: return ConcatF16Comp; break;
		case LayerShader::Conv2d1x1: return Conv2d1x1F16Comp; break;
		case LayerShader::Conv2d: return Conv2dF16Comp; break;
		case LayerShader::Dense: return DenseF16Comp; break;
		case LayerShader::Depthwise: return DepthwiseF16Comp; break;
		case LayerShader::Flatten: return FlattenF16Comp; break;
		case LayerShader::Instancenorm: return InstancenormF16Comp; break;
		case LayerShader::Maxpool2d: return Maxpool2dF16Comp; break;
		case LayerShader::Pad: return PadF16Comp; break;
		case LayerShader::Resize: return ResizeF16Comp; break;
		case LayerShader::Subpixel: return SubpixelF16Comp; break;
		case LayerShader::Unary: return UnaryF16Comp; break;
		case LayerShader::Upsampling2dBilinear: return Upsampling2dBilinearF16Comp; break;
		case LayerShader::Upsampling2dNearest: return Upsampling2dNearestF16Comp; break;
		case LayerShader::AddVectorToMatrixRows: return AddVectorToMatrixRowsComp; break;
		case LayerShader::BufferNorm: return BufferNormComp; break;
		case LayerShader::MultiplyMatrixByMatrix: return MultiplyMatrixByMatrixComp; break;
		case LayerShader::MultiplyMatrixByMatrixBorder: return MultiplyMatrixByMatrixBordersComp; break;
		case LayerShader::MultiplyMatrixByTransposedMatrix: return MultiplyMatrixByTransposedMatrixComp; break;
		case LayerShader::MultiplyMatrixByTransposedMatrixBorder: return MultiplyMatrixByTransposedMatrixBordersComp; break;
		case LayerShader::MultiplyTransposedMatrixByMatrix: return MultiplyTransposedMatrixByMatrixComp; break;
		case LayerShader::MultiplyTransposedMatrixByMatrixBorder: return MultiplyTransposedMatrixByMatrixBordersComp; break;
		case LayerShader::MatrixSoftmaxByRows: return MatrixSoftmaxByRowsComp; break;
		case LayerShader::VectorAddFloat1: return VectorAddFloat1Comp; break;
		case LayerShader::VectorAddFloat4: return VectorAddFloat4Comp; break;
		case LayerShader::VectorReLU: return VectorReLUComp; break;
		case LayerShader::VectorReLU4: return VectorReLU4Comp; break;
		case LayerShader::VectorReLUDiff: return VectorReLUDiffComp; break;
		case LayerShader::VectorLog: return VectorLogComp; break;
		case LayerShader::VectorDotProduct: return VectorDotProductComp; break;
		case LayerShader::VectorEltwiseMultiply: return VectorMultiplyFloatComp; break;
		case LayerShader::VectorMultiplyAndAdd: return VectorMultiplyAndAddComp; break;
		case LayerShader::VectorSub: return VectorSubComp; break;
		case LayerShader::SumMatrixColumns: return SumMatrixColumnsComp; break;
		case LayerShader::SumMatrixRows: return SumMatrixRowsComp; break;
		case LayerShader::MultiplyDiagMatrixByMatrix: return MultiplyDiagMatrixByMatrixComp; break;
		case LayerShader::StatNorm: return StatNormComp; break;
		case LayerShader::StatClassMap: return StatClassMapComp; break;
		case LayerShader::StatClassPercent: return StatClassPercentComp; break;
		case LayerShader::StatAnalysis: return StatAnalysisComp; break;
		}
		break;
	case Precision::F32:
		switch (sh) {
		case LayerShader::Gen: return GenF32Comp; break;
		case LayerShader::Norm: return NormF32Comp; break;
		case LayerShader::Activation: return ActivationF32Comp; break;
		case LayerShader::Add: return AddF32Comp; break;
		case LayerShader::Avgpool2d: return Avgpool2dF32Comp; break;
		case LayerShader::Batchnorm: return BatchnormF32Comp; break;
		case LayerShader::Concat: return ConcatF32Comp; break;
		case LayerShader::Conv2d1x1: return Conv2d1x1F32Comp; break;
		case LayerShader::Conv2d: return Conv2dF32Comp; break;
		case LayerShader::Dense: return DenseF32Comp; break;
		case LayerShader::Depthwise: return DepthwiseF32Comp; break;
		case LayerShader::Flatten: return FlattenF32Comp; break;
		case LayerShader::Instancenorm: return InstancenormF32Comp; break;
		case LayerShader::Maxpool2d: return Maxpool2dF32Comp; break;
		case LayerShader::Pad: return PadF32Comp; break;
		case LayerShader::Resize: return ResizeF32Comp; break;
		case LayerShader::Subpixel: return SubpixelF32Comp; break;
		case LayerShader::Unary: return UnaryF32Comp; break;
		case LayerShader::Upsampling2dBilinear: return Upsampling2dBilinearF32Comp; break;
		case LayerShader::Upsampling2dNearest: return Upsampling2dNearestF32Comp; break;
		case LayerShader::AddVectorToMatrixRows: return AddVectorToMatrixRowsComp; break;
		case LayerShader::BufferNorm: return BufferNormComp; break;
		case LayerShader::MultiplyMatrixByMatrix: return MultiplyMatrixByMatrixComp; break;
		case LayerShader::MultiplyMatrixByMatrixBorder: return MultiplyMatrixByMatrixBordersComp; break;
		case LayerShader::MultiplyMatrixByTransposedMatrix: return MultiplyMatrixByTransposedMatrixComp; break;
		case LayerShader::MultiplyMatrixByTransposedMatrixBorder: return MultiplyMatrixByTransposedMatrixBordersComp; break;
		case LayerShader::MultiplyTransposedMatrixByMatrix: return MultiplyTransposedMatrixByMatrixComp; break;
		case LayerShader::MultiplyTransposedMatrixByMatrixBorder: return MultiplyTransposedMatrixByMatrixBordersComp; break;
		case LayerShader::MatrixSoftmaxByRows: return MatrixSoftmaxByRowsComp; break;
		case LayerShader::VectorAddFloat1: return VectorAddFloat1Comp; break;
		case LayerShader::VectorAddFloat4: return VectorAddFloat4Comp; break;
		case LayerShader::VectorReLU: return VectorReLUComp; break;
		case LayerShader::VectorReLU4: return VectorReLU4Comp; break;
		case LayerShader::VectorReLUDiff: return VectorReLUDiffComp; break;
		case LayerShader::VectorLog: return VectorLogComp; break;
		case LayerShader::VectorDotProduct: return VectorDotProductComp; break;
		case LayerShader::VectorEltwiseMultiply: return VectorMultiplyFloatComp; break;
		case LayerShader::VectorMultiplyAndAdd: return VectorMultiplyAndAddComp; break;
		case LayerShader::VectorSub: return VectorSubComp; break;
		case LayerShader::SumMatrixColumns: return SumMatrixColumnsComp; break;
		case LayerShader::SumMatrixRows: return SumMatrixRowsComp; break;
		case LayerShader::MultiplyDiagMatrixByMatrix: return MultiplyDiagMatrixByMatrixComp; break;
		case LayerShader::StatNorm: return StatNormComp; break;
		case LayerShader::StatClassMap: return StatClassMapComp; break;
		case LayerShader::StatClassPercent: return StatClassPercentComp; break;
		case LayerShader::StatAnalysis: return StatAnalysisComp; break;
		}
		break;
	default:
		break;
	}

	switch (sh) {
	case LayerShader::AddVectorToMatrixRows: return AddVectorToMatrixRowsComp; break;
	case LayerShader::BufferNorm: return BufferNormComp; break;
	case LayerShader::MultiplyMatrixByMatrix: return MultiplyMatrixByMatrixComp; break;
	case LayerShader::MultiplyMatrixByMatrixBorder: return MultiplyMatrixByMatrixBordersComp; break;
	case LayerShader::MultiplyMatrixByTransposedMatrix: return MultiplyMatrixByTransposedMatrixComp; break;
	case LayerShader::MultiplyMatrixByTransposedMatrixBorder: return MultiplyMatrixByTransposedMatrixBordersComp; break;
	case LayerShader::MultiplyTransposedMatrixByMatrix: return MultiplyTransposedMatrixByMatrixComp; break;
	case LayerShader::MultiplyTransposedMatrixByMatrixBorder: return MultiplyTransposedMatrixByMatrixBordersComp; break;
	case LayerShader::MatrixSoftmaxByRows: return MatrixSoftmaxByRowsComp; break;
	case LayerShader::VectorAddFloat1: return VectorAddFloat1Comp; break;
	case LayerShader::VectorAddFloat4: return VectorAddFloat4Comp; break;
	case LayerShader::VectorReLU: return VectorReLUComp; break;
	case LayerShader::VectorReLU4: return VectorReLU4Comp; break;
	case LayerShader::VectorReLUDiff: return VectorReLUDiffComp; break;
	case LayerShader::VectorLog: return VectorLogComp; break;
	case LayerShader::VectorDotProduct: return VectorDotProductComp; break;
	case LayerShader::VectorEltwiseMultiply: return VectorMultiplyFloatComp; break;
	case LayerShader::VectorMultiplyAndAdd: return VectorMultiplyAndAddComp; break;
	case LayerShader::VectorSub: return VectorSubComp; break;
	case LayerShader::SumMatrixColumns: return SumMatrixColumnsComp; break;
	case LayerShader::SumMatrixRows: return SumMatrixRowsComp; break;
	case LayerShader::MultiplyDiagMatrixByMatrix: return MultiplyDiagMatrixByMatrixComp; break;

	case LayerShader::StatNorm: return StatNormComp; break;
	case LayerShader::StatClassMap: return StatClassMapComp; break;
	case LayerShader::StatClassPercent: return StatClassPercentComp; break;
	case LayerShader::StatAnalysis: return StatAnalysisComp; break;
	default: break;
	}

	return SpanView<uint32_t>();
}

void FillFloatBuffer(uint8_t *buf, uint64_t size, float val) {
	auto target = (float *)buf;
	size /= sizeof(float);
	while (size > 0) {
		*(target ++) = val;
		-- size;
	}
}

}

#include "XLSnnVkActivationLayer.cc"
#include "XLSnnVkGenerationLayer.cc"
#include "XLSnnVkInputLayer.cc"
#include "XLSnnVkConvLayer.cc"
#include "XLSnnVkSubpixelLayer.cc"
#include "XLSnnVkMatrixMulLayer.cc"
#include "XLSnnVkStatPercentLayer.cc"
#include "XLSnnVkLossLayer.cc"
#include "XLSnnVkTrainableLayer.cc"

#include "XLSnnVkNmeMath.cc"

#endif
