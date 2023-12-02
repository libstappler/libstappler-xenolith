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

#ifndef SRC_PROCESSOR_XLSNNMODEL_H_
#define SRC_PROCESSOR_XLSNNMODEL_H_

#include "XLCommon.h"

namespace stappler::xenolith::shadernn {

template <typename T>
static constexpr auto ROUND_UP(T x, T y) -> T { return (((x) + (y) - (1)) / (y) * (y)); }

template <typename T>
static constexpr auto UP_DIV(T x, T y) -> T { return ((x) + (y) - (1)) / (y); }

enum class Activation : uint32_t {
	None,
	RELU,
	RELU6,
	TANH,
	SIGMOID,
	LEAKYRELU,
	SILU
};

enum class ModelFlags {
	None = 0,
	HalfPrecision = 1 << 0,
	Range01 = 1 << 1,
};

SP_DEFINE_ENUM_AS_MASK(ModelFlags)

class Layer;
class Attachment;

class Model : public Ref {
public:
	virtual ~Model();

	virtual bool init(ModelFlags, const Value &val, uint32_t numLayers, StringView dataFilePath);

	virtual void addLayer(Rc<Layer> &&);

	virtual bool link();

	bool isHalfPrecision() const { return (_flags & ModelFlags::HalfPrecision) != ModelFlags::None; }
	bool usesDataFile() const { return _dataFile; }

	float readFloatData();

	const Vector<Layer *> &getSortedLayers() const { return _sortedLayers; }

	Vector<Layer *> getInputs() const;

protected:
	void linkInput(Vector<Layer *> &, Layer *, Attachment *);

	ModelFlags _flags;
	uint32_t _numLayers;

	int32_t _inputWidth = -1;
	int32_t _inputHeight = -1;
	int32_t _inputChannels = -1;
	int32_t _upscale = -1;
	bool _useSubPixel = false;

	filesystem::File _dataFile;
	Map<uint32_t, Rc<Layer>> _layers;
	Vector<Layer *> _sortedLayers;
	Vector<Rc<Attachment>> _attachments;
};

Activation getActivationValue(StringView);

float convertToMediumPrecision(float in);
float convertToHighPrecision(uint16_t in);
void convertToMediumPrecision(Vector<float> &in);
void convertToMediumPrecision(Vector<double> &in);
void getByteRepresentation(float in, Vector<unsigned char> &byteRep, bool fp16);

}

#endif /* SRC_PROCESSOR_XLSNNMODEL_H_ */
