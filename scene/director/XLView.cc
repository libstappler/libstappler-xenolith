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

#include "XLView.h"
#include "XLInputDispatcher.h"

namespace stappler::xenolith {

XL_DECLARE_EVENT_CLASS(View, onFrameRate);
XL_DECLARE_EVENT_CLASS(View, onBackground);
XL_DECLARE_EVENT_CLASS(View, onFocus);

View::View() { }

View::~View() { }

bool View::init(Application &loop, ViewInfo &&info) {
	_mainLoop = &loop;
	_glLoop = _mainLoop->getGlLoop();
	_constraints.extent = Extent2(info.rect.width, info.rect.height);
	_constraints.density = 1.0f;
	if (info.density != 0.0f) {
		_constraints.density = info.density;
	}
	_constraints.contentPadding = info.decoration;
	_frameEmitter = Rc<FrameEmitter>::create(_glLoop, info.frameInterval);
	_info = move(info);

	log::debug("View", "init");

	return true;
}

void View::end() {
	_running = false;
	_frameEmitter->invalidate();
	_mainLoop->performOnMainThread([this, cb = move(_info.onClosed)] () {
		if (_director) {
			_director->end();
		}
		cb(*this);
	}, this);
}

void View::update(bool displayLink) {
	Vector<Pair<Function<void()>, Rc<Ref>>> callback;

	_mutex.lock();
	callback = move(_callbacks);
	_callbacks.clear();
	_mutex.unlock();

	for (auto &it : callback) {
		it.first();
	}
}

void View::close() {
	_shouldQuit.clear();
}

void View::performOnThread(Function<void()> &&func, Ref *target, bool immediate) {
	if (immediate && std::this_thread::get_id() == _threadId) {
		func();
	} else {
		std::unique_lock<Mutex> lock(_mutex);
		if (_running) {
			_callbacks.emplace_back(move(func), target);
			wakeup();
		} else if (!_init) {
			_callbacks.emplace_back(move(func), target);
		}
	}
}

const Rc<Director> &View::getDirector() const {
	return _director;
}

void View::handleInputEvent(const InputEventData &event) {
	_mainLoop->performOnMainThread([this, event = event] () mutable {
		if (event.isPointEvent()) {
			event.point.density = _constraints.density;
		}

		switch (event.event) {
		case InputEventName::Background:
			_inBackground = event.getValue();
			onBackground(this, _inBackground);
			break;
		case InputEventName::PointerEnter:
			_pointerInWindow = event.getValue();
			break;
		case InputEventName::FocusGain:
			_hasFocus = event.getValue();
			onFocus(this, _hasFocus);
			break;
		default:
			break;
		}
		_director->getInputDispatcher()->handleInputEvent(event);
	}, this);
	setReadyForNextFrame();
}

void View::handleInputEvents(Vector<InputEventData> &&events) {
	_mainLoop->performOnMainThread([this, events = move(events)] () mutable {
		for (auto &event : events) {
			if (event.isPointEvent()) {
				event.point.density = _constraints.density;
			}

			switch (event.event) {
			case InputEventName::Background:
				_inBackground = event.getValue();
				onBackground(this, _inBackground);
				break;
			case InputEventName::PointerEnter:
				_pointerInWindow = event.getValue();
				break;
			case InputEventName::FocusGain:
				_hasFocus = event.getValue();
				onFocus(this, _hasFocus);
				break;
			default:
				break;
			}
			_director->getInputDispatcher()->handleInputEvent(event);
		}
	}, this, true);
	setReadyForNextFrame();
}

core::ImageInfo View::getSwapchainImageInfo() const {
	return getSwapchainImageInfo(_config);
}

core::ImageInfo View::getSwapchainImageInfo(const core::SwapchainConfig &cfg) const {
	core::ImageInfo swapchainImageInfo;
	swapchainImageInfo.format = cfg.imageFormat;
	swapchainImageInfo.flags = core::ImageFlags::None;
	swapchainImageInfo.imageType = core::ImageType::Image2D;
	swapchainImageInfo.extent = Extent3(cfg.extent.width, cfg.extent.height, 1);
	swapchainImageInfo.arrayLayers = core::ArrayLayers( 1 );
	swapchainImageInfo.usage = core::ImageUsage::ColorAttachment;
	if (cfg.transfer) {
		swapchainImageInfo.usage |= core::ImageUsage::TransferDst;
	}
	return swapchainImageInfo;
}

core::ImageViewInfo View::getSwapchainImageViewInfo(const core::ImageInfo &image) const {
	core::ImageViewInfo info;
	switch (image.imageType) {
	case core::ImageType::Image1D:
		info.type = core::ImageViewType::ImageView1D;
		break;
	case core::ImageType::Image2D:
		info.type = core::ImageViewType::ImageView2D;
		break;
	case core::ImageType::Image3D:
		info.type = core::ImageViewType::ImageView3D;
		break;
	}

	return image.getViewInfo(info);
}

uint64_t View::getLastFrameInterval() const {
	return _lastFrameInterval;
}
uint64_t View::getAvgFrameInterval() const {
	return _avgFrameIntervalValue;
}

uint64_t View::getLastFrameTime() const {
	return _frameEmitter->getLastFrameTime();
}
uint64_t View::getAvgFrameTime() const {
	return _frameEmitter->getAvgFrameTime();
}

uint64_t View::getAvgFenceTime() const {
	return _frameEmitter->getAvgFenceTime();
}

uint64_t View::getFrameInterval() const {
	std::unique_lock<Mutex> lock(_frameIntervalMutex);
	return _info.frameInterval;
}

void View::setFrameInterval(uint64_t value) {
	performOnThread([this, value] {
		std::unique_lock<Mutex> lock(_frameIntervalMutex);
		_info.frameInterval = value;
		onFrameRate(this, int64_t(_info.frameInterval));
	}, this, true);
}

void View::setReadyForNextFrame() {

}

void View::retainBackButton() {
	performOnThread([this] {
		++ _backButtonCounter;
	}, this, true);
}

void View::releaseBackButton() {
	performOnThread([this] {
		-- _backButtonCounter;
	}, this, true);
}

void View::setDecorationTone(float) {

}

void View::setDecorationVisible(bool) {

}

uint64_t View::retainView() {
	return retain();
}

void View::releaseView(uint64_t id) {
	release(id);
}

void View::setContentPadding(const Padding &padding) {
	_constraints.contentPadding = padding;
	setReadyForNextFrame();
}

}
