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

#include "AppActionEaseTest.h"
#include "SPVec2.h"
#include "XLAction.h"
#include "XLActionEase.h"
#include "XLEventListener.h"
#include "XLGestureRecognizer.h"
#include "XLInputListener.h"
#include "XLCurveBuffer.h"
#include "XLDirector.h"
#include "XLTemporaryResource.h"

namespace stappler::xenolith::app {

bool ActionEaseNode::init(StringView str, Function<Rc<ActionInterval>(Rc<ActionInterval> &&)> &&cb,
		Function<void()> &&onActivated) {
	if (!Node::init()) {
		return false;
	}

	_label = addChild(Rc<Label>::create());
	_label->setString(str);
	_label->setAlignment(Label::TextAlign::Right);
	_label->setAnchorPoint(Anchor::MiddleRight);
	_label->setFontSize(20);

	_layer = addChild(Rc<Layer>::create(Color::Red_500));
	_layer->setAnchorPoint(Anchor::BottomLeft);
	_layer->setContentSize(Size2(48.0f, 48.0f));

	_callback = sp::move(cb);
	_activated = onActivated;

	auto l = _layer->addComponent(Rc<InputListener>::create());
	l->addTapRecognizer([this](const GestureTap &tap) {
		run();
		return true;
	});

	l->addMouseOverRecognizer([&](const GestureData &data) {
		if (data.event == GestureEvent::Began) {
			if (_activated) {
				_activated();
			}
			_layer->setColor(Color::Orange_500);
		} else if (data.event == GestureEvent::Ended || data.event == GestureEvent::Cancelled) {
			_layer->setColor(Color::Red_500);
		}
		return true;
	});

	return true;
}

void ActionEaseNode::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	_label->setPosition(Vec2(-4.0f, _contentSize.height / 2.0f));
	_layer->setContentSize(Size2(48.0f, _contentSize.height));
}

void ActionEaseNode::run() {
	_layer->stopAllActions();

	auto progress = _layer->getPosition().x / (_contentSize.width - _layer->getContentSize().width);
	if (progress < 0.5f) {
		auto a = _callback(Rc<MoveTo>::create(_time,
				Vec2(_contentSize.width - _layer->getContentSize().width, 0.0f)));
		_layer->runAction(move(a));
	} else {
		auto a = _callback(Rc<MoveTo>::create(_time, Vec2(0.0f, 0.0f)));
		_layer->runAction(move(a));
	}
}

bool ActionEaseTest::init() {
	if (!LayoutTest::init(LayoutName::ActionEaseTest, "")) {
		return false;
	}

	_slider = addChild(Rc<SliderWithLabel>::create("Time: 1.0", 0.0f, [this](float value) {
		for (auto &it : _nodes) { it->setTime(1.0f + 9.0f * value); }
		_slider->setString(toString("Time: ", 1.0f + 9.0f * value));
	}));
	_slider->setAnchorPoint(Anchor::Middle);

	_button = addChild(Rc<ButtonWithLabel>::create("Run all", [&]() {
		for (auto &it : _nodes) { it->run(); }
	}));
	_button->setAnchorPoint(Anchor::Middle);

	_selectedLabel = addChild(Rc<Label>::create());
	_selectedLabel->setAlignment(Label::TextAlign::Center);
	_selectedLabel->setAnchorPoint(Anchor::Middle);

	_curveSprite = addChild(Rc<Sprite>::create());
	_curveSprite->setAnchorPoint(Anchor::MiddleTop);
	_curveSprite->setContentSize(Size2(256.0f, 128.0f));
	_curveSprite->setTextureAutofit(Autofit::Contain);
	_curveSprite->setColor(Color::Blue_500);

	_modeButton = addChild(Rc<ButtonWithLabel>::create("InOut", [&]() {
		switch (_mode) {
		case Mode::InOut:
			_mode = Mode::In;
			_modeButton->setString("In");
			break;
		case Mode::In:
			_mode = Mode::Out;
			_modeButton->setString("Out");
			break;
		case Mode::Out:
			_mode = Mode::InOut;
			_modeButton->setString("InOut");
			break;
		}
		setSelected(_selectedType);
	}));
	_modeButton->setAnchorPoint(Anchor::Middle);

	auto makeNode = [&](StringView name, interpolation::Type type) {
		auto node = addChild(Rc<ActionEaseNode>::create(name, [this, type](Rc<ActionInterval> &&a) {
			return makeAction(getSelectedType(type), move(a));
		}, [this, type] { setSelected(type); }));
		node->setAnchorPoint(Anchor::Middle);
		return node;
	};

	_nodes.emplace_back(makeNode("Elastic:", interpolation::ElasticEaseInOut));
	_nodes.emplace_back(makeNode("Bounce:", interpolation::BounceEaseInOut));
	_nodes.emplace_back(makeNode("Back:", interpolation::BackEaseInOut));
	_nodes.emplace_back(makeNode("Sine:", interpolation::SineEaseInOut));
	_nodes.emplace_back(makeNode("Exponential:", interpolation::ExpoEaseInOut));
	_nodes.emplace_back(makeNode("Quadratic:", interpolation::QuadEaseInOut));
	_nodes.emplace_back(makeNode("Cubic:", interpolation::CubicEaseInOut));
	_nodes.emplace_back(makeNode("Quartic:", interpolation::QuartEaseInOut));
	_nodes.emplace_back(makeNode("Quintic:", interpolation::QuintEaseInOut));
	_nodes.emplace_back(makeNode("Circle:", interpolation::CircEaseInOut));

	return true;
}

void ActionEaseTest::handleContentSizeDirty() {
	LayoutTest::handleContentSizeDirty();

	auto size = 28.0f * _nodes.size();
	auto offset = size / 2.0f;

	_slider->setPosition(_contentSize / 2.0f + Size2(0.0f, offset + 36.0f));
	_slider->setContentSize(Size2(200.0f, 24.0f));

	_button->setPosition(_contentSize / 2.0f + Size2(0.0f, offset + 72.0f));
	_button->setContentSize(Size2(200.0f, 36.0f));

	_modeButton->setPosition(_contentSize / 2.0f + Size2(0.0f, offset + 114.0f));
	_modeButton->setContentSize(Size2(200.0f, 36.0f));

	for (auto &it : _nodes) {
		it->setPosition(_contentSize / 2.0f + Size2(72.0f, offset));
		it->setContentSize(Size2(std::min(_contentSize.width - 160.0f, 600.0f), 24.0f));
		offset -= 28.0f;
	}

	_selectedLabel->setPosition(_contentSize / 2.0f + Size2(0.0f, offset));
	_curveSprite->setPosition(_contentSize / 2.0f + Size2(0.0f, offset - 40.0f));
}

Rc<ActionInterval> ActionEaseTest::makeAction(interpolation::Type type,
		Rc<ActionInterval> &&a) const {
	return Rc<EaseActionTyped>::create(move(a), type);
}

interpolation::Type ActionEaseTest::getSelectedType(interpolation::Type type) const {
	return interpolation::Type(type - toInt(_mode));
}

void ActionEaseTest::setSelected(interpolation::Type type) {
	_selectedType = type;

	auto t = getSelectedType(type);
	if (_selectedSubType != t) {
		_selectedSubType = t;

		switch (t) {
		case interpolation::EaseIn: _selectedLabel->setString("EaseIn"); break;
		case interpolation::EaseOut: _selectedLabel->setString("EaseOut"); break;
		case interpolation::EaseInOut: _selectedLabel->setString("EaseInOut"); break;

		case interpolation::SineEaseIn: _selectedLabel->setString("SineEaseIn"); break;
		case interpolation::SineEaseOut: _selectedLabel->setString("SineEaseOut"); break;
		case interpolation::SineEaseInOut: _selectedLabel->setString("SineEaseInOut"); break;

		case interpolation::QuadEaseIn: _selectedLabel->setString("QuadEaseIn"); break;
		case interpolation::QuadEaseOut: _selectedLabel->setString("QuadEaseOut"); break;
		case interpolation::QuadEaseInOut: _selectedLabel->setString("QuadEaseInOut"); break;

		case interpolation::CubicEaseIn: _selectedLabel->setString("CubicEaseIn"); break;
		case interpolation::CubicEaseOut: _selectedLabel->setString("CubicEaseOut"); break;
		case interpolation::CubicEaseInOut: _selectedLabel->setString("CubicEaseInOut"); break;

		case interpolation::QuartEaseIn: _selectedLabel->setString("QuartEaseIn"); break;
		case interpolation::QuartEaseOut: _selectedLabel->setString("QuartEaseOut"); break;
		case interpolation::QuartEaseInOut: _selectedLabel->setString("QuartEaseInOut"); break;

		case interpolation::QuintEaseIn: _selectedLabel->setString("QuintEaseIn"); break;
		case interpolation::QuintEaseOut: _selectedLabel->setString("QuintEaseOut"); break;
		case interpolation::QuintEaseInOut: _selectedLabel->setString("QuintEaseInOut"); break;

		case interpolation::ExpoEaseIn: _selectedLabel->setString("ExpoEaseIn"); break;
		case interpolation::ExpoEaseOut: _selectedLabel->setString("ExpoEaseOut"); break;
		case interpolation::ExpoEaseInOut: _selectedLabel->setString("ExpoEaseInOut"); break;

		case interpolation::CircEaseIn: _selectedLabel->setString("CircEaseIn"); break;
		case interpolation::CircEaseOut: _selectedLabel->setString("CircEaseOut"); break;
		case interpolation::CircEaseInOut: _selectedLabel->setString("CircEaseInOut"); break;

		case interpolation::ElasticEaseIn: _selectedLabel->setString("ElasticEaseIn"); break;
		case interpolation::ElasticEaseOut: _selectedLabel->setString("ElasticEaseOut"); break;
		case interpolation::ElasticEaseInOut: _selectedLabel->setString("ElasticEaseInOut"); break;

		case interpolation::BackEaseIn: _selectedLabel->setString("BackEaseIn"); break;
		case interpolation::BackEaseOut: _selectedLabel->setString("BackEaseOut"); break;
		case interpolation::BackEaseInOut: _selectedLabel->setString("BackEaseInOut"); break;

		case interpolation::BounceEaseIn: _selectedLabel->setString("BounceEaseIn"); break;
		case interpolation::BounceEaseOut: _selectedLabel->setString("BounceEaseOut"); break;
		case interpolation::BounceEaseInOut: _selectedLabel->setString("BounceEaseInOut"); break;
		default: _selectedLabel->setString(""); break;
		}

		auto str = _selectedLabel->getString8();
		if (!str.empty()) {
			auto texname = toString("tmp://", str);

			_curveBuffer = Rc<CurveBuffer>::create(256, t, SpanView<float>());

			CurveBuffer::RenderInfo renderInfo{Extent2(256 + 16, 128 + 32), UVec2{8, 16},
				UVec2{256 + 8, 128 + 16}, 0, 128, 192, 96};

			auto bitmap = _curveBuffer->renderComponent(renderInfo, 0);
			auto cache = _director->getResourceCache();

			if (auto res = cache->getTemporaryResource(texname)) {
				if (auto tex = res->acquireTexture(texname)) {
					_curveSprite->scheduleTextureUpdate(move(tex));
				}
			} else {
				auto tex = cache->addExternalImage(texname,
						core::ImageInfo(Extent2(bitmap.width(), bitmap.height()),
								core::ImageFormat::R8_UNORM, core::ImageUsage::Sampled),
						bitmap.data());
				if (tex) {
					_curveSprite->scheduleTextureUpdate(move(tex));
				}
			}
		}
	}
}

} // namespace stappler::xenolith::app
