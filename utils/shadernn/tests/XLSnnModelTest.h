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

#ifndef TESTS_XLSNNMODELTEST_H_
#define TESTS_XLSNNMODELTEST_H_

#include "XLCoreQueue.h"
#include "XLApplication.h"
#include "XLSnnModel.h"
#include "XLSnnModelProcessor.h"

namespace stappler::xenolith::shadernn {

struct MnistTrainData : Ref {
	struct ImagesHeader {
		uint32_t magic;
		uint32_t images;
		uint32_t rows;
		uint32_t columns;
	};

	struct VectorsHeader {
		uint32_t magic;
		uint32_t items;
	};

	MnistTrainData(StringView path);
	~MnistTrainData();

	void loadVectors(StringView ipath);
	void loadImages(StringView ipath);
	void loadIndexes();

	void readImages(uint8_t *ptr, uint64_t size, size_t offset);
	void readLabels(uint8_t *iptr, uint64_t size, size_t offset);

	bool validateImages(const uint8_t *ptr, uint64_t size, size_t offset);

	void shuffle(Random &rnd);

	ImagesHeader imagesHeader;
	VectorsHeader vectorsHeader;

	float *imagesData = 0;
	float *vectorsData = 0;

	size_t imagesSize = 0;
	size_t vectorsSize = 0;

	std::vector<uint32_t> indexes;
};

struct CsvData : Ref {
	Vector<String> fields;
	Vector<Value> data;
};

class ModelQueue : public core::Queue {
public:
	static Rc<CsvData> readCsv(StringView);

	virtual ~ModelQueue();

	bool init(StringView model, ModelFlags, StringView input);

	const Vector<const AttachmentData *> &getInputAttachments() const { return _inputAttachments; }
	const AttachmentData *getOutputAttachment() const { return _outputAttachment; }

	void run(Application *);

protected:
	using core::Queue::init;

	void onComplete(Application *, BytesView data);

	Application *_app = nullptr;
	String _image;
	Vector<const AttachmentData *> _inputAttachments;
	const AttachmentData *_outputAttachment = nullptr;

	Rc<MnistTrainData> _trainData;
	Rc<CsvData> _csvData;

	Rc<Model> _model;
	Rc<ModelProcessor> _processor;

	size_t _endEpoch = 2;
	size_t _epoch = 0;
	size_t _loadOffset = 0;
	float _epochLoss = 0.0f;
};

}

#endif /* TESTS_XLSNNMODELTEST_H_ */
