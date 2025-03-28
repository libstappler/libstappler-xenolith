/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "XLSnnModelTest.h"
#include "XLSnnModel.h"
#include "XLSnnLayer.h"
#include "XLSnnAttachment.h"
#include "XLSnnVkInputLayer.h"
#include "XLVkAttachment.h"
#include "XLCoreFrameRequest.h"
#include "SPFilesystem.h"
#include "SPBitmap.h"
#include "SPValid.h"

namespace stappler::xenolith::shadernn {

void MnistTrainData::loadVectors(StringView ipath) {
	auto path = filepath::merge<Interface>(ipath, "train-labels.idx1-ubyte");
	auto vectors = ::fopen(path.data(), "r");
	if (vectors) {
		::fseek(vectors, 0, SEEK_END);
		auto fsize = ::ftell(vectors);
		::fseek(vectors, 0, SEEK_SET);

		if (::fread(&vectorsHeader, sizeof(VectorsHeader), 1, vectors) == 0) {
			log::error("shadernn::MnistTrainData", "Fail to read model file");
		}

		vectorsHeader.magic = byteorder::bswap32(vectorsHeader.magic);
		vectorsHeader.items = byteorder::bswap32(vectorsHeader.items);

		auto dataSize = fsize - sizeof(ImagesHeader);

		uint8_t *buf = (uint8_t *)::malloc(dataSize);
		if (::fread(buf, dataSize, 1, vectors) == 0) {
			log::error("shadernn::MnistTrainData", "Fail to read model file");
		}

		vectorsData = (float *)malloc(dataSize * sizeof(float) * 10);
		::memset(vectorsData, 0, dataSize * sizeof(float));
		auto ptr = vectorsData;
		for (size_t i = 0; i < dataSize; ++ i) {
			for (size_t j = 0; j < 10; ++ j) {
				ptr[j] = 0.0f;
			}
			ptr[buf[i]] = 1.0f;
			ptr += 10;
		}

		::free(buf);
		::fclose(vectors);
	}
}

void MnistTrainData::loadImages(StringView ipath) {
	auto path = filepath::merge<Interface>(ipath, "train-images.idx3-ubyte");
	auto images = ::fopen(path.data(), "r");
	if (images) {
		::fseek(images, 0, SEEK_END);
		auto fsize = ::ftell(images);
		::fseek(images, 0, SEEK_SET);

		if (::fread(&vectorsHeader, sizeof(ImagesHeader), 1, images) == 0) {
			log::error("shadernn::MnistTrainData", "Fail to read model file");
		}

		imagesHeader.magic = byteorder::bswap32(imagesHeader.magic);
		imagesHeader.images = byteorder::bswap32(imagesHeader.images);
		imagesHeader.rows = byteorder::bswap32(imagesHeader.rows);
		imagesHeader.columns = byteorder::bswap32(imagesHeader.columns);

		auto dataSize = fsize - sizeof(ImagesHeader);

		uint8_t *buf = (uint8_t *)::malloc(dataSize);

		if (::fread(buf, dataSize, 1, images) == 0) {
			log::error("shadernn::MnistTrainData", "Fail to read model file");
		}

		imagesData = (float *)malloc(dataSize * sizeof(float));
		auto ptr = imagesData;
		for (size_t i = 0; i < dataSize; ++ i) {
			*ptr ++ = float(buf[i]) / 255.0f;
		}

		::free(buf);
		::fclose(images);
	}
}

void MnistTrainData::loadIndexes() {
	indexes.resize(imagesHeader.images);
	for (size_t i = 0; i < imagesHeader.images; ++ i) {
		indexes[i] = i;
	}
}

void MnistTrainData::readImages(uint8_t *iptr, uint64_t size, size_t offset) {
	auto count = size / (imagesHeader.rows * imagesHeader.columns * sizeof(float));

	float *ptr = (float *)iptr;

	for (size_t i = 0; i < count; ++ i) {
    	auto idx = indexes[offset + i];
    	auto blockSize = imagesHeader.rows * imagesHeader.columns;
    	::memcpy(ptr + i * blockSize, imagesData + idx * blockSize, blockSize * sizeof(float));
	}
}

void MnistTrainData::readLabels(uint8_t *iptr, uint64_t size, size_t offset) {
	auto count = size / (10 * sizeof(float));

	float *ptr = (float *)iptr;

	for (size_t i = 0; i < count; ++ i) {
    	auto idx = indexes[offset + i];
    	auto blockSize = 10;
    	::memcpy(ptr + i * blockSize, vectorsData + idx * blockSize, blockSize * sizeof(float));
	}
}

bool MnistTrainData::validateImages(const uint8_t *iptr, uint64_t size, size_t offset) {
	auto count = size / (imagesHeader.rows * imagesHeader.columns * sizeof(float));

	const float *ptr = (const float *)iptr;

	for (size_t i = 0; i < count; ++ i) {
    	auto idx = indexes[offset + i];
    	auto blockSize = imagesHeader.rows * imagesHeader.columns;

    	auto sourcePtr = ptr + i * blockSize;
    	auto targetPtr = imagesData + idx * blockSize;

    	if (::memcmp(sourcePtr, targetPtr, blockSize * sizeof(float)) != 0) {
    		for (size_t j = 0; j < imagesHeader.rows; ++ j) {
        		for (size_t k = 0; k < imagesHeader.columns; ++ k) {
        			std::cout << " " << *(sourcePtr + (j * imagesHeader.rows) + k);
        		}
        		std::cout << "\n";
    		}
    		std::cout << "\n";
    		for (size_t j = 0; j < imagesHeader.rows; ++ j) {
        		for (size_t k = 0; k < imagesHeader.columns; ++ k) {
        			std::cout << " " << *(targetPtr + (j * imagesHeader.rows) + k);
        		}
        		std::cout << "\n";
    		}
    		std::cout << "\n";
    		return false;
    	}
	}
	return true;
}

struct StdRandom {
	using result_type = unsigned int;

	static constexpr auto min() { return std::numeric_limits<unsigned int>::min(); };
	static constexpr auto max() { return std::numeric_limits<unsigned int>::max(); };

	unsigned int operator() () {
		return rnd->next();
	}

	Random *rnd;
};

void MnistTrainData::shuffle(Random &rnd) {
    std::shuffle(indexes.begin(), indexes.end(), StdRandom{&rnd});
}

MnistTrainData::MnistTrainData(StringView path) {
	loadVectors(path);
	loadImages(path);
	loadIndexes();
}

MnistTrainData::~MnistTrainData() {
	if (imagesData) {
		::free(imagesData);
		imagesData = nullptr;
	}
	if (vectorsData) {
		::free(vectorsData);
		vectorsData = nullptr;
	}
}

Rc<CsvData> ModelQueue::readCsv(StringView data) {
	Rc<CsvData> ret = Rc<CsvData>::alloc();

	auto readQuoted = [] (StringView &r) {
		StringView tmp(r);

		while (r.is("\"\"")) {
			r += 2;
		}

		while (!r.empty() && !r.is('"')) {
			r.skipUntil<StringView::Chars<'"', '\\'>>();
			if (r.is('\\')) {
				r += 2;
			}
			while (r.is("\"\"")) {
				r += 2;
			}
		}

		tmp = StringView(tmp.data(), r.data() - tmp.data());

		if (r.is('"')) {
			++ r;
		}

		return tmp;
	};

	auto readHeader = [&] (StringView &r) {
		while (!r.empty() && !r.is('\n') && !r.is("\r")) {
			r.skipChars<StringView::WhiteSpace>();
			if (r.is('"')) {
				++ r;
				ret->fields.emplace_back(readQuoted(r).str<Interface>());
				r.skipUntil<StringView::Chars<',', '\n', '\r'>>();
			} else {
				auto tmp = r.readUntil<StringView::Chars<',', '\n', '\r'>>();
				tmp.trimChars<StringView::WhiteSpace>();
				ret->fields.emplace_back(readQuoted(r).str<Interface>());
			}
			if (r.is(',')) {
				++ r;
			}
		}
		if (r.is('\n') || r.is('\r')) {
			r.skipChars<StringView::WhiteSpace>();
		}
	};

	auto validateFloat = [] (StringView str) {
		if (str.empty()) {
			return false;
		}

		StringView r(str);
		if (r.is('-')) { ++ r; }
		r.skipChars<chars::Range<char, '0', '9'>, StringView::Chars<'.'>>();
		if (!r.empty()) {
			return false;
		}

		return true;
	};

	auto readLine = [&] (StringView &r) {
		Value ret;
		while (!r.empty() && !r.is('\n') && !r.is("\r")) {
			r.skipChars<StringView::WhiteSpace>();
			if (r.is('"')) {
				++ r;
				auto data = readQuoted(r);
				if (valid::validateNumber(data)) {
					ret.addInteger(data.readInteger(10).get(0));
				} else if (validateFloat(data)) {
					ret.addInteger(data.readInteger(10).get(0));
				} else {
					ret.addString(data.str<Interface>());
				}
				r.skipUntil<StringView::Chars<',', '\n', '\r'>>();
			} else {
				auto tmp = r.readUntil<StringView::Chars<',', '\n', '\r'>>();
				tmp.trimChars<StringView::WhiteSpace>();
				if (valid::validateNumber(tmp)) {
					ret.addInteger(tmp.readInteger(10).get(0));
				} else if (validateFloat(data)) {
					ret.addInteger(data.readInteger(10).get(0));
				} else {
					ret.addString(tmp.str<Interface>());
				}
			}
			if (r.is(',')) {
				++ r;
			}
		}
		if (r.is('\n') || r.is('\r')) {
			r.skipChars<StringView::WhiteSpace>();
		}
		return ret;
	};

	while (!data.empty()) {
		if (ret->fields.empty()) {
			readHeader(data);
		} else {
			if (auto val = readLine(data)) {
				if (!val.getValue(0).isInteger()) {
					std::cout << val << "\n";
				}
				ret->data.emplace_back(move(val));
			}
		}
	}

	return ret;
}

ModelQueue::~ModelQueue() { }

bool ModelQueue::init(StringView modelPath, ModelFlags flags, StringView input) {
	_processor = Rc<ModelProcessor>::create();
	_model = _processor->load(FilePath(modelPath), flags);

	if (!_model) {
		return false;
	}

	core::Queue::Builder builder(filepath::name(modelPath));

	Extent3 frameExtent(1, 1, 1);
	if (input.starts_with("mnist:")) {
		_trainData = Rc<MnistTrainData>::alloc(input.sub(6));
	} else if (input.starts_with("csv:")) {
		auto data = filesystem::readIntoMemory<Interface>(input.sub(4));
		if (!data.empty()) {
			_csvData = readCsv(StringView((const char *)data.data(), data.size()));
		} else {
			return false;
		}
	} else {
		if (!bitmap::getImageSize(input, frameExtent.width, frameExtent.height)) {
			log::error("InputQueue", "fail to read image: ", input);
			return false;
		}
	}

	_image = input.str<Interface>();

	Map<Layer *, const core::AttachmentData *> inputs;
	Map<Attachment *, const core::AttachmentData *> attachments;

	auto modelInputs = _model->getInputs();
	for (auto &it : modelInputs) {
		inputs.emplace(it, it->makeInputAttachment(builder));
	}

	size_t i = 0;
	for (auto &it : _model->getSortedLayers()) {
		auto output = it->getOutput();
		auto attachment = it->makeOutputAttachment(builder, _model->getSortedLayers().size() == i + 1);
		attachments.emplace(output, attachment);
		if (_model->getSortedLayers().size() == i + 1) {
			_outputAttachment = attachment;
		}
		++ i;
	}

	for (auto &it : _model->getSortedLayers()) {
		auto ret = it->prepare(builder, inputs, attachments);
		if (!ret) {
			return false;
		}
		if (it->isInput()) {
			_inputAttachments.emplace_back(static_cast<vk::shadernn::InputLayer *>(ret->pass.get())->getDataAttachment());
		}
	}

	return Queue::init(move(builder));
}

void ModelQueue::run(Application *app) {
	_app = app;

	Extent3 frameExtent(1, 1, 1);
	if (!_trainData && !_csvData) {
		if (!bitmap::getImageSize(StringView(_image), frameExtent.width, frameExtent.height)) {
			log::error("InputQueue", "fail to read image: ", _image);
			return;
		}
	} else if (_csvData) {
		frameExtent.width = _csvData->fields.size();
		frameExtent.height = _csvData->data.size();
	}

	auto req = Rc<core::FrameRequest>::create(Rc<core::Queue>(this), core::FrameConstraints{frameExtent});

	ModelSpecialization spec = _processor->specializeModel(_model, frameExtent);

	for (auto &it : spec.attachments) {
		if (auto a = getAttachment(it.first->getName())) {
			if (auto img = dynamic_cast<vk::ImageAttachment *>(a->attachment.get())) {
				core::ImageInfoData info = img->getImageInfo();
				info.extent = it.second;
				log::debug("ModelQueue", "Specialize attachment ", it.first->getName(), " for extent ", info.extent);
				req->addImageSpecialization(img, move(info));
			}
		}
	}

	for (auto &it : _inputAttachments) {
		if (it->type == core::AttachmentType::Image) {
			auto inputData = Rc<vk::shadernn::InputDataInput>::alloc();
			inputData->norm = vk::shadernn::NormData{
				Vec4(-0.5f, -0.5f, -0.5f, -0.5f), // to -0.5 - 0.5
				Vec4(2.0f, 2.0f, 2.0f, 2.0f) // to -1.0 - 1.0
			};
			inputData->image.extent = frameExtent;
			inputData->image.stdCallback = [path = _image] (uint8_t *ptr, uint64_t size, const core::ImageData::DataCallback &dcb) {
				core::Resource::loadImageFileData(ptr, size, path, core::ImageFormat::R8G8B8A8_UNORM, dcb);
			};

			req->addInput(it, move(inputData));
		} else if (_trainData) {
			if (it->key == "input_samples_buffer") {
				auto inputData = Rc<vk::shadernn::InputBufferDataInput>::alloc();
				inputData->buffer.stdCallback = [data = _trainData, offset = _loadOffset] (uint8_t *ptr, uint64_t size, const core::BufferData::DataCallback &cb) {
					data->readImages(ptr, size, offset);
				};
				req->addInput(it, move(inputData));
			} else if (it->key == "input_labels_buffer") {
				auto inputData = Rc<vk::shadernn::InputBufferDataInput>::alloc();
				inputData->buffer.stdCallback = [data = _trainData, offset = _loadOffset] (uint8_t *ptr, uint64_t size, const core::BufferData::DataCallback &cb) {
					data->readLabels(ptr, size, offset);
				};
				req->addInput(it, move(inputData));
			}
		} else if (_csvData) {
			auto inputData = Rc<vk::shadernn::InputCsvInput>::alloc();
			inputData->data = _csvData->data;
			req->addInput(it, move(inputData));
		}
	}

	req->setOutput(getOutputAttachment(), [this, app, trainData = _trainData, csvData = _csvData] (core::FrameAttachmentData &data, bool success, Ref *) {
		if (data.image) {
			app->getGlLoop()->captureImage([app] (core::ImageInfoData info, BytesView view) {
				if (!view.empty()) {
					Vector<Bitmap> planes;
					for (size_t i = 0; i < info.extent.depth; ++ i) {
						auto &b = planes.emplace_back(Bitmap());
						b.alloc(info.extent.width, info.extent.height, bitmap::PixelFormat::RGBA8888, bitmap::AlphaFormat::Premultiplied);
					}

					Vector<uint8_t *> ptrs;
					for (auto &it : planes) {
						ptrs.emplace_back(it.dataPtr());
					}

					std::cout << view.size() << " ";

					for (size_t i = 0; i < info.extent.width; ++ i) {
						for (size_t j = 0; j < info.extent.height; ++ j) {
							for (size_t k = 0; k < info.extent.depth; ++ k) {
								for (size_t f = 0; f < 4; ++ f) {
									*ptrs[k] = uint8_t((view.readFloat32() + 1.0f) * 127.5f);
									if (f == 3) {
										//*ptrs[k] = 255;
									}
									ptrs[k] ++;
								}
							}
						}
					}

					std::cout << view.size() << "\n";

					for (size_t i = 0; i < info.extent.depth; ++ i) {
						planes[i].save(toString(i, "_", Time::now().toMicros(), ".png"));
					}
				}
				app->end();
			}, data.image->getImage(), core::AttachmentLayout::General);
		} else if (auto buf = dynamic_cast<vk::BufferAttachmentHandle *>(data.handle.get())) {
			if (trainData) {
				auto target = buf->getBuffers().back().buffer;
				app->getGlLoop()->captureBuffer([this, app] (const core::BufferInfo &info, BytesView view) {
					//front->synchronizeParameters(SpanView<float>((float *)view.data(), view.size() / sizeof(float)));
					onComplete(app, view);
				}, target);

			} else if (csvData) {
				auto target = buf->getBuffers().front().buffer;
				app->getGlLoop()->captureBuffer([app, trainData] (core::BufferInfo info, BytesView view) {
					std::cout << view.size() / (sizeof(float) * 4) << "\n";
					std::cout << "0";

					size_t row = 1;
					size_t i = 0;
					while (!view.empty()) {
						switch (i) {
						case 0: {
							auto u = view.readUnsigned32();
							if (u > uint32_t(maxOf<int32_t>())) {
								std::cout << ", " << static_cast<int32_t>(u);
							} else {
								std::cout << ", " << u;
							}
							break;
						}
						case 1:
							std::cout << ", " << view.readUnsigned32();
							break;
						case 2:
						case 3:
							std::cout << ", " << view.readFloat32();
							break;
						default:
							view.readUnsigned32();
							break;
						}
						++ i;
						if (i > 3) {
							i = 0;
							std::cout << "\n" << row ++;
						}
					}
					std::cout << "\n";
				}, target);
			}
		}
		return true;
	});

	app->getGlLoop()->runRenderQueue(move(req), 0);
}

void ModelQueue::onComplete(Application *app, BytesView data) {
	//auto iter = _loadOffset / 100;
	_loadOffset += 100;

	data.readFloat32();
	auto loss = data.readFloat32();

	if (_loadOffset < 60000) {
		// 5  std::cout << iter << " Loss: " << loss << "\n";
		_epochLoss += loss;
		app->performOnAppThread([this, app] {
			run(_app);
		});
	} else {
		std::cout << _epoch << ": avg loss: " << _epochLoss / 600.0f << "\n";

		_trainData->shuffle(_model->getRand());
		_epochLoss = 0;
		_loadOffset = 0;
		++ _epoch;

		if (_epoch < _endEpoch) {
			app->performOnAppThread([this, app] {
				run(app);
			});
		} else {
			release(0);
			app->end();
		}
	}
}

}
