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

#ifndef SRC_BACKEND_VK_XLSNNVKSHADERS_H_
#define SRC_BACKEND_VK_XLSNNVKSHADERS_H_

#include "XLSnnModel.h"
#include "XLVkAttachment.h"

namespace stappler::xenolith::vk::shadernn {

using Activation = xenolith::shadernn::Activation;

using xenolith::shadernn::ROUND_UP;
using xenolith::shadernn::UP_DIV;

enum class Precision {
	Unknown,
	F8,
	F16,
	F32,
	F64
};

enum class LayerShader {
	Gen,
	Norm,
	Activation,
	Add,
	Avgpool2d,
	Batchnorm,
	Concat,
	Conv2d1x1,
	Conv2d,
	Dense,
	Depthwise,
	Flatten,
	Instancenorm,
	Maxpool2d,
	Pad,
	Resize,
	Subpixel,
	Unary,
	Upsampling2dBilinear,
	Upsampling2dNearest,

	BufferNorm,
	MultiplyMatrixByMatrix,
	MultiplyMatrixByMatrixBorder,
	MultiplyMatrixByTransposedMatrix,
	MultiplyMatrixByTransposedMatrixBorder,
	MultiplyTransposedMatrixByMatrix,
	MultiplyTransposedMatrixByMatrixBorder,
	AddVectorToMatrixRows,
	MatrixSoftmaxByRows,
	VectorAddFloat1,
	VectorAddFloat4,
	VectorReLU,
	VectorReLU4,
	VectorReLUDiff,
	VectorLog,
	VectorDotProduct,
	VectorEltwiseMultiply,
	VectorMultiplyAndAdd,
	VectorSub,
	SumMatrixColumns,
	SumMatrixRows,
	MultiplyDiagMatrixByMatrix,

	StatNorm,
	StatClassMap,
	StatClassPercent,
	StatAnalysis,
};

Precision getAttachmentPrecision(const core::AttachmentData *);

SpanView<uint32_t> getShader(LayerShader, Precision);

void FillFloatBuffer(uint8_t *buf, uint64_t size, float val);

}

#endif /* SRC_BACKEND_VK_XLSNNVKSHADERS_H_ */
