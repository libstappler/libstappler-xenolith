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

#ifndef XENOLITH_FONT_XLFONTLIBRARY_H_
#define XENOLITH_FONT_XLFONTLIBRARY_H_

#include "XLFontController.h"
#include "XLCoreAttachment.h"
#include "SPFontLibrary.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

class FontExtension;

struct RenderFontInput : public core::AttachmentInputData {
	Rc<thread::TaskQueue> queue;
	Rc<core::DynamicImage> image;
	Rc<FontExtension> ext;
	Vector<FontUpdateRequest> requests;
	Function<void(const core::ImageInfoData &, BytesView)> output;
};

class FontExtension : public ApplicationExtension {
public:
	using DefaultFontName = FontLibrary::DefaultFontName;

	virtual ~FontExtension();

	bool init(Rc<Application> &&, Rc<core::Queue> &&);

	Application *getMainLoop() const { return _mainLoop; }
	const core::Loop *getGlLoop() const { return _glLoop; }
	const Rc<core::Queue> &getQueue() const { return _queue; }
	FontLibrary *getLibrary() const { return _library; }

	virtual void initialize(Application *) override;
	virtual void invalidate(Application *) override;
	virtual void update(Application *, const UpdateTime &clock) override;

	FontController::Builder makeDefaultControllerBuilder(StringView);

	Rc<FontController> acquireController(FontController::Builder &&);

	void updateImage(const Rc<core::DynamicImage> &, Vector<FontUpdateRequest> &&,
			Rc<core::DependencyEvent> &&);

protected:
	void onActivated();

	struct ImageQuery {
		Rc<core::DynamicImage> image;
		Vector<FontUpdateRequest> chars;
		Rc<core::DependencyEvent> dependency;
	};

	bool _active = false;
	Rc<FontLibrary> _library;
	Rc<Application> _mainLoop;
	Rc<core::Loop> _glLoop;
	Rc<core::Queue> _queue;
	Vector<ImageQuery> _pendingImageQueries;
};

}

#endif /* XENOLITH_FONT_XLFONTLIBRARY_H_ */
