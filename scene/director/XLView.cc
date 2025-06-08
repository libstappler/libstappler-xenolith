/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#include "XLView.h"
#include "XLInputDispatcher.h"
#include "XLApplication.h"
#include "SPEventTimerHandle.h"
#include "XLPlatformViewInterface.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

XL_DECLARE_EVENT_CLASS(View, onFrameRate);

View::~View() { log::debug("xenolith::View", "~View"); }

bool View::init(Application &loop, ViewInfo &&info) {
	if (!ViewInterface::init(loop, *loop.getGlLoop())) {
		return false;
	}

	_info = move(info);
	return true;
}

void View::run() {
	if (!_presentationEngine->isRunning()) {
		_presentationEngine->run();
	}

	_presentationEngine->scheduleNextImage(
			[this](core::PresentationFrame *, bool success) { mapWindow(); });
}

void View::end() {
	if (!_presentationEngine) {
		return;
	}

	auto engine = move(_presentationEngine);
	_presentationEngine = nullptr;

	if (engine) {
		engine->end();
	}

	_application->performOnAppThread(
			[this, cb = sp::move(_info.onClosed), engine = move(engine)]() mutable {
		if (_director) {
			_director->end();
		}
		cb(*this);
		_loop->performOnThread([engine = move(engine)]() mutable {
#if SP_REF_DEBUG
			if (engine->getReferenceCount() > 1) {
				auto tmp = engine.get();
				engine = nullptr;

				tmp->foreachBacktrace(
						[](uint64_t id, Time time, const std::vector<std::string> &vec) {
					StringStream stream;
					stream << "[" << id << ":" << time.toHttp<Interface>() << "]:\n";
					for (auto &it : vec) { stream << "\t" << it << "\n"; }
					log::debug("core::PresentationEngine", stream.str());
				});
			}
			engine = nullptr;

			auto refcount = getReferenceCount();
			if (refcount > 1) {
				foreachBacktrace([](uint64_t id, Time time, const std::vector<std::string> &vec) {
					StringStream stream;
					stream << "[" << id << ":" << time.toHttp<Interface>() << "]:\n";
					for (auto &it : vec) { stream << "\t" << it << "\n"; }
					log::debug("View", stream.str());
				});
			}
#else
			engine = nullptr;
#endif
		}, this);
	}, this);
}

void View::runWithQueue(const Rc<RenderQueue> &queue) {
	if (!_presentationEngine->isRunning()) {
		run();
	}
}

void View::setPresentationEngine(Rc<core::PresentationEngine> &&e) {
	if (_presentationEngine) {
		log::error("View", "Presentation engine already defined");
		return;
	}

	_presentationEngine = move(e);

	auto c = _presentationEngine->getFrameConstraints();
	_director = Rc<Director>::create(_application.get_cast<Application>(), c, this);
	if (_info.onCreated) {
		_application->performOnAppThread([this, c] { _info.onCreated(*this, c); }, this);
	} else {
		run();
	}
}

Director *View::getDirector() const { return _director; }

core::ImageInfo View::getSwapchainImageInfo(const core::SwapchainConfig &cfg) const {
	core::ImageInfo swapchainImageInfo;
	swapchainImageInfo.format = cfg.imageFormat;
	swapchainImageInfo.flags = core::ImageFlags::None;
	swapchainImageInfo.imageType = core::ImageType::Image2D;
	swapchainImageInfo.extent = Extent3(cfg.extent.width, cfg.extent.height, 1);
	swapchainImageInfo.arrayLayers = core::ArrayLayers(1);
	swapchainImageInfo.usage = core::ImageUsage::ColorAttachment;
	if (cfg.transfer) {
		swapchainImageInfo.usage |= core::ImageUsage::TransferDst;
	}
	return swapchainImageInfo;
}

core::ImageViewInfo View::getSwapchainImageViewInfo(const core::ImageInfo &image) const {
	core::ImageViewInfo info;
	switch (image.imageType) {
	case core::ImageType::Image1D: info.type = core::ImageViewType::ImageView1D; break;
	case core::ImageType::Image2D: info.type = core::ImageViewType::ImageView2D; break;
	case core::ImageType::Image3D: info.type = core::ImageViewType::ImageView3D; break;
	}

	return image.getViewInfo(info);
}

core::SwapchainConfig View::selectConfig(const core::SurfaceInfo &surface) const {
	return _info.selectConfig(*this, surface);
}

SP_COVERAGE_TRIVIAL
Extent2 View::getExtent() const {
	if (_presentationEngine) {
		auto &e = _presentationEngine->getFrameConstraints().extent;
		return Extent2(e.width, e.height);
	} else {
		return Extent2(_info.window.rect.width, _info.window.rect.height);
	}
}

const WindowInfo &View::getWindowInfo() const { return _info.window; }

void View::retainBackButton() {
	performOnThread([this] { ++_backButtonCounter; }, this, true);
}

void View::releaseBackButton() {
	performOnThread([this] { --_backButtonCounter; }, this, true);
}

uint64_t View::getBackButtonCounter() const { return _backButtonCounter; }

void View::setDecorationTone(float) { }

void View::setDecorationVisible(bool) { }

void View::deprecateSwapchain() {
	performOnThread([this] {
		if (_presentationEngine) {
			_presentationEngine->deprecateSwapchain(false);
		}
	}, this, false);
}

void View::propagateInputEvent(core::InputEventData &event) {
	platform::ViewInterface::propagateInputEvent(event);
	_director->getInputDispatcher()->handleInputEvent(event);
}

void View::propagateTextInput(TextInputState &state) {
	platform::ViewInterface::propagateTextInput(state);
	_director->getTextInputManager()->handleInputUpdate(state);
}

} // namespace stappler::xenolith
