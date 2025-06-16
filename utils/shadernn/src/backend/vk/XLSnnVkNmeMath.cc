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

#include "XLSnnVkNmeMath.h"

namespace stappler::xenolith::vk::shadernn {

struct AddVectorToMatrixRowsData {
	int batchSize;
	int matrixHeight;
	int matrixWidth;
};

struct MultiplyMatrixByMatrixData {
	int batchSize;
	int firstHeight;
	int firstWidth;
	int firstRowSize;
	int secondWidth;
	int secondRowSize;
	int resultRowSize;
	int toAdd;
};

struct BatchMultiplyMatrixByMatrixBordersData {
	int batchSize;
	int firstHeight;
	int firstWidth;
	int firstRowSize;
	int secondWidth;
	int secondRowSize;
	int resultRowSize;
	int leftOffset;
	int topOffset;
	int toAdd;
};

struct MultiplyMatrixByTransposedMatrixData {
	int batchSize;
	int firstHeight;
	int firstWidth;
	int firstRowSize;
	int secondHeight;
	int secondRowSize;
	int resultRowSize;
	int toAdd;
};

struct MultiplyMatrixByTransposedMatrixBordersData {
	int batchSize;
	int firstHeight;
	int firstWidth;
	int firstRowSize;
	int secondHeight;
	int secondRowSize;
	int resultRowSize;
	int leftOffset;
	int topOffset;
	int toAdd;
};

struct BatchMultiplyTransposedMatrixByMatrixData {
	int batchSize;
	int firstHeight;
	int firstWidth;
	int firstRowSize;
	int secondWidth;
	int secondRowSize;
	int resultRowSize;
	int toAdd;
};

struct BatchMultiplyTransposedMatrixByMatrixBordersData {
	int batchSize;
	int firstHeight;
	int firstWidth;
	int firstRowSize;
	int secondWidth;
	int secondRowSize;
	int resultRowSize;
	int leftOffset;
	int topOffset;
	int toAdd;
};

struct MatrixSoftmaxByRowsData {
	int matrixHeight;
	int matrixWidth;
};

struct MultiplyDiagMatrixByMatrixData {
	int height;
	int width;
};

struct SumMatrixColumnsData {
	int width;
	int height;
};

struct SumMatrixRowsData {
	int width;
	int height;
	int batchSize;

	int toAdd;
};

struct VectorLogData {
	int neg;
};

struct VectorMultiplyFloatData {
	int isSecondValue;
	int isNeg;
	int toAdd;
};

struct VectorDotData {
	int targetOffset;
	int hasMult;
	int multOffset;
};

inline int Ceil(int val, int discret) {
	if (val > 0) {
		return (val + discret - 1) / discret;
	}
	return val / discret;
}

// The maximum number of groups over the X dimension when working with a 1D (vector) shader
// With larger sizes, the shader data will be represented in two dimensions
constexpr int VulkanMaxVectorXGroupCount = 8'192;

// The number of combined operations
constexpr int VectorCombine = 4;

static void runVectorShader(CommandBuffer &buf, const core::ComputePipelineData *pipeline,
		BytesView pcb, int count) {
	int groupCountX = Ceil(count, pipeline->pipeline->getLocalX());
	int groupCountY = Ceil(groupCountX, VulkanMaxVectorXGroupCount);
	groupCountX = std::min<int>(groupCountX, VulkanMaxVectorXGroupCount);

	if (!pcb.empty()) {
		buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0, pcb);
	}

	buf.cmdBindPipeline(static_cast<ComputePipeline *>(pipeline->pipeline.get()));
	buf.cmdDispatch(groupCountX, groupCountY, 1);
}

static void BatchMultiplyMatrixByTransposedMatrix(CommandBuffer &buf,
		const core::ComputePipelineData *mul, const core::ComputePipelineData *borders, bool toAdd,
		int batchSize, int firstHeight, int firstWidth, int firstRowSize, int secondHeight,
		int secondRowSize, int resultRowSize, int resultBufferSize) {

	if (firstHeight >= 4 && secondHeight >= 4) {
		MultiplyMatrixByTransposedMatrixData param = {batchSize, firstHeight, firstWidth,
			firstRowSize, secondHeight, secondRowSize, resultRowSize, (toAdd) ? 1 : 0};

		buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0,
				BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)));
		buf.cmdDispatchPipeline(mul, firstHeight / 4, secondHeight / 4, batchSize);
	}

	int leftOffset = secondHeight - secondHeight % 4;
	int topOffset = firstHeight - firstHeight % 4;
	int count = secondHeight * firstHeight - leftOffset * topOffset;
	if (count > 0) {
		MultiplyMatrixByTransposedMatrixBordersData param = {batchSize, firstHeight, firstWidth,
			firstRowSize, secondHeight, secondRowSize, resultRowSize, leftOffset, topOffset, toAdd};
		buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0,
				BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)));
		buf.cmdDispatchPipeline(borders, count, batchSize, 1);
	}
}

static void batchMultiplyTransposedMatrixByMatrix(CommandBuffer &buf,
		const core::ComputePipelineData *mul, const core::ComputePipelineData *borders, bool toAdd,
		int batchSize, int firstHeight, int firstWidth, int firstRowSize, int secondWidth,
		int secondRowSize, int resultRowSize, int resultBufferSize) {
	if (firstWidth >= 4 && secondWidth >= 4) {
		BatchMultiplyTransposedMatrixByMatrixData param = {batchSize, firstHeight, firstWidth,
			firstRowSize, secondWidth, secondRowSize, resultRowSize, toAdd ? 1 : 0};

		buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0,
				BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)));
		buf.cmdDispatchPipeline(mul, secondWidth / 4, firstWidth / 4, batchSize);
	}

	int leftOffset = secondWidth - secondWidth % 4;
	int topOffset = firstWidth - firstWidth % 4;
	int count = secondWidth * firstWidth - leftOffset * topOffset;
	if (count > 0) {
		BatchMultiplyTransposedMatrixByMatrixBordersData param = {batchSize, firstHeight,
			firstWidth, firstRowSize, secondWidth, secondRowSize, resultRowSize, leftOffset,
			topOffset, toAdd ? 1 : 0};

		buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0,
				BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)));
		buf.cmdDispatchPipeline(borders, count, batchSize, 1);
	}
}

static void multiplyMatrixByMatrix(CommandBuffer &buf, const core::ComputePipelineData *mul,
		const core::ComputePipelineData *borders, bool toAdd, int batchSize, int firstHeight,
		int firstWidth, int firstRowSize, int secondWidth, int secondRowSize, int resultRowSize,
		int resultBufferSize) {

	if (firstHeight >= 4 && secondWidth >= 4) {
		MultiplyMatrixByMatrixData param = {batchSize, firstHeight, firstWidth, firstRowSize,
			secondWidth, secondRowSize, resultRowSize, toAdd ? 1 : 0};

		buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0,
				BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)));
		buf.cmdDispatchPipeline(mul, secondWidth / 4, firstHeight / 4, batchSize);
	}

	int leftOffset = secondWidth - secondWidth % 4;
	int topOffset = firstHeight - firstHeight % 4;
	int count = secondWidth * firstHeight - leftOffset * topOffset;
	if (count > 0) {
		BatchMultiplyMatrixByMatrixBordersData param = {batchSize, firstHeight, firstWidth,
			firstRowSize, secondWidth, secondRowSize, resultRowSize, leftOffset, topOffset,
			toAdd ? 1 : 0};

		buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0,
				BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)));
		buf.cmdDispatchPipeline(borders, count, batchSize, 1);
	}
}

void MultiplyMatrixByMatrix(CommandBuffer &buf, const core::ComputePipelineData *mul,
		const core::ComputePipelineData *borders, int batchSize, int firstHeight, int firstWidth,
		int secondWidth, int resultBufferSize) {
	multiplyMatrixByMatrix(buf, mul, borders, false, batchSize, firstHeight, firstWidth, firstWidth,
			secondWidth, secondWidth, secondWidth, resultBufferSize);
}

void MultiplyMatrixByTransposedMatrix(CommandBuffer &buf, const core::ComputePipelineData *mul,
		const core::ComputePipelineData *borders, int firstHeight, int firstWidth, int firstRowSize,
		int secondHeight, int secondRowSize, int resultRowSize, int resultBufferSize) {

	BatchMultiplyMatrixByTransposedMatrix(buf, mul, borders, false, 1, firstHeight, firstWidth,
			firstRowSize, secondHeight, secondRowSize, resultRowSize, resultBufferSize);
}

void MultiplyMatrixByTransposedMatrix(CommandBuffer &buf, const core::ComputePipelineData *mul,
		const core::ComputePipelineData *borders, int batchSize, int firstHeight, int firstWidth,
		int secondHeight, int resultBufferSize) {
	BatchMultiplyMatrixByTransposedMatrix(buf, mul, borders, false, batchSize, firstHeight,
			firstWidth, firstWidth, secondHeight, firstWidth, secondHeight, resultBufferSize);
}

void MultiplyTransposedMatrixByMatrixAndAdd(CommandBuffer &buf,
		const core::ComputePipelineData *mul, const core::ComputePipelineData *borders,
		int firstHeight, int firstWidth, int firstRowSize, int secondWidth, int secondRowSize,
		int resultRowSize, int resultBufferSize) {
	batchMultiplyTransposedMatrixByMatrix(buf, mul, borders, true, 1, firstHeight, firstWidth,
			firstRowSize, secondWidth, secondRowSize, resultRowSize, resultBufferSize);
}

void MultiplyTransposedMatrixByMatrix(CommandBuffer &buf, const core::ComputePipelineData *mul,
		const core::ComputePipelineData *borders, int firstHeight, int firstWidth, int firstRowSize,
		int secondWidth, int secondRowSize, int resultRowSize, int resultBufferSize) {
	batchMultiplyTransposedMatrixByMatrix(buf, mul, borders, false, 1, firstHeight, firstWidth,
			firstRowSize, secondWidth, secondRowSize, resultRowSize, resultBufferSize);
}

void AddVectorToMatrixRows(CommandBuffer &buf, const core::ComputePipelineData *p, int batchSize,
		int matrixHeight, int matrixWidth) {
	AddVectorToMatrixRowsData param = {batchSize, matrixHeight, matrixWidth};

	buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0,
			BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)));
	buf.cmdDispatchPipeline(p, matrixWidth, Ceil(matrixHeight, 4), batchSize);
}

void VectorAdd(CommandBuffer &buf, const core::ComputePipelineData *add4,
		const core::ComputePipelineData *add1, int vectorSize) {
	int countQuad = (vectorSize / 16) * 4;
	if (countQuad > 0) {
		runVectorShader(buf, add4, BytesView(), countQuad);
	}

	int countSingle = vectorSize % 16;
	if (countSingle > 0) {
		int offset = vectorSize - countSingle;

		struct {
			int offset;
		} param = {offset};

		runVectorShader(buf, add1, BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)),
				countSingle);
	}
}

void VectorReLU(CommandBuffer &buf, const core::ComputePipelineData *relu4,
		const core::ComputePipelineData *relu, int vectorSize, float threshold) {
	int countQuad = (vectorSize / 16) * 4;
	if (countQuad > 0) {
		struct {
			float value;
		} param = {threshold};

		runVectorShader(buf, relu4, BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)),
				countQuad);
	}

	int countSingle = vectorSize % 16;
	if (countSingle > 0) {
		int offset = vectorSize - countSingle;

		struct {
			float value;
			int offset;
		} param = {threshold, offset};

		runVectorShader(buf, relu, BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)),
				countSingle);
	}
}

void VectorReLUDiff(CommandBuffer &buf, const core::ComputePipelineData *relu, int vectorSize,
		float threshold) {
	struct {
		float value;
	} param = {threshold};

	runVectorShader(buf, relu, BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)),
			Ceil(vectorSize, VectorCombine));
}

void MatrixSoftmaxByRows(CommandBuffer &buf, const core::ComputePipelineData *p, int height,
		int width) {
	MatrixSoftmaxByRowsData param = {height, width};

	buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0,
			BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)));
	buf.cmdDispatchPipeline(p, width, height, 1);
}

void VectorNegLog(CommandBuffer &buf, const core::ComputePipelineData *p, int vectorSize) {
	VectorLogData param = {1};

	runVectorShader(buf, p, BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)),
			Ceil(vectorSize, VectorCombine));
}

void VectorEltwiseMultiply(CommandBuffer &buf, const core::ComputePipelineData *p, int vectorSize) {
	VectorMultiplyFloatData param = {0, 0, 0};

	runVectorShader(buf, p, BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)),
			Ceil(vectorSize, VectorCombine));
}

void VectorMultiply(CommandBuffer &buf, const core::ComputePipelineData *p, int vectorSize) {
	VectorMultiplyFloatData param = {1, 0, 0};

	runVectorShader(buf, p, BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)),
			Ceil(vectorSize, VectorCombine));
}

void VectorMultiplyAndAdd(CommandBuffer &buf, const core::ComputePipelineData *p, int vectorSize) {
	runVectorShader(buf, p, BytesView(), Ceil(vectorSize, VectorCombine));
}

void VectorSub(CommandBuffer &buf, const core::ComputePipelineData *p, int vectorSize) {
	runVectorShader(buf, p, BytesView(), Ceil(vectorSize, VectorCombine));
}

void SumMatrixColumns(CommandBuffer &buf, const core::ComputePipelineData *p, int matrixHeight,
		int matrixWidth) {
	SumMatrixColumnsData param = {matrixWidth, matrixHeight};

	buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0,
			BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)));
	buf.cmdDispatchPipeline(p, matrixHeight, 1, 1);
}

void SumMatrixRowsAdd(CommandBuffer &buf, const core::ComputePipelineData *p, int batchSize,
		int matrixHeight, int matrixWidth) {
	struct SumMatrixRowsData param = {matrixWidth, matrixHeight, batchSize, 1};
	buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0,
			BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)));
	buf.cmdDispatchPipeline(p, matrixWidth, 1, batchSize);
}

void SumMatrixRows(CommandBuffer &buf, const core::ComputePipelineData *p, int batchSize,
		int matrixHeight, int matrixWidth) {
	struct SumMatrixRowsData param = {matrixWidth, matrixHeight, batchSize, 0};
	buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0,
			BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)));
	buf.cmdDispatchPipeline(p, matrixWidth, 1, batchSize);
}

void MultiplyDiagMatrixByMatrix(CommandBuffer &buf, const core::ComputePipelineData *p,
		int firstSize, int secondWidth, int resultBufferSize) {
	MultiplyDiagMatrixByMatrixData param = {firstSize, secondWidth};

	buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0,
			BytesView(reinterpret_cast<uint8_t *>(&param), sizeof(param)));
	buf.cmdDispatchPipeline(p, Ceil(firstSize, 4), secondWidth, 1);
}

void VectorDotProduct(CommandBuffer &buf, const core::ComputePipelineData *p, int vectorSize) {
	runVectorShader(buf, p, BytesView(),
			p->pipeline->getLocalX() * p->pipeline->getLocalY() * p->pipeline->getLocalZ());
}

} // namespace stappler::xenolith::vk::shadernn
