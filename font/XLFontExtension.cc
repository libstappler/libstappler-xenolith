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

#include "XLFontExtension.h"
#include "XLCoreLoop.h"
#include "XLCoreQueue.h"
#include "XLCoreImageStorage.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameQueue.h"
#include "XLApplication.h"
#include "XLVkFontQueue.h"
#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_MULTIPLE_MASTERS_H
#include FT_SFNT_NAMES_H
#include FT_ADVANCES_H

#include "SPFontFace.h"
#include "SPBitmap.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

Rc<core::Queue> FontExtension::createFontQueue(core::Instance *instance, StringView name) {
	return Rc<vk::FontQueue>::create(name);
}

Rc<ApplicationExtension> FontExtension::createFontExtension(Rc<Application> &&app, Rc<core::Queue> &&loop) {
	return Rc<FontExtension>::create(move(app), move(loop));
}

Rc<ApplicationExtension> FontExtension::createDefaultController(FontExtension *ext, StringView name) {
	auto builder = ext->makeDefaultControllerBuilder(name);
	return ext->acquireController(move(builder));
}

FontExtension::~FontExtension() {
	_queue = nullptr;
}

bool FontExtension::init(Rc<Application> &&loop, Rc<core::Queue> &&q) {
	_mainLoop = move(loop);
	_glLoop = _mainLoop->getGlLoop();
	_queue = move(q);
	_library = Rc<FontLibrary>::alloc();
	return true;
}

void FontExtension::initialize(Application *app) {
	_glLoop = app->getGlLoop();
	if (_queue->isCompiled()) {
		onActivated();
	} else {
		auto linkId = retain();
		_glLoop->compileQueue(_queue, [this, linkId] (bool success) {
			if (success) {
				_mainLoop->performOnAppThread([this] {
					onActivated();
				}, this);
			}
			release(linkId);
		});
	}
}

void FontExtension::invalidate(Application *) {
	_library->invalidate();

	_queue = nullptr;
	_mainLoop = nullptr;
	_glLoop = nullptr;
}

void FontExtension::update(Application *, const UpdateTime &clock) {
	_library->update();
}

static Bytes openResourceFont(FontLibrary::DefaultFontName name) {
	auto d = FontLibrary::getFont(name);
	return data::decompress<memory::StandartInterface>(d.data(), d.size());
}

static String getResourceFontName(FontLibrary::DefaultFontName name) {
	return toString("resource:", FontLibrary::getFontName(name));
}

static const FontController::FontSource * makeResourceFontQuery(FontController::Builder &builder, FontLibrary::DefaultFontName name,
		FontLayoutParameters params = FontLayoutParameters()) {
	return builder.addFontSource( getResourceFontName(name), [name] { return openResourceFont(name); }, params);
}

FontController::Builder FontExtension::makeDefaultControllerBuilder(StringView key) {
	FontController::Builder ret(key);

	auto res_RobotoFlex = makeResourceFontQuery(ret, DefaultFontName::RobotoFlex_VariableFont);
	auto res_RobotoMono_Variable = makeResourceFontQuery(ret, DefaultFontName::RobotoMono_VariableFont);
	auto res_RobotoMono_Italic_Variable = makeResourceFontQuery(ret, DefaultFontName::RobotoMono_Italic_VariableFont, FontLayoutParameters{
		FontStyle::Italic, FontWeight::Normal, FontStretch::Normal});
	auto res_DejaVuSans = ret.addFontSource( getResourceFontName(DefaultFontName::DejaVuSans), FontLibrary::getFont(DefaultFontName::DejaVuSans));

	ret.addFontFaceQuery("sans", res_RobotoFlex);
	ret.addFontFaceQuery("sans", res_DejaVuSans);
	ret.addFontFaceQuery("monospace", res_RobotoMono_Variable);
	ret.addFontFaceQuery("monospace", res_RobotoMono_Italic_Variable);

	ret.addAlias("default", "sans");
	ret.addAlias("system", "sans");

	return ret;
}

Rc<FontController> FontExtension::acquireController(FontController::Builder &&b) {
	struct ControllerBuilder : Ref {
		FontController::Builder builder;
		Rc<FontController> controller;

		bool invalid = false;
		std::atomic<size_t> pendingData = 0;
		FontExtension *ext = nullptr;

		ControllerBuilder(FontController::Builder &&b)
		: builder(move(b)) {
			if (auto c = builder.getTarget()) {
				controller = c;
			}
		}

		void invalidate() {
			controller = nullptr;
		}

		void loadData() {
			if (invalid) {
				invalidate();
				return;
			}

			ext->getMainLoop()->performOnAppThread([this] () {
				for (const Pair<const String, FontController::FamilyQuery> &it : builder.getFamilyQueries()) {
					Vector<Rc<FontFaceData>> d; d.reserve(it.second.sources.size());
					for (auto &iit : it.second.sources) {
						d.emplace_back(move(iit->data));
					}
					controller->addFont(it.second.family, sp::move(d), it.second.addInFront);
				}

				if (builder.getTarget()) {
					for (auto &it : builder.getAliases()) {
						controller->addAlias(it.first, it.second);
					}
					controller->sendFontUpdatedEvent();
				} else {
					controller->setAliases(Map<String, String>(builder.getAliases()));
					controller->setLoaded(true);
				}
				controller = nullptr;
			}, this);
		}

		void onDataLoaded(bool success) {
			auto v = pendingData.fetch_sub(1);
			if (!success) {
				invalid = true;
				if (v == 1) {
					invalidate();
				}
			} else if (v == 1) {
				loadData();
			}
		}
	};

	bool hasController = (b.getTarget() != nullptr);
	auto builder = Rc<ControllerBuilder>::alloc(move(b));
	builder->ext = this;
	if (!hasController) {
		builder->controller = Rc<FontController>::create(this, builder->builder.getName());
	}

	builder->pendingData = builder->builder.getDataQueries().size();

	for (auto &it : builder->builder.getDataQueries()) {
		_mainLoop->perform([this, name = it.first, sourcePtr = &it.second, builder] (const thread::Task &) mutable -> bool {
			sourcePtr->data = _library->openFontData(name, sourcePtr->params, sourcePtr->preconfiguredParams, [&] () -> FontLibrary::FontData {
				if (sourcePtr->fontCallback) {
					return FontLibrary::FontData(sp::move(sourcePtr->fontCallback));
				} else if (!sourcePtr->fontExternalData.empty()) {
					return FontLibrary::FontData(sourcePtr->fontExternalData, true);
				} else if (!sourcePtr->fontMemoryData.empty()) {
					return FontLibrary::FontData(sp::move(sourcePtr->fontMemoryData));
				} else if (!sourcePtr->fontFilePath.empty()) {
					auto d = filesystem::readIntoMemory<Interface>(sourcePtr->fontFilePath);
					if (!d.empty()) {
						return FontLibrary::FontData(sp::move(d));
					}
				}
				return FontLibrary::FontData(BytesView(), false);
			});
			if (sourcePtr->data) {
				builder->onDataLoaded(true);
			} else {
				builder->onDataLoaded(false);
			}
			return true;
		});
	}

	return builder->controller;
}

Rc<core::DynamicImage> FontExtension::makeInitialImage(StringView name) const {
	return Rc<core::DynamicImage>::create([name = name.str<Interface>()] (core::DynamicImage::Builder &builder) {
		builder.setImage(name,
			core::ImageInfo(
					Extent2(2, 2),
					core::ImageUsage::Sampled | core::ImageUsage::TransferSrc,
					core::PassType::Graphics,
					core::ImageFormat::R8_UNORM
			), [] (uint8_t *ptr, uint64_t, const core::ImageData::DataCallback &cb) {
				if (ptr) {
					ptr[0] = 0;
					ptr[1] = 255;
					ptr[2] = 255;
					ptr[3] = 0;
				} else {
					Bytes bytes; bytes.reserve(4);
					bytes.emplace_back(0);
					bytes.emplace_back(255);
					bytes.emplace_back(255);
					bytes.emplace_back(0);
					cb(bytes);
				}
			}, nullptr);
		return true;
	});
}

void FontExtension::updateImage(const Rc<core::DynamicImage> &image, Vector<font::FontUpdateRequest> &&data,
		Rc<core::DependencyEvent> &&dep) {
	if (!_mainLoop) {
		return; // extension is disabled
	}

	if (!_active) {
		_pendingImageQueries.emplace_back(ImageQuery{image, sp::move(data), sp::move(dep)});
		return;
	}

	auto input = Rc<RenderFontInput>::alloc();
	input->queue = _mainLoop->getAppLooper()->getThreadPool();
	input->image = image;
	input->ext = this;
	input->requests = sp::move(data);
	/*input->output = [] (const core::ImageInfoData &info, BytesView data) {
		Bitmap bmp(data, info.extent.width, info.extent.height, bitmap::PixelFormat::A8);
		bmp.save(bitmap::FileFormat::Png, toString(Time::now().toMicros(), ".png"));
	};*/

	auto req = Rc<core::FrameRequest>::create(_queue);
	if (dep) {
		req->addSignalDependency(sp::move(dep));
	}

	for (auto &it : _queue->getInputAttachments()) {
		req->addInput(it, sp::move(input));
		break;
	}

	_glLoop->runRenderQueue(sp::move(req), 0, [app = Rc<Application>(_mainLoop)] (bool success) {
		app->wakeup();
	});
}

void FontExtension::onActivated() {
	_active = true;

	for (auto &it : _pendingImageQueries) {
		updateImage(it.image, sp::move(it.chars), sp::move(it.dependency));
	}

	_pendingImageQueries.clear();
}

}
