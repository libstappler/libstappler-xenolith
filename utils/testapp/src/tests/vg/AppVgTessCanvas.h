/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef TEST_SRC_TESTS_VG_APPVGTESSCANVAS_H_
#define TEST_SRC_TESTS_VG_APPVGTESSCANVAS_H_

#include "AppTests.h" // IWYU pragma: keep
#include "XL2dLayer.h" // IWYU pragma: keep
#include "XL2dLabel.h"
#include "XL2dVectorSprite.h"
#include "XLInput.h"

namespace stappler::xenolith::app {

class VgTessCursor : public VectorSprite {
public:
	enum State {
		Point,
		Capture,
		Target
	};

	virtual ~VgTessCursor() = default;

	virtual bool init() override;

	void setState(State);
	State getState() const { return _state; }

protected:
	using VectorSprite::init;

	void updateState(VectorImage &image, State);

	State _state = State::Point;
};

class VgTessPoint : public VectorSprite {
public:
	virtual ~VgTessPoint() = default;

	bool init(const Vec2 &p, uint32_t index);

	void setPoint(const Vec2 &);
	const Vec2 &getPoint() const { return _point; }

	void setIndex(uint32_t index);
	uint32_t getIndex() const { return _index; }

protected:
	using VectorSprite::init;

	uint32_t _index = 0;
	Vec2 _point;
	Label *_label = nullptr;
};

class VgTessCanvas : public Node {
public:
	static Color getColorForIndex(uint32_t idx);

	virtual ~VgTessCanvas();

	virtual bool init(Function<void()> &&);

	virtual void handleEnter(Scene *) override;
	virtual void handleContentSizeDirty() override;

	void setWinding(vg::Winding);
	void setDrawStyle(vg::DrawFlags);

	vg::Winding getWinding() const { return _winding; }
	vg::DrawFlags getDrawStyle() const { return _drawStyle; }

	void setSelectedContour(uint32_t);
	uint32_t getSelectedContour() const;
	uint32_t getContoursCount() const;

	void addContour();

	void setStrokeWidth(float);
	float getStrokeWidth() const;

protected:
	using Node::init;

	void onTouch(const InputEvent &);
	void onMouseMove(const InputEvent &);

	bool onPointerEnter(bool);

	void onActionTouch(const InputEvent &);

	VgTessPoint *getTouchedPoint(const Vec2 &) const;

	void updatePoints();
	void saveData();

	struct ContourData {
		uint32_t index;
		Vector<Rc<VgTessPoint>> points;
	};

	Function<void()> _onContourUpdated;

	float _strokeWidth = 25.0f;
	bool _pointerInWindow = false;
	Vec2 _currentLocation;
	VgTessCursor *_cursor = nullptr;

	VectorSprite *_test1 = nullptr;
	VectorSprite *_test2 = nullptr;

	vg::Winding _winding = vg::Winding::EvenOdd;
	vg::DrawFlags _drawStyle = vg::DrawFlags::Stroke;
	uint32_t _contourSelected = 0;
	Vector<ContourData> _contours;

	VgTessPoint *_capturedPoint = nullptr;
	VectorSprite *_pathFill = nullptr;
	VectorSprite *_pathLines = nullptr;
};

} // namespace stappler::xenolith::app

#endif /* TEST_SRC_TESTS_VG_APPVGTESSCANVAS_H_ */
