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

#ifndef SRC_BACKEND_VK_XLSNNVKNMEMATH_H_
#define SRC_BACKEND_VK_XLSNNVKNMEMATH_H_

#include "XLSnnVkShaders.h"
#include "XLVkDeviceQueue.h"

namespace stappler::xenolith::vk::shadernn {

void AddVectorToMatrixRows(CommandBuffer &buf, const core::ComputePipelineData *p, int batchSize,
		int matrixHeight, int matrixWidth);

void MultiplyMatrixByMatrix(CommandBuffer &buf, const core::ComputePipelineData *mul,
		const core::ComputePipelineData *borders, int batchSize, int firstHeight, int firstWidth,
		int secondWidth, int resultBufferSize);

void MultiplyMatrixByTransposedMatrix(CommandBuffer &buf, const core::ComputePipelineData *mul,
		const core::ComputePipelineData *borders, int firstHeight, int firstWidth, int firstRowSize,
		int secondHeight, int secondRowSize, int resultRowSize, int resultBufferSize);

void MultiplyMatrixByTransposedMatrix(CommandBuffer &buf, const core::ComputePipelineData *mul,
		const core::ComputePipelineData *borders, int batchSize, int firstHeight, int firstWidth,
		int secondHeight, int resultBufferSize);

void MultiplyTransposedMatrixByMatrix(CommandBuffer &buf, const core::ComputePipelineData *mul,
		const core::ComputePipelineData *borders, int firstHeight, int firstWidth, int firstRowSize,
		int secondWidth, int secondRowSize, int resultRowSize, int resultBufferSize);

void MultiplyTransposedMatrixByMatrixAndAdd(CommandBuffer &buf,
		const core::ComputePipelineData *mul, const core::ComputePipelineData *borders,
		int firstHeight, int firstWidth, int firstRowSize, int secondWidth, int secondRowSize,
		int resultRowSize, int resultBufferSize);

void VectorAdd(CommandBuffer &buf, const core::ComputePipelineData *add4,
		const core::ComputePipelineData *add1, int vectorSize);

void VectorReLU(CommandBuffer &buf, const core::ComputePipelineData *relu4,
		const core::ComputePipelineData *relu, int vectorSize, float threshold);

void VectorReLUDiff(CommandBuffer &buf, const core::ComputePipelineData *relu, int vectorSize,
		float threshold);

void MatrixSoftmaxByRows(CommandBuffer &buf, const core::ComputePipelineData *p, int height,
		int width);

void VectorNegLog(CommandBuffer &buf, const core::ComputePipelineData *p, int vectorSize);

void VectorEltwiseMultiply(CommandBuffer &buf, const core::ComputePipelineData *p, int vectorSize);

void VectorMultiply(CommandBuffer &buf, const core::ComputePipelineData *p, int vectorSize);

void VectorMultiplyAndAdd(CommandBuffer &buf, const core::ComputePipelineData *p, int vectorSize);

void VectorSub(CommandBuffer &buf, const core::ComputePipelineData *p, int vectorSize);

void SumMatrixColumns(CommandBuffer &buf, const core::ComputePipelineData *p, int matrixHeight,
		int matrixWidth);

void SumMatrixRowsAdd(CommandBuffer &buf, const core::ComputePipelineData *p, int batchSize,
		int matrixHeight, int matrixWidth);

void SumMatrixRows(CommandBuffer &buf, const core::ComputePipelineData *p, int batchSize,
		int matrixHeight, int matrixWidth);

void MultiplyDiagMatrixByMatrix(CommandBuffer &buf, const core::ComputePipelineData *p,
		int firstSize, int secondWidth, int resultBufferSize);

void VectorDotProduct(CommandBuffer &buf, const core::ComputePipelineData *p, int vectorSize);

} // namespace stappler::xenolith::vk::shadernn

#endif /* SRC_BACKEND_VK_XLSNNVKNMEMATH_H_ */
