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

namespace stappler::xenolith::font {

struct RenderFontInput : public core::AttachmentInputData {
	Rc<thread::TaskQueue> queue;
	Rc<core::DynamicImage> image;
	Rc<font::FontLibrary> library;
	Vector<FontUpdateRequest> requests;
	Function<void(const core::ImageInfoData &, BytesView)> output;
};

class FontFaceObjectHandle : public Ref {
public:
	virtual ~FontFaceObjectHandle();

	bool init(const Rc<FontLibrary> &, Rc<FontFaceObject> &&, Function<void(const FontFaceObjectHandle *)> &&onDestroy);

	FT_Face getFace() const { return _face->getFace(); }

	bool acquireTexture(char16_t, const Callback<void(const CharTexture &)> &);

protected:
	Rc<FontLibrary> _library;
	Rc<FontFaceObject> _face;
	Function<void(const FontFaceObjectHandle *)> _onDestroy;
};

class FontLibrary : public ApplicationExtension {
public:
	enum class DefaultFontName {
		None,
		RobotoFlex_VariableFont,
		RobotoMono_VariableFont,
		RobotoMono_Italic_VariableFont,
	};

	struct FontData {
		bool persistent;
		BytesView view;
		Bytes bytes;
		Function<Bytes()> callback;

		FontData(BytesView v, bool p) : persistent(p) {
			if (persistent) {
				view = v;
			} else {
				bytes = v.bytes<Interface>();
				view = bytes;
			}
		}
		FontData(Bytes &&b) : persistent(false), bytes(move(b)) {
			view = bytes;
		}
		FontData(Function<Bytes()> &&cb) : persistent(true), callback(move(cb)) { }
	};

	static BytesView getFont(DefaultFontName);
	static StringView getFontName(DefaultFontName);

	FontLibrary();
	virtual ~FontLibrary();

	bool init(Rc<Application> &&, Rc<core::Queue> &&);

	Application *getMainLoop() const { return _mainLoop; }
	const core::Loop *getGlLoop() const { return _glLoop; }
	const Rc<core::Queue> &getQueue() const { return _queue; }

	Rc<FontFaceData> openFontData(StringView, FontLayoutParameters params, const Callback<FontData()> & = nullptr);

	Rc<FontFaceObject> openFontFace(StringView, const FontSpecializationVector &, const Callback<FontData()> &);
	Rc<FontFaceObject> openFontFace(const Rc<FontFaceData> &, const FontSpecializationVector &);

	virtual void initialize(Application *) override;
	virtual void invalidate(Application *) override;
	virtual void update(Application *, const UpdateTime &clock) override;

	FontController::Builder makeDefaultControllerBuilder(StringView);

	Rc<FontController> acquireController(FontController::Builder &&);

	void updateImage(const Rc<core::DynamicImage> &, Vector<FontUpdateRequest> &&,
			Rc<core::DependencyEvent> &&);

	uint16_t getNextId();
	void releaseId(uint16_t);

	Rc<FontFaceObjectHandle> makeThreadHandle(const Rc<FontFaceObject> &);

protected:
	FT_Face newFontFace(BytesView);
	void doneFontFace(FT_Face);

	void onActivated();

	struct ImageQuery {
		Rc<core::DynamicImage> image;
		Vector<FontUpdateRequest> chars;
		Rc<core::DependencyEvent> dependency;
	};

	bool _active = false;

	Mutex _mutex;
	std::shared_mutex _sharedMutex;
	Map<StringView, Rc<FontFaceObject>> _faces;
	Map<StringView, Rc<FontFaceData>> _data;
	Map<FontFaceObject *, Map<std::thread::id, Rc<FontFaceObjectHandle>>> _threads;
	FT_Library _library = nullptr;

	Rc<Application> _mainLoop;
	Rc<core::Loop> _glLoop;
	Rc<core::Queue> _queue;
	Vector<ImageQuery> _pendingImageQueries;
	std::bitset<1024 * 16> _fontIds;
};

}

#endif /* XENOLITH_FONT_XLFONTLIBRARY_H_ */