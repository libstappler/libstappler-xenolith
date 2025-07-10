/**
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

#include "XLAppWindow.h"
#include "XLAppThread.h"
#include "XLCorePresentationEngine.h"
#include "platform/XLContextNativeWindow.h"
#include "director/XLDirector.h"
#include "input/XLInputDispatcher.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

XL_DECLARE_EVENT_CLASS(AppWindow, onBackground);
XL_DECLARE_EVENT_CLASS(AppWindow, onFocus);

bool AppWindow::init(NotNull<Context> ctx, NotNull<AppThread> app, NotNull<NativeWindow> w) {
	_context = ctx;
	_application = app;
	_window = w;

	_presentationEngine = _context->getGlLoop()->makePresentationEngine(this);

	return _presentationEngine != nullptr;
}

void AppWindow::runWithQueue(const Rc<core::Queue> &queue) {
	if (!_presentationEngine->isRunning()) {
		run();
	}
}

void AppWindow::run() {
	if (!_presentationEngine->isRunning()) {
		_presentationEngine->run();
	}

	_presentationEngine->scheduleNextImage(
			[this](core::PresentationFrame *, bool success) { _window->mapWindow(); });
}

void AppWindow::update(bool displayLink) { _presentationEngine->update(displayLink); }

void AppWindow::end() {
	if (!_presentationEngine) {
		return;
	}

	auto engine = move(_presentationEngine);
	_presentationEngine = nullptr;

	if (engine) {
		engine->end();
	}

	_application->performOnAppThread([this, engine = move(engine)]() mutable {
		if (_director) {
			_director->end();
		}
		_context->handleAppWindowDestroyed(this);
		_context->performOnThread([engine = move(engine)]() mutable {
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

void AppWindow::close(bool graceful) {
	if (!graceful) {
		end();
	} else {
		_presentationEngine->deprecateSwapchain(core::PresentationEngine::SwapchainFlags::EndOfLife,
				[this](bool) { _context->performOnThread([this] { end(); }, this, false); });
	}
}

void AppWindow::setPresentationEngine(Rc<core::PresentationEngine> &&e) {
	if (_presentationEngine) {
		log::error("View", "Presentation engine already defined");
		return;
	}

	_presentationEngine = move(e);

	auto c = _presentationEngine->getFrameConstraints();
	_director = Rc<Director>::create(_application.get_cast<AppThread>(), c, this);

	_context->handleAppWindowCreated(this, c);
}

void AppWindow::handleInputEvents(Vector<InputEventData> &&events) {
	if (!_presentationEngine) {
		return;
	}

	_application->performOnAppThread([this, events = sp::move(events)]() mutable {
		for (auto &event : events) { propagateInputEvent(event); }
	}, this, true);
	setReadyForNextFrame();
}

const WindowInfo *AppWindow::getInfo() const {
	if (_window) {
		return _window->getInfo();
	}
	return nullptr;
}

core::ImageInfo AppWindow::getSwapchainImageInfo(const core::SwapchainConfig &cfg) const {
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

core::SurfaceInfo AppWindow::getSurfaceOptions(core::SurfaceInfo &&info) const {
	if (_window) {
		return _window->getSurfaceOptions(move(info));
	}
	return move(info);
}

core::ImageViewInfo AppWindow::getSwapchainImageViewInfo(const core::ImageInfo &image) const {
	core::ImageViewInfo info;
	switch (image.imageType) {
	case core::ImageType::Image1D: info.type = core::ImageViewType::ImageView1D; break;
	case core::ImageType::Image2D: info.type = core::ImageViewType::ImageView2D; break;
	case core::ImageType::Image3D: info.type = core::ImageViewType::ImageView3D; break;
	}

	return image.getViewInfo(info);
}

core::SwapchainConfig AppWindow::selectConfig(const core::SurfaceInfo &cfg) {
	return _context->handleAppWindowSurfaceUpdate(this, cfg);
}

void AppWindow::acquireFrameData(core::PresentationFrame *frame,
		Function<void(core::PresentationFrame *)> &&cb) {
	_application->performOnAppThread(
			[this, frame = Rc<core::PresentationFrame>(frame), cb = sp::move(cb),
					req = Rc<core::FrameRequest>(frame->getRequest())]() mutable {
		if (_director->acquireFrame(req)) {
			_context->performOnThread(
					[frame = move(frame), cb = sp::move(cb)]() mutable { cb(frame); }, this);
		}
	},
			this);
}

void AppWindow::handleFramePresented(core::PresentationFrame *frame) {
	_window->handleFramePresented(frame);
}

Rc<core::Surface> AppWindow::makeSurface(NotNull<core::Instance> instance) {
	return _window->makeSurface(instance);
}

core::FrameConstraints AppWindow::getInitialFrameConstraints() const {
	return _window->exportConstraints(_window->getInfo()->exportConstraints());
}

uint64_t AppWindow::getInitialFrameInterval() const { return _window->getScreenFrameInterval(); }

void AppWindow::updateConfig() {
	_context->performOnThread([this] { _presentationEngine->deprecateSwapchain(); }, this, true);
}

void AppWindow::setReadyForNextFrame() {
	_context->performOnThread([this] {
		if (_presentationEngine) {
			_presentationEngine->setReadyForNextFrame();
		}
	}, this, true);
}

void AppWindow::setRenderOnDemand(bool value) {
	_context->performOnThread([this, value] {
		if (_presentationEngine) {
			_presentationEngine->setRenderOnDemand(value);
		}
	}, this, true);
}

bool AppWindow::isRenderOnDemand() const {
	return _presentationEngine ? _presentationEngine->isRenderOnDemand() : false;
}

void AppWindow::setFrameInterval(uint64_t value) {
	_context->performOnThread([this, value] {
		if (_presentationEngine) {
			_presentationEngine->setTargetFrameInterval(value);
		}
	}, this, true);
}

uint64_t AppWindow::getFrameInterval() const {
	return _presentationEngine ? _presentationEngine->getTargetFrameInterval() : 0;
}

void AppWindow::setInsetDecoration(const Padding &padding) {
	_context->performOnThread([this, padding] { _presentationEngine->setContentPadding(padding); },
			this, true);
}

void AppWindow::waitUntilFrame() {
	if (_presentationEngine) {
		_presentationEngine->waitUntilFramePresentation();
	}
}

void AppWindow::retainExitGuard() {
	_context->performOnThread([this]() {
		if (_exitGuard == 0) {
			_window->setExitGuard(true);
		}
		++_exitGuard;
	}, this);
}
void AppWindow::releaseExitGuard() {
	_context->performOnThread([this]() {
		SPASSERT(_exitGuard > 0, "Exit guard should be retained");
		--_exitGuard;
		if (_exitGuard == 0) {
			_window->setExitGuard(false);
		}
	}, this);
}

void AppWindow::setInsetDecorationVisible(bool val) {
	_insetDecorationVisible = val;

	_context->performOnThread([this, val]() {
		if (_window) {
			_window->setInsetDecorationVisible(val);
		}
	}, this);
}

bool AppWindow::isInsetDecorationVisible() const { return _insetDecorationVisible; }

void AppWindow::setInsetDecorationTone(float val) {
	_insetDecorationVisible = val;

	_context->performOnThread([this, val]() {
		if (_window) {
			_window->setInsetDecorationTone(val);
		}
	}, this);
}

float AppWindow::getInsetDecorationTone() const { return _insetDecorationTone; }

void AppWindow::acquireTextInput(TextInputRequest &&req) {
	_context->performOnThread([this, data = move(req)]() { _window->acquireTextInput(data); },
			this);
}

void AppWindow::releaseTextInput() {
	_context->performOnThread([this]() { _window->releaseTextInput(); }, this);
}

void AppWindow::updateLayers(Vector<WindowLayer> &&layers) {
	_context->performOnThread([this, layers = sp::move(layers)]() mutable {
		_window->updateLayers(sp::move(layers));
	}, this);
}

void AppWindow::propagateInputEvent(core::InputEventData &event) {
	if (event.isPointEvent()) {
		event.point.density =
				_presentationEngine ? _presentationEngine->getFrameConstraints().density : 1.0f;
	}

	switch (event.event) {
	case InputEventName::Background:
		_inBackground = event.getValue();
		onBackground(this, _inBackground);
		break;
	case InputEventName::PointerEnter: _pointerInWindow = event.getValue(); break;
	case InputEventName::FocusGain:
		_hasFocus = event.getValue();
		onFocus(this, _hasFocus);
		break;
	default: break;
	}

	_director->getInputDispatcher()->handleInputEvent(event);
}

void AppWindow::propagateTextInput(TextInputState &state) {
	_director->getTextInputManager()->handleInputUpdate(state);
}

} // namespace stappler::xenolith
