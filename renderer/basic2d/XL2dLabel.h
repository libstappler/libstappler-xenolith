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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DLABEL_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DLABEL_H_

#include "XL2dSprite.h"
#include "XLCoreInput.h"
#include "XLFontLabelBase.h"
#include <future>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class EventListener;

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

struct LabelResult : Ref {
	TransformVertexData data;
	Vector<ColorMask> colorMap;
};

class LabelDeferredResult : public DeferredVertexResult {
public:
	virtual ~LabelDeferredResult();

	bool init(std::future<Rc<LabelResult>> &&);

	virtual SpanView<TransformVertexData> getData() override;

	virtual void handleReady(Rc<LabelResult> &&);

	void updateColor(const Color4F &);

	Rc<VertexData> getResult() const;

protected:
	mutable Mutex _mutex;
	Rc<LabelResult> _result;
	std::future<Rc<LabelResult>> *_future = nullptr;
};

class Label : public Sprite, public font::LabelBase {
public:
	using TextLayout = font::TextLayout;
	using LineLayout = font::LineLayoutData;
	using TextAlign = font::TextAlign;

	using ColorMapVec = Vector<Vector<bool>>;

	class Selection : public Sprite {
	public:
		virtual ~Selection();
		virtual bool init() override;
		virtual void clear();
		virtual void emplaceRect(const Rect &);
		virtual void updateColor() override;

		virtual core::TextCursor getTextCursor() const { return _cursor; }
		virtual void setTextCursor(core::TextCursor c) { _cursor = c; }

	protected:
		virtual void updateVertexes() override;

		core::TextCursor _cursor = core::TextCursor::InvalidCursor;
	};

	static void writeQuads(VertexArray &vertexes, const font::TextLayoutData<memory::StandartInterface> *format, Vector<ColorMask> &colorMap);
	static void writeQuads(VertexArray &vertexes, const font::TextLayoutData<memory::PoolInterface> *format, Vector<ColorMask> &colorMap);
	static Rc<LabelResult> writeResult(TextLayout *format, const Color4F &);

	virtual ~Label();

	virtual bool init() override;
	virtual bool init(StringView) override;
	virtual bool init(StringView, float w, TextAlign = TextAlign::Left);
	virtual bool init(font::FontController *, const DescriptionStyle & = DescriptionStyle(),
			StringView = StringView(), float w = 0.0f, TextAlign = TextAlign::Left);
	virtual bool init(const DescriptionStyle &, StringView = StringView(),
			float w = 0.0f, TextAlign = TextAlign::Left);

	virtual void tryUpdateLabel();

	virtual void setStyle(const DescriptionStyle &);
	virtual const DescriptionStyle &getStyle() const;

	virtual void onContentSizeDirty() override;
	virtual void onTransformDirty(const Mat4 &) override;
	virtual void onGlobalTransformDirty(const Mat4 &) override;

	virtual void setAdjustValue(uint8_t);
	virtual uint8_t getAdjustValue() const;

	virtual bool isOverflow() const;

	virtual size_t getCharsCount() const;
	virtual size_t getLinesCount() const;
	virtual LineLayout getLine(uint32_t num) const;

	virtual uint16_t getFontHeight() const;

	virtual Vec2 getCursorPosition(uint32_t charIndex, bool prefix = true) const;
	virtual Vec2 getCursorOrigin() const;

	// returns character index in FormatSpec for position in label or maxOf<uint32_t>()
	// pair.second - true if index match suffix or false if index match prefix
	// use convertToNodeSpace to get position
	virtual Pair<uint32_t, bool> getCharIndex(const Vec2 &, font::CharSelectMode = font::CharSelectMode::Best) const;

	virtual core::TextCursor selectWord(uint32_t) const;

	virtual float getMaxLineX() const;

	virtual void setDeferred(bool);
	virtual bool isDeferred() const { return _deferred; }

	virtual void setSelectionCursor(core::TextCursor);
	virtual core::TextCursor getSelectionCursor() const;

	virtual void setSelectionColor(const Color4F &);
	virtual Color4F getSelectionColor() const;

	virtual void setMarkedCursor(core::TextCursor);
	virtual core::TextCursor getMarkedCursor() const;

	virtual void setMarkedColor(const Color4F &);
	virtual Color4F getMarkedColor() const;

protected:
	using Sprite::init;

	virtual Rc<LabelDeferredResult> runDeferred(thread::TaskQueue &, TextLayout *format, const Color4F &color);
	virtual void updateLabel();
	virtual void onFontSourceUpdated();
	virtual void onFontSourceLoaded();
	virtual void onLayoutUpdated();
	virtual void updateColor() override;
	virtual void updateVertexes() override;
	virtual void updateVertexesColor() override;

	virtual void updateQuadsForeground(font::FontController *, TextLayout *, Vector<ColorMask> &);

	virtual bool checkVertexDirty() const override;

	virtual NodeFlags processParentFlags(FrameInfo &info, NodeFlags parentFlags) override;

	virtual void pushCommands(FrameInfo &, NodeFlags flags) override;

	void updateLabelScale(const Mat4 &parent);

	EventListener *_listener = nullptr;
	Time _quadRequestTime;
	Rc<font::FontController> _source;
	Rc<TextLayout> _format;
	Vector<ColorMask> _colorMap;

	bool _deferred = true;

	uint8_t _adjustValue = 0;
	size_t _updateCount = 0;

	Selection *_selection = nullptr;
	Selection *_marked = nullptr;

	Rc<LabelDeferredResult> _deferredResult;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DLABEL_H_ */
