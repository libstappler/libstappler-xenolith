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
#include "SPCore.h"
#include "XLAppThread.h"
#include "XLContextInfo.h"
#include "XLCoreEnum.h"
#include "XLCorePresentationEngine.h"
#include "XlCoreMonitorInfo.h"
#include "platform/XLContextNativeWindow.h"
#include "director/XLDirector.h"
#include "input/XLInputDispatcher.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

XL_DECLARE_EVENT_CLASS(AppWindow, onBackground);
XL_DECLARE_EVENT_CLASS(AppWindow, onFocus);
XL_DECLARE_EVENT_CLASS(AppWindow, onFullscreen);

AppWindow::~AppWindow() { log::info("AppWindow", "~AppWindow"); }

bool AppWindow::init(NotNull<Context> ctx, NotNull<AppThread> app, NotNull<NativeWindow> w) {
	_context = ctx;
	_application = app;
	_window = w;

	_isFullscreen = w->getInfo()->fullscreen != FullscreenInfo::None;

	_presentationEngine =
			_context->getGlLoop()->makePresentationEngine(this, w->getPreferredOptions());

	auto c = _presentationEngine->getFrameConstraints();

	_director = Rc<Director>::create(_application.get_cast<AppThread>(), c, this);

	return _presentationEngine != nullptr;
}

void AppWindow::runWithQueue(const Rc<core::Queue> &queue) {
	if (!_presentationEngine->isRunning()) {
		if (!_presentationEngine->isRunning()) {
			_presentationEngine->run();
		}

		_presentationEngine->scheduleNextImage([this](core::PresentationFrame *, bool success) {
			// Map windows after frame was rendered
			_window->mapWindow();
		});
	}
}

void AppWindow::run() {
	_application->performOnAppThread([this, director = _director]() mutable {
		_context->handleAppWindowCreated(_application, this, director);
	}, this);
}

void AppWindow::update(core::PresentationUpdateFlags flags) { _presentationEngine->update(flags); }

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
		_context->handleAppWindowDestroyed(_application, this);
		_context->performOnThread([engine = move(engine)]() mutable { engine = nullptr; }, this);
		_director = nullptr;
	}, this);
}

void AppWindow::close(bool graceful) {
	if (_inCloseRequest) {
		return;
	}

	_inCloseRequest = true;
	_context->performOnThread([this, w = _window, graceful] {
		if (w) {
			if (!w->close()) {
				_application->performOnAppThread([this] { _inCloseRequest = false; }, this);
				return;
			}
		}

		if (!graceful) {
			end();
		} else {
			_presentationEngine->deprecateSwapchain(core::PresentationSwapchainFlags::EndOfLife,
					[this](bool) {
				// successful stop
				end();
			});
		}
	}, this);
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

void AppWindow::handleTextInput(const TextInputState &state) {
	if (!_presentationEngine) {
		return;
	}

	_application->performOnAppThread([this, state = state]() mutable { propagateTextInput(state); },
			this, true);
	setReadyForNextFrame();
}

const WindowInfo *AppWindow::getInfo() const {
	if (_window) {
		return _window->getInfo();
	}
	return nullptr;
}

WindowCapabilities AppWindow::getCapabilities() const {
	if (_window) {
		return _window->getInfo()->capabilities;
	}
	return WindowCapabilities::None;
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

core::SwapchainConfig AppWindow::selectConfig(const core::SurfaceInfo &cfg, bool fastMode) {
	auto c = _context->handleAppWindowSurfaceUpdate(this, cfg, fastMode);
	// preserve selected config for app thread
	_application->performOnAppThread([this, c, fastMode] {
		_appSwapchainConfig = c;
		if (fastMode && _appSwapchainConfig.presentModeFast != core::PresentMode::Unsupported) {
			_appSwapchainConfig.presentMode = _appSwapchainConfig.presentModeFast;
		}
	}, this);
	return c;
}

void AppWindow::acquireFrameData(NotNull<core::PresentationFrame> frame,
		Function<void(NotNull<core::PresentationFrame>)> &&cb) {
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

void AppWindow::handleFramePresented(NotNull<core::PresentationFrame> frame) {
	_window->handleFramePresented(frame);
}

Rc<core::Surface> AppWindow::makeSurface(NotNull<core::Instance> instance) {
	return _window->makeSurface(instance);
}

core::FrameConstraints AppWindow::exportFrameConstraints() const {
	auto c = _window->getInfo()->exportConstraints();
	c.extent = _window->getExtent();
	return _window->exportConstraints(sp::move(c));
}

void AppWindow::setFrameOrder(uint64_t frameOrder) {
	if (_window) {
		_window->setFrameOrder(frameOrder);
	}
}

void AppWindow::updateConstraints(core::PresentationSwapchainFlags flags) {
	_context->performOnThread([this, flags] {
		if (_presentationEngine) {
			_presentationEngine->deprecateSwapchain(flags);
		}
	}, this, true);
}

void AppWindow::setReadyForNextFrame() {
	_context->performOnThread([this] {
		if (_presentationEngine) {
			_presentationEngine->setReadyForNextFrame();
		}
	}, this, true);
}

void AppWindow::waitUntilFrame() {
	if (_presentationEngine) {
		_presentationEngine->waitUntilFramePresentation();
	}
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

void AppWindow::setInsetDecoration(const Padding &padding) {
	_context->performOnThread([this, padding] { _presentationEngine->setContentPadding(padding); },
			this, true);
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

void AppWindow::acquireScreenInfo(Function<void(NotNull<ScreenInfo>)> &&cb, Ref *ref) {
	_context->performOnThread([this, cb = sp::move(cb), ref = Rc<Ref>(ref)]() mutable {
		auto winfo = _window->getInfo();
		if (hasFlag(winfo->capabilities, WindowCapabilities::DirectOutput)
				&& hasFlag(winfo->flags, WindowFlags::DirectOutput)) {
			auto info = _presentationEngine->getScreenInfo();
			_application->performOnAppThread(
					[cb = sp::move(cb), ref = move(ref), info = move(info)]() mutable {
				cb(info);
				ref = nullptr;
				info = nullptr;
			}, this);
		} else {
			_application->acquireScreenInfo(sp::move(cb), ref);
		}
	}, this);
}

bool AppWindow::setFullscreen(FullscreenInfo &&info, Function<void(Status)> &&cb, Ref *ref) {
	if (!hasFlag(getCapabilities(), WindowCapabilities::Fullscreen)) {
		return false;
	}
	_context->performOnThread(
			[this, info = move(info), cb = sp::move(cb), ref = Rc<Ref>(ref)]() mutable {
		auto winfo = _window->getInfo();
		auto useDirect = hasFlag(winfo->capabilities, WindowCapabilities::DirectOutput)
				&& hasFlag(winfo->flags, WindowFlags::DirectOutput);
		if (useDirect) {
			auto st = _presentationEngine->setFullscreenSurface(info.id, info.mode);
			_application->performOnAppThread([st, cb = sp::move(cb), ref = move(ref)]() mutable {
				cb(st);
				ref = nullptr;
			}, this);
		} else {
			_window->setFullscreen(move(info),
					[this, cb = sp::move(cb), ref = move(ref)](Status st) mutable {
				_application->performOnAppThread(
						[st, cb = sp::move(cb), ref = move(ref)]() mutable {
					cb(st);
					ref = nullptr;
				}, this);
			}, this);
		}
	}, this);
	return true;
}

void AppWindow::captureScreenshot(
		Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) {
	_context->performOnThread([this, cb = sp::move(cb)]() mutable {
		_presentationEngine->captureScreenshot(sp::move(cb));
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
	case InputEventName::Fullscreen:
		_isFullscreen = event.getValue();
		onFullscreen(this, _isFullscreen);
		break;
	default: break;
	}

	_director->getInputDispatcher()->handleInputEvent(event);
}

void AppWindow::propagateTextInput(TextInputState &state) {
	_director->getTextInputManager()->handleInputUpdate(state);
}

} // namespace stappler::xenolith
