/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#include "XLCorePresentationEngine.h"
#include "XLCoreImageStorage.h"
#include "XLCorePresentationFrame.h"
#include "XLCoreSwapchain.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreDevice.h"
#include "SPEventLooper.h"
#include "SPEventTimerHandle.h"

#define XL_COREPRESENT_DEBUG 0

#ifndef XL_VKAPI_LOG
#define XL_VKAPI_LOG(...)
#endif

#if XL_COREPRESENT_DEBUG
#define XL_COREPRESENT_LOG(...) log::debug("core::PresentationEngine", __VA_ARGS__)
#else
#define XL_COREPRESENT_LOG(...)
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

bool PresentationEngine::isFrameValid(const PresentationFrame *frame) const {
	if (frame->getSwapchain() == _swapchain && !_swapchain->isDeprecated()) {
		return true;
	}
	return false;
}

Rc<FrameHandle> PresentationEngine::submitNextFrame(Rc<FrameRequest> &&req) {
	auto frame = _loop->makeFrame(move(req), 0);
	if (frame && frame->isValidFlag()) {
		frame->update(true);
		return frame;
	}
	return nullptr;
}

bool PresentationEngine::waitUntilFramePresentation() {
	if (!_loop->isOnThisThread()) {
		return false;
	}

	if (_waitUntilFrame) {
		return false;
	}

	_waitUntilFrame = true;
	_nextPresentWindow = 0;
	setReadyForNextFrame();
	auto ret = _loop->getLooper()->run();
	_waitUntilFrame = false;

	return ret == Status::Suspended;
}

void PresentationEngine::scheduleNextImage(Function<void(PresentationFrame *, bool)> &&cb,
		PresentationFrame::Flags frameFlags) {
	if (!_activeFrames.empty() || _swapchain->isDeprecated()) {
		return;
	}

	XL_COREPRESENT_LOG("scheduleNextImage");

	if (_options.renderImageOffscreen) {
		frameFlags = PresentationFrame::OffscreenTarget;
	} else {
		frameFlags = PresentationFrame::None;
	}

	scheduleSwapchainImage(Rc<PresentationFrame>::create(this, _constraints, _frameOrder,
			frameFlags, sp::move(cb)));

	_readyForNextFrame = false;
}

bool PresentationEngine::scheduleSwapchainImage(Rc<PresentationFrame> &&frame) {
	if (!frame) {
		return false;
	}

	XL_COREPRESENT_LOG("scheduleSwapchainImage");

	acquireFrameData(frame, [this](core::PresentationFrame *frame) mutable {
		if (isRunning() && frame->getSwapchain() == _swapchain) {
			XL_COREPRESENT_LOG("scheduleSwapchainImage: setup frame request");
			auto a = frame->setupOutputAttachment();
			if (!a) {
				if (frame->getRequest()->getQueue()) {
					log::error("core::PresentationEngine", "Fail to run view with queue '",
							frame->getRequest()->getQueue()->getName(),
							"': no usable output attachments found");
				}
				return;
			}

			frame->getRequest()->setOutput(a,
					[frame = Rc<PresentationFrame>(frame)](core::FrameAttachmentData &data,
							bool success, Ref *) mutable -> bool {
				if (!frame) {
					return true;
				}
				// Called in GL Thread
				XL_COREPRESENT_LOG("scheduleSwapchainImage: output on frame");
				if (data.image && success) {
					frame->assignResult(data.image);
					frame = nullptr;
					return false;
				} else {
					frame->invalidate(false);
				}
				frame = nullptr;
				return true;
			},
					this);

			XL_COREPRESENT_LOG("scheduleSwapchainImage: submit frame");

			auto nextFrame = frame->submitFrame();
			if (nextFrame) {
				// set to next suggested number
				_frameOrder = nextFrame->getOrder() + 1;

				_window->setFrameOrder(nextFrame->getOrder());
			}
		} else {
			log::error("core::PresentationEngine", "acquireFrameData - Swapchain was invalidated");
			frame->invalidate(frame->getSwapchain() != _swapchain);
		}
	});

	if (frame->getSwapchainImage()) {
		scheduleImage(frame);
	}

	return true;
}

void PresentationEngine::deprecateSwapchain(PresentationSwapchainFlags flags,
		Function<void(bool)> &&cb) {
	XL_COREPRESENT_LOG("deprecateSwapchain");
	if (!_running || !_swapchain) {
		return;
	}

	_swapchain->deprecate();

	_deprecationFlags |= flags;
	if (cb) {
		_deprecationCallbacks.emplace_back(sp::move(cb));
	}

	auto it = _scheduledForPresent.begin();
	while (it != _scheduledForPresent.end()) {
		runScheduledPresent(move(it->first), move(it->second));
		it = _scheduledForPresent.erase(it);
	}

	if (_acquisitionTimer) {
		_acquisitionTimer->cancel();
		_acquisitionTimer = nullptr;
	}

	auto acquiredImages = _swapchain->getAcquiredImagesCount();
	if (acquiredImages == 0) {
		scheduleSwapchainRecreation();
	}
}

PresentationEngine::~PresentationEngine() {
	log::debug("PresentationEngine", "~PresentationEngine");
}

bool PresentationEngine::init(NotNull<Loop> loop, NotNull<Device> device,
		NotNull<PresentationWindow> window) {
	_loop = loop;
	_device = device;
	_window = window;
	_originalSurface = _surface = _window->makeSurface(loop->getInstance());
	_constraints = _window->exportFrameConstraints();
	return true;
}

bool PresentationEngine::run() {
	_running = true;
	return isRunning();
}

void PresentationEngine::end() {
	_running = false;

	if (_acquisitionTimer) {
		_acquisitionTimer->cancel();
		_acquisitionTimer = nullptr;
	}

	Vector<Rc<Ref>> releaseList;

	auto activeFrames = std::move(_activeFrames);
	_activeFrames.clear();

	for (auto &it : activeFrames) {
		releaseList.emplace_back(it);
		it->invalidate();
	}

	auto totalFrames = std::move(_totalFrames);
	_totalFrames.clear();

	for (auto &it : totalFrames) {
		releaseList.emplace_back(it);
		it->invalidate();
	}

	releaseList.clear();

	auto framesAwaitingImages = sp::move(_framesAwaitingImages);
	auto scheduledForPresent = sp::move(_scheduledForPresent);
	auto scheduledPresentHandles = sp::move(_scheduledPresentHandles);

	for (auto &it : framesAwaitingImages) { it->invalidate(true); }
	for (auto &it : scheduledForPresent) {
		it.first->invalidate(true);
		it.second = nullptr;
	}
	for (auto &it : scheduledPresentHandles) { it->cancel(); }

	_framesAwaitingImages.clear();
	_scheduledForPresent.clear();
	_scheduledPresentHandles.clear();

	_swapchain = nullptr;
}

bool PresentationEngine::present(PresentationFrame *frame, ImageStorage *image) {
	XL_COREPRESENT_LOG("present");
	if (frame->hasFlag(PresentationFrame::DoNotPresent)) {
		frame->setPresented(Status::Done);
		return true;
	}

	if (image) {
		if (_options.followDisplayLink) {
			// schedule image for next DispayLink signal
			XL_COREPRESENT_LOG("schedulePresent: ", 0);
			_scheduledForPresent.emplace_back(frame, image);
			return true;
		}
		auto clock = sp::platform::clock(ClockType::Monotonic);
		if (!_options.usePresentWindow || !_nextPresentWindow
				|| _nextPresentWindow < clock + _engineUpdateInterval) {
			runScheduledPresent(frame, image);
		} else {
			auto frameTimeout = _nextPresentWindow - clock;
			XL_COREPRESENT_LOG("schedulePresent: ", frameTimeout);

			// schedule image until next present window
			auto handle = _loop->getLooper()->schedule(TimeInterval::microseconds(frameTimeout),
					[this, frame = Rc<PresentationFrame>(frame),
							image = Rc<ImageStorage>(image)](event::Handle *h, bool success) {
				if (success) {
					runScheduledPresent(frame, image);
				} else {
					frame->invalidate(false);
				}
				_scheduledPresentHandles.erase(h);
			},
					this);

			_scheduledPresentHandles.emplace(move(handle));
		}
	} else {
		if (!_options.renderImageOffscreen) {
			return true;
		}
		if (presentImmediate(frame)) {
			frame->setPresented(Status::ErrorCancelled);
		} else {
			frame->invalidate();
		}
		if (_swapchain->isDeprecated()) {
			scheduleSwapchainRecreation();
		}
	}
	return true;
}

void PresentationEngine::update(PresentationUpdateFlags flags) {
	if ((hasFlag(flags, PresentationUpdateFlags::DisplayLink) && _options.followDisplayLink)
			|| hasFlag(flags, PresentationUpdateFlags::FlushPending)) {
		// ignore present windows
		for (auto &it : _scheduledForPresent) {
			runScheduledPresent(move(it.first), move(it.second));
		}
		_scheduledForPresent.clear();
	}
}

void PresentationEngine::setTargetFrameInterval(uint64_t value) { _targetFrameInterval = value; }

void PresentationEngine::presentWithQueue(DeviceQueue &queue, NotNull<PresentationFrame> frame,
		ImageStorage *image) {
	XL_COREPRESENT_LOG("presentWithQueue: ", _activeFrames.size());
	auto clock = sp::platform::clock(ClockType::Monotonic);
	auto res = _swapchain->present(queue, image);
	auto dt = updatePresentationInterval();

	if (res == Status::Suboptimal || res == Status::ErrorCancelled) {
		XL_COREPRESENT_LOG("presentWithQueue - deprecate swapchain");
		_swapchain->deprecate();
	} else if (res != Status::Ok) {
		log::error("vk::View", "presentWithQueue: error:", res);
	}
	XL_COREPRESENT_LOG("presentWithQueue - presented");

	// read before frame marked as presented
	bool isCorrectable = frame->hasFlag(PresentationFrame::CorrectableFrame);

	frame->setPresented(res);

	if (_waitUntilFrame) {
		_loop->getLooper()->wakeup();
	}

	if (!_options.followDisplayLink && _targetFrameInterval) {
		// use clock before `present` call
		_nextPresentWindow = clock + _targetFrameInterval
				- _engineUpdateInterval; // allow one tick of `update`, that may be required for scheduling
	}

	if (!_running || (_swapchain->getAcquiredImagesCount() != 0 && !_activeFrames.empty())) {
		return;
	}

	if (_swapchain->isDeprecated() && _swapchain->getAcquiredImagesCount() == 0) {
		// perform on next stack frame
		scheduleSwapchainRecreation();
	} else if ((!_options.renderOnDemand || _readyForNextFrame) && _activeFrames.empty()) {
		if (_options.followDisplayLink) {
			// no need for a present window if we in DisplayLink mode
			XL_COREPRESENT_LOG("presentWithQueue - scheduleNextImage - followDisplayLink");
			scheduleNextImage();
		} else {
			if (_targetFrameInterval) {
				// adjust present window
				// if current or average framerate below target - reduce present window to release new frame early
				if (isCorrectable && dt.dt > _targetFrameInterval + _engineUpdateInterval) {
					_nextPresentWindow -= (dt.dt - _targetFrameInterval);
				}
			} else {
				_nextPresentWindow = 0;
			}

			XL_COREPRESENT_LOG("presentWithQueue - scheduleNextImage");
			scheduleNextImage(nullptr, PresentationFrame::CorrectableFrame);
		}
	}
}

PresentationEngine::FrameTimeInfo PresentationEngine::updatePresentationInterval() {
	FrameTimeInfo ret;
	ret.clock = sp::platform::clock(ClockType::Monotonic);
	ret.dt = ret.clock - _lastPresentationTime;
	_lastPresentationInterval = ret.dt;
	_avgPresentationInterval.addValue(ret.dt);
	_avgPresentationIntervalValue = _avgPresentationInterval.getAverage();
	_lastPresentationTime = ret.clock;
	ret.avg = _avgPresentationIntervalValue.load();
	return ret;
}

uint64_t PresentationEngine::getFrameOrder() const { return _frameOrder; }

uint64_t PresentationEngine::getLastFrameInterval() const { return _lastPresentationInterval; }

uint64_t PresentationEngine::getAvgFrameInterval() const { return _avgPresentationIntervalValue; }

uint64_t PresentationEngine::getLastFrameTime() const { return _lastFrameTime; }

uint64_t PresentationEngine::getLastFenceFrameTime() const { return _lastFenceFrameTime; }

uint64_t PresentationEngine::getLastTimestampFrameTime() const { return _lastTimestampFrameTime; }

void PresentationEngine::setReadyForNextFrame() {
	// if we not in on-demand mode - ignore
	if (!_options.renderOnDemand) {
		_readyForNextFrame = false;
		return;
	}

	if (!_readyForNextFrame) {
		// spawn frame if there is none
		if (_swapchain && _swapchain->getAcquiredImagesCount() == 0 && _activeFrames.empty()) {
			XL_COREPRESENT_LOG("setReadyForNextFrame - scheduleNextImage");
			scheduleNextImage();
		} else {
			// or mark for update
			_readyForNextFrame = true;
		}
	}
}

void PresentationEngine::setRenderOnDemand(bool value) { _options.renderOnDemand = value; }

bool PresentationEngine::isRenderOnDemand() const { return _options.renderOnDemand; }

bool PresentationEngine::isRunning() const {
	return _running && _swapchain && !_swapchain->isDeprecated();
}

void PresentationEngine::setContentPadding(const Padding &padding) {
	_constraints.contentPadding = padding;
	setReadyForNextFrame();
}

bool PresentationEngine::handleFrameStarted(NotNull<PresentationFrame> frame) {
	XL_COREPRESENT_LOG(frame->getFrameOrder(), ": handleFrameStarted");
	if (frame->hasFlag(PresentationFrame::DoNotPresent)) {
		return _detachedFrames.emplace(frame).second;
	} else {
		_totalFrames.emplace(frame);
		return _activeFrames.emplace(frame).second;
	}
}

void PresentationEngine::handleFrameInvalidated(NotNull<PresentationFrame> frame) {
	XL_COREPRESENT_LOG(frame->getFrameOrder(), ": handleFrameInvalidated");
	if (frame->hasFlag(PresentationFrame::DoNotPresent)) {
		_detachedFrames.erase(frame);
		return;
	}

	_activeFrames.erase(frame);
	_totalFrames.erase(frame);

	if (_swapchain->isDeprecated() && _swapchain->getAcquiredImagesCount() == 0) {
		// perform on next stack frame
		scheduleSwapchainRecreation();
	} else {
		acquireScheduledImage();
	}
}

void PresentationEngine::handleFrameReady(NotNull<PresentationFrame> frame) {
	XL_COREPRESENT_LOG(frame->getFrameOrder(), ": handleFrameReady");
	if (_options.earlyPresent) {
		present(frame, frame->getSwapchainImage());
	} else if (_options.preStartFrame) {
		if (!frame->hasFlag(PresentationFrame::DoNotPresent)) {
			_activeFrames.erase(frame);
			if ((!_options.renderOnDemand || _readyForNextFrame) && _activeFrames.empty()) {
				scheduleNextImage();
			}
		}
	}
}

void PresentationEngine::handleFramePresented(NotNull<PresentationFrame> frame) {
	XL_COREPRESENT_LOG(frame->getFrameOrder(), ": handleFramePresented");

	if (!frame->hasFlag(PresentationFrame::DoNotPresent)) {
		_window->handleFramePresented(frame);
		_activeFrames.erase(frame);
	}

	if (!_options.earlyPresent) {
		if (frame->hasFlag(PresentationFrame::DoNotPresent)) {
			_detachedFrames.erase(frame);
		} else {
			_totalFrames.erase(frame);
			if (!_framesAwaitingImages.empty()) {
				acquireScheduledImage();
			}
		}
	}
}

void PresentationEngine::handleFrameComplete(NotNull<PresentationFrame> frame) {
	XL_COREPRESENT_LOG(frame->getFrameOrder(), ": handleFrameCancel");
	if (frame->hasFlag(PresentationFrame::DoNotPresent)) {
		_detachedFrames.erase(frame);
		return;
	}
	if (auto h = frame->getHandle()) {
		_lastFrameTime = h->getTimeEnd() - h->getTimeStart();
		_avgFrameTime.addValue(_lastFrameTime);
		_avgFrameTimeValue = _avgFrameTime.getAverage();

		if (auto t = h->getSubmissionTime()) {
			_lastFenceFrameTime = t;
			_avgFenceInterval.addValue(t);
			_avgFenceIntervalValue = _avgFenceInterval.getAverage();
		}
		if (auto t = h->getDeviceTime()) {
			_lastTimestampFrameTime = t;
			_avgTimestampInterval.addValue(t);
			_avgTimestampIntervalValue = _avgTimestampInterval.getAverage();
		}
	}
	if (!_options.earlyPresent && frame->hasFlag(PresentationFrame::ImageRendered)) {
		_loop->performOnThread(
				[this, image = Rc<SwapchainImage>(frame->getSwapchainImage()),
						frame = Rc<PresentationFrame>(frame)] { present(frame, image); },
				this, false);
	} else {
		_totalFrames.erase(frame);
		if (_swapchain->isDeprecated() && _swapchain->getAcquiredImagesCount() == 0) {
			// perform on next stack frame
			scheduleSwapchainRecreation();
		} else if ((!_options.renderOnDemand || _readyForNextFrame) && _activeFrames.empty()) {
			scheduleNextImage();
		} else if (!_framesAwaitingImages.empty()) {
			acquireScheduledImage();
		}
	}
}

void PresentationEngine::captureScreenshot(
		Function<void(const ImageInfoData &info, BytesView view)> &&cb) {

	scheduleSwapchainImage(Rc<PresentationFrame>::create(this, _constraints, _frameOrder,
			PresentationFrame::OffscreenTarget | PresentationFrame::DoNotPresent,
			[this, cb = sp::move(cb)](PresentationFrame *frame, bool success) mutable {
		auto target = frame->getTarget();
		_loop->captureImage(sp::move(cb), target->getImage(), target->getLayout());
	}));
}

void PresentationEngine::acquireFrameData(NotNull<PresentationFrame> frame,
		Function<void(NotNull<PresentationFrame>)> &&cb) {
	_window->acquireFrameData(frame, sp::move(cb));
}

void PresentationEngine::scheduleSwapchainRecreation() {
	if (_swapchain && _swapchain->getPresentedFramesCount() == 0) {
		log::warn("core::PresentationEngine",
				"Scheduling swapchain recreation without frame presentation");
	}
	_loop->performOnThread([this] { recreateSwapchain(); }, this, false);
}

void PresentationEngine::resetFrames() {
	auto frames = _activeFrames;
	for (auto &it : frames) { it->invalidate(true); }

	frames = _totalFrames;
	for (auto &it : frames) { it->invalidate(true); }

	frames = _detachedFrames;
	for (auto &it : frames) { it->invalidate(true); }

	for (auto &it : _scheduledPresentHandles) { it->cancel(); }
	_scheduledPresentHandles.clear();

	_framesAwaitingImages.clear();
	_scheduledForPresent.clear();
	_requestedSwapchainImage.clear();
	_acquiredSwapchainImages.clear();
}

void PresentationEngine::scheduleImage(NotNull<PresentationFrame> frame) {
	XL_COREPRESENT_LOG("scheduleImage");
	if (!_acquiredSwapchainImages.empty()) {
		// pop one of the previously acquired images
		auto acquiredImage = _acquiredSwapchainImages.front();
		_acquiredSwapchainImages.pop_front();
		frame->assignSwapchainImage(acquiredImage);
	} else {
		_framesAwaitingImages.emplace_back(frame);
		acquireScheduledImage();
	}
}

Status PresentationEngine::acquireScheduledImage() {
	if (!_requestedSwapchainImage.empty() || _framesAwaitingImages.empty()
			|| _totalFrames.size() != _activeFrames.size()) {
		XL_COREPRESENT_LOG("acquireScheduledImage - dropped");
		return Status::Declined;
	}

	Status status;

	XL_COREPRESENT_LOG("acquireScheduledImage");
	auto loop = (Loop *)_loop.get();
	auto fence = loop->acquireFence(FenceType::Swapchain);
	if (auto acquiredImage = _swapchain->acquire(true, fence, status)) {
		_requestedSwapchainImage.emplace(acquiredImage);
		XL_COREPRESENT_LOG("acquireScheduledImage - spawn request: ",
				_requestedSwapchainImage.size());
		fence->addRelease([this, f = fence.get(), acquiredImage](bool success) mutable {
			if (success) {
				handleSwapchainImageReady(move(acquiredImage));
			} else {
				_requestedSwapchainImage.erase(acquiredImage);
			}
			XL_COREPRESENT_LOG("[", f->getFrame(), "] acquireScheduledImage [complete]", " [",
					sp::platform::clock(ClockType::Monotonic) - f->getArmedTime(), "]");
		}, this, "PresentationEngine::acquireScheduledImage");
		fence->schedule(*loop);
		return Status::Ok;
	} else {
		XL_COREPRESENT_LOG("acquireScheduledImage - failed");
		fence->schedule(*loop);
		if (status == Status::Timeout) {
			// schedule timed waiter
			scheduleImageAcquisition();
		}
		return status;
	}
}

void PresentationEngine::scheduleImageAcquisition() {
	_acquisitionTimer = _loop->getLooper()->scheduleTimer(event::TimerInfo{
		.completion = event::CompletionHandle<event::TimerHandle>::create<PresentationEngine>(this,
				[](PresentationEngine *e, event::TimerHandle *h, uint32_t, Status st) {
		if (st == Status::Ok && e->acquireScheduledImage() != Status::Timeout) {
			h->cancel();
			if (e->_acquisitionTimer == h) {
				e->_acquisitionTimer = nullptr;
			}
		}
	}),
		.interval = config::PresentationSchedulerInterval,
		.count = event::TimerInfo::Infinite,
	});
}

void PresentationEngine::handleSwapchainImageReady(Rc<Swapchain::SwapchainAcquiredImage> &&image) {
	XL_COREPRESENT_LOG("onSwapchainImageReady: ", _framesAwaitingImages.size());
	auto ptr = image.get();

	if (!_framesAwaitingImages.empty()) {
		// send new swapchain image to framebuffer
		auto target = _framesAwaitingImages.front();
		_framesAwaitingImages.pop_front();

		target->assignSwapchainImage(image);
	} else {
		// hold image until next framebuffer request, if not active queries
		_acquiredSwapchainImages.emplace_back(move(image));
	}

	_requestedSwapchainImage.erase(ptr);

	if (!_framesAwaitingImages.empty()) {
		// run next image query if someone waits for it
		acquireScheduledImage();
	}
}

void PresentationEngine::runScheduledPresent(NotNull<PresentationFrame> frame,
		ImageStorage *image) {
	XL_COREPRESENT_LOG("runScheduledPresent");

	if (!_loop->isRunning() || frame->hasFlag(PresentationFrame::Invalidated)) {
		return;
	}
	auto queue = _device->tryAcquireQueue(QueueFlags::Present);
	if (queue) {
		presentSwapchainImage(move(queue), frame, image);
	} else {
		_device->acquireQueue(QueueFlags::Present, *_loop,
				[this, frame = Rc<PresentationFrame>(frame), image = Rc<ImageStorage>(image)](
						Loop &, const Rc<DeviceQueue> &queue) mutable {
			presentSwapchainImage(Rc<DeviceQueue>(queue), frame, image);
		},
				[frame = Rc<PresentationFrame>(frame)](Loop &) { frame->invalidate(); }, this);
	}
}

void PresentationEngine::presentSwapchainImage(Rc<DeviceQueue> &&queue,
		NotNull<PresentationFrame> frame, ImageStorage *image) {
	XL_COREPRESENT_LOG("presentSwapchainImage");
	if (frame->getSwapchain() == _swapchain && frame->getSwapchainImage()->isSubmitted()) {
		presentWithQueue(*queue, frame, image);
	}
	_device->releaseQueue(move(queue));
}

} // namespace stappler::xenolith::core
