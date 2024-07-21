/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLVkView.h"
#include "XLVkLoop.h"
#include "XLVkDevice.h"
#include "XLVkTextureSet.h"
#include "XLVkSwapchain.h"
#include "XLVkGuiConfig.h"
#include "XLDirector.h"
#include "XLCoreImageStorage.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameCache.h"
#include "SPBitmap.h"

#define XL_VKVIEW_DEBUG 1

#ifndef XL_VKAPI_LOG
#define XL_VKAPI_LOG(...)
#endif

#if XL_VKVIEW_DEBUG
#define XL_VKVIEW_LOG(...) log::debug("vk::View", __VA_ARGS__)
#else
#define XL_VKVIEW_LOG(...)
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

View::~View() {

}

bool View::init(Application &loop, const Device &dev, ViewInfo &&info) {
	if (!xenolith::View::init(loop, move(info))) {
		return false;
	}

	_threadName = toString("View:", info.title);
	_instance = static_cast<Instance *>(loop.getGlLoop()->getGlInstance().get());
	_device = const_cast<Device *>(static_cast<const Device *>(&dev));
	_director = Rc<Director>::create(_mainLoop, _constraints, this);
	if (_info.onCreated) {
		_mainLoop->performOnMainThread([this, c = _constraints] {
			_info.onCreated(*this, c);
		}, this);
	} else {
		run();
	}
	return true;
}

void View::threadInit() {
	_init = true;
	_running = true;
	_avgFrameInterval.reset(0);

	_refId = retain();
	thread::ThreadInfo::setThreadInfo(_threadName);
	_threadId = std::this_thread::get_id();
	_shouldQuit.test_and_set();

	auto info = getSurfaceOptions();
	auto cfg = _info.selectConfig(*this, info);

	if (info.surfaceDensity != 1.0f) {
		_constraints.density = _info.density * info.surfaceDensity;
	}

	createSwapchain(info, move(cfg), cfg.presentMode);

	if (_initImage && !_options.followDisplayLink) {
		presentImmediate(move(_initImage), nullptr);
		_initImage = nullptr;
	}

	mapWindow();
}

void View::threadDispose() {
	clearImages();
	_running = false;

	if (_options.renderImageOffscreen) {
		// offscreen does not need swapchain outside of view thread
		_swapchain->invalidate();
	}
	_swapchain = nullptr;
	_surface = nullptr;

	finalize();

	if (_threadStarted) {
		_thread.detach();
		_threadStarted = false;
	}

#if SP_REF_DEBUG
	auto refcount = getReferenceCount();
	release(_refId);
	if (refcount > 1) {
		this->foreachBacktrace([] (uint64_t id, Time time, const std::vector<std::string> &vec) {
			StringStream stream;
			stream << "[" << id << ":" << time.toHttp<Interface>() << "]:\n";
			for (auto &it : vec) {
				stream << "\t" << it << "\n";
			}
			log::debug("vk::View", stream.str());
		});
	} else {
		_glLoop = nullptr;
	}
#else
	release(_refId);
#endif
}

void View::update(bool displayLink) {
	xenolith::View::update(displayLink);

	updateFences();

	if (displayLink && _options.followDisplayLink) {
		// ignore present windows
		for (auto &it : _scheduledPresent) {
			runScheduledPresent(move(it));
		}
		_scheduledPresent.clear();
	}

	do {
		auto it = _fenceImages.begin();
		while (it != _fenceImages.end()) {
			if (_fenceOrder < (*it)->getOrder()) {
				_scheduledImages.emplace_back(move(*it));
				it = _fenceImages.erase(it);
			} else {
				++ it;
			}
		}
	} while (0);

	acquireScheduledImage();

	auto clock = xenolith::platform::clock(core::ClockType::Monotonic);

	if (!_options.followDisplayLink) {
		auto it = _scheduledPresent.begin();
		while (it != _scheduledPresent.end()) {
			if (!(*it)->getPresentWindow() || (*it)->getPresentWindow() < clock) {
				runScheduledPresent(move(*it));
				it = _scheduledPresent.erase(it);
			} else {
				++ it;
			}
		}
	}

	if (_swapchain && !_swapchainInvalidated && _scheduledTime < clock && _options.renderOnDemand) {
		auto acquiredImages = _swapchain->getAcquiredImagesCount();
		if (_swapchain && _framesInProgress == 0 && acquiredImages == 0) {
			XL_VKVIEW_LOG("update - scheduleNextImage");
			scheduleNextImage(0, true);
		} else {
			//log::verbose("vk::View", "Frame dropped: ", acquiredImages, " ", _framesInProgress);
		}
	}
}

void View::close() {
	xenolith::View::close();
}

void View::run() {
	auto refId = retain();
	_threadStarted = true;
	_thread = std::thread(View::workerThread, this, nullptr);
	performOnThread([this, refId] {
		release(refId);
	}, this);
}

void View::runWithQueue(const Rc<RenderQueue> &queue) {
	XL_VKVIEW_LOG("runWithQueue");
	auto a = queue->getPresentImageOutput();
	if (!a) {
		a = queue->getTransferImageOutput();
	}
	if (!a) {
		log::error("vk::View", "Fail to run view with queue '", queue->getName(),  "': no usable output attachments found");
		return;
	}

	log::verbose("View", "View::runWithQueue");

	auto req = Rc<FrameRequest>::create(queue, _frameEmitter, _constraints);
	req->setOutput(a, [this] (core::FrameAttachmentData &attachment, bool success, Ref *data) {
		log::verbose("View", "View::runWithQueue - output");
		if (success) {
			_initImage = move(attachment.image);
		}
		run();
		return true;
	}, this);

	_mainLoop->performOnMainThread([this, req] {
		if (_director->acquireFrame(req)) {
			_glLoop->performOnGlThread([this, req = move(req)] () mutable {
				_frameEmitter->submitNextFrame(move(req));
			});
		}
	}, this);
}

void View::onAdded(Device &dev) {
	std::unique_lock<Mutex> lock(_mutex);
	_device = &dev;
	_running = true;
}

void View::onRemoved() {
	std::unique_lock<Mutex> lock(_mutex);
	_running = false;
	_callbacks.clear();
	lock.unlock();
	if (_threadStarted) {
		_thread.join();
	}
}

void View::deprecateSwapchain(bool fast) {
	XL_VKVIEW_LOG("deprecateSwapchain");
	if (!_running) {
		return;
	}
	performOnThread([this, fast] {
		if (!_swapchain) {
			return;
		}

		_swapchain->deprecate(fast);
		auto it = _scheduledPresent.begin();
		while (it != _scheduledPresent.end()) {
			runScheduledPresent(move(*it));
			it = _scheduledPresent.erase(it);
		}

		if (!_blockSwapchainRecreation && _swapchain->getAcquiredImagesCount() == 0) {
			recreateSwapchain(_swapchain->getRebuildMode());
		}
	}, this, true);
}

bool View::present(Rc<ImageStorage> &&object) {
	XL_VKVIEW_LOG("present");
	if (object->isSwapchainImage()) {
		if (_options.followDisplayLink) {
			performOnThread([this, object = move(object)] () mutable {
				schedulePresent((SwapchainImage *)object.get(), 0);
			}, this);
			return false;
		}
		auto clock = xenolith::platform::clock(core::ClockType::Monotonic);
		auto img = (SwapchainImage *)object.get();
		if (!img->getPresentWindow() || img->getPresentWindow() < clock) {
			if (_options.presentImmediate) {
				performOnThread([this, object = move(object)] () mutable {
					auto queue = _device->tryAcquireQueueSync(QueueOperations::Present, true);
					auto img = (SwapchainImage *)object.get();
					if (img->getSwapchain() == _swapchain && img->isSubmitted()) {
						presentWithQueue(*queue, move(object));
					}
					_glLoop->performOnGlThread([this,  queue = move(queue)] () mutable {
						_device->releaseQueue(move(queue));
					}, this);
				}, this);
				return false;
			}
			auto queue = _device->tryAcquireQueueSync(QueueOperations::Present, false);
			if (queue) {
				performOnThread([this, queue = move(queue), object = move(object)] () mutable {
					auto img = (SwapchainImage *)object.get();
					if (img->getSwapchain() == _swapchain && img->isSubmitted()) {
						presentWithQueue(*queue, move(object));
					}
					_glLoop->performOnGlThread([this,  queue = move(queue)] () mutable {
						_device->releaseQueue(move(queue));
					}, this);
				}, this);
			} else {
				_device->acquireQueue(QueueOperations::Present, *(Loop *)_glLoop.get(),
						[this, object = move(object)] (Loop &, const Rc<DeviceQueue> &queue) mutable {
					performOnThread([this, queue, object = move(object)] () mutable {
						auto img = (SwapchainImage *)object.get();
						if (img->getSwapchain() == _swapchain && img->isSubmitted()) {
							presentWithQueue(*queue, move(object));
						}
						_glLoop->performOnGlThread([this,  queue = move(queue)] () mutable {
							_device->releaseQueue(move(queue));
						}, this);
					}, this);
				}, [this] (Loop &) {
					invalidate();
				}, this);
			}
		} else {
			performOnThread([this, object = move(object), t = img->getPresentWindow() - clock] () mutable {
				schedulePresent((SwapchainImage *)object.get(), t);
			}, this, true);
		}
	} else {
		if (!_options.renderImageOffscreen) {
			return true;
		}
		auto gen = _gen;
		performOnThread([this, object = move(object), gen] () mutable {
			presentImmediate(move(object), [this, gen] (bool success) {
				if (gen == _gen) {
					XL_VKVIEW_LOG("present - scheduleNextImage");
					scheduleNextImage(0, false);
				}
			});
			if (_swapchain->isDeprecated()) {
				recreateSwapchain(_swapchain->getRebuildMode());
			}
		}, this);
		return true;
	}
	return false;
}

bool View::presentImmediate(Rc<ImageStorage> &&object, Function<void(bool)> &&scheduleCb) {
	XL_VKVIEW_LOG("presentImmediate: ", _framesInProgress);
	if (!_swapchain) {
		return false;
	}

	auto ops = QueueOperations::Present;
	auto dev = (Device *)_device.get();

	VkFilter filter;
	if (!isImagePresentable(*object->getImage(), filter)) {
		return false;
	}

	Rc<DeviceQueue> queue;
	Rc<CommandPool> pool;
	Rc<Fence> presentFence;

	Rc<Image> sourceImage = (Image *)object->getImage().get();
	Rc<ImageStorage> targetImage;

	Vector<const CommandBuffer *> buffers;
	Loop *loop = (Loop *)_glLoop.get();

	auto cleanup = [&] {
		if (presentFence) {
			presentFence = nullptr;
		}
		if (pool) {
			dev->releaseCommandPoolUnsafe(move(pool));
			pool = nullptr;
		}
		if (queue) {
			dev->releaseQueue(move(queue));
			queue = nullptr;
		}
		return false;
	};

#if XL_VKAPI_DEBUG
	auto t = xenolith::platform::clock(core::ClockType::Monotonic);
#endif

	if (_options.waitOnSwapchainPassFence) {
		waitForFences(_frameOrder);
	}

	XL_VKAPI_LOG("[PresentImmediate] [waitForFences] [", xenolith::platform::clock(core::ClockType::Monotonic) - t, "]");

	if (!scheduleCb) {
		presentFence = loop->acquireFence(0, false);
	}

	auto swapchainAcquiredImage = _swapchain->acquire(true, presentFence);;
	if (!swapchainAcquiredImage) {
		XL_VKAPI_LOG("[PresentImmediate] [acquire-failed] [", xenolith::platform::clock(core::ClockType::Monotonic) - t, "]");
		if (presentFence) {
			presentFence->schedule(*loop);
		}
		return cleanup();
	}

	targetImage = Rc<SwapchainImage>::create(Rc<SwapchainHandle>(_swapchain), *swapchainAcquiredImage->data, move(swapchainAcquiredImage->sem));

	XL_VKAPI_LOG("[PresentImmediate] [acquire] [", xenolith::platform::clock(core::ClockType::Monotonic) - t, "]");

	pool = dev->acquireCommandPool(ops);

	auto buf = pool->recordBuffer(*dev, [&] (CommandBuffer &buf) {
		auto targetImageObj = (Image *)targetImage->getImage().get();
		auto sourceLayout = VkImageLayout(object->getLayout());

		Vector<ImageMemoryBarrier> inputImageBarriers;
		inputImageBarriers.emplace_back(ImageMemoryBarrier(targetImageObj,
			VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));

		Vector<ImageMemoryBarrier> outputImageBarriers;
		outputImageBarriers.emplace_back(ImageMemoryBarrier(targetImageObj,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));

		if (sourceLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
			inputImageBarriers.emplace_back(ImageMemoryBarrier(sourceImage,
				VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
				sourceLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
		}

		if (!inputImageBarriers.empty()) {
			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, inputImageBarriers);
		}

		buf.cmdCopyImage(sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, targetImageObj, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, filter);

		if (!outputImageBarriers.empty()) {
			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, outputImageBarriers);
		}

		return true;
	});

	buffers.emplace_back(buf);

	core::FrameSync frameSync;
	object->rearmSemaphores(*loop);

	frameSync.waitAttachments.emplace_back(core::FrameSyncAttachment{nullptr, object->getWaitSem(),
		object.get(), core::PipelineStage::Transfer});
	frameSync.waitAttachments.emplace_back(core::FrameSyncAttachment{nullptr, targetImage->getWaitSem(),
		targetImage.get(), core::PipelineStage::Transfer});

	frameSync.signalAttachments.emplace_back(core::FrameSyncAttachment{nullptr, targetImage->getSignalSem(),
		targetImage.get()});

	XL_VKAPI_LOG("[PresentImmediate] [writeBuffers] [", xenolith::platform::clock(core::ClockType::Monotonic) - t, "]");

	if (presentFence) {
		presentFence->check(*(Loop *)_glLoop.get(), false);
	}

	XL_VKAPI_LOG("[PresentImmediate] [acquireFence] [", xenolith::platform::clock(core::ClockType::Monotonic) - t, "]");

	queue = dev->tryAcquireQueueSync(ops, true);
	if (!queue) {
		return cleanup();
	}

	XL_VKAPI_LOG("[PresentImmediate] [acquireQueue] [", xenolith::platform::clock(core::ClockType::Monotonic) - t, "]");

	if (!presentFence) {
		presentFence = loop->acquireFence(0, false);
	}

	if (!queue->submit(frameSync, *presentFence, *pool, buffers)) {
		return cleanup();
	}

	XL_VKAPI_LOG("[PresentImmediate] [submit] [", xenolith::platform::clock(core::ClockType::Monotonic) - t, "]");

	auto result = _swapchain->present(*queue, targetImage);
	updateFrameInterval();

	XL_VKAPI_LOG("[PresentImmediate] [present] [", xenolith::platform::clock(core::ClockType::Monotonic) - t, "]");

	if (result == VK_SUCCESS) {
		if (queue) {
			dev->releaseQueue(move(queue));
			queue = nullptr;
		}
		if (scheduleCb) {
			pool->autorelease(object);
			presentFence->addRelease([dev, pool = pool ? move(pool) : nullptr, scheduleCb = move(scheduleCb), object = move(object), loop] (bool success) mutable {
				if (pool) {
					dev->releaseCommandPoolUnsafe(move(pool));
				}
				loop->releaseImage(move(object));
				scheduleCb(success);
			}, this, "View::presentImmediate::releaseCommandPoolUnsafe");
			scheduleFence(move(presentFence));
		} else {
			presentFence->check(*((Loop *)_glLoop.get()), false);
			dev->releaseCommandPoolUnsafe(move(pool));
			loop->releaseImage(move(object));
		}
		XL_VKAPI_LOG("[PresentImmediate] [presentFence] [", xenolith::platform::clock(core::ClockType::Monotonic) - t, "]");
		presentFence = nullptr;
		XL_VKAPI_LOG("[PresentImmediate] [", xenolith::platform::clock(core::ClockType::Monotonic) - t, "]");
		return true;
	} else {
		if (queue) {
			queue->waitIdle();
			dev->releaseQueue(move(queue));
			queue = nullptr;
		}
		if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
			_swapchain->deprecate(false);
			presentFence->check(*loop, false);
			XL_VKAPI_LOG("[PresentImmediate] [presentFence] [", xenolith::platform::clock(core::ClockType::Monotonic) - t, "]");
			presentFence = nullptr;

			dev->releaseCommandPoolUnsafe(move(pool));
			pool = nullptr;
		}
		XL_VKAPI_LOG("[PresentImmediate] [", xenolith::platform::clock(core::ClockType::Monotonic) - t, "]");
		return cleanup();
	}
}

void View::invalidateTarget(Rc<ImageStorage> &&object) {
	XL_VKVIEW_LOG("invalidateTarget");
	if (!object) {
		return;
	}

	if (object->isSwapchainImage()) {
		auto img = (SwapchainImage *)object.get();
		img->invalidateImage();
	} else {

	}
}

Rc<Ref> View::getSwapchainHandle() const {
	if (_swapchain) {
		return _swapchain.get();
	} else {
		return nullptr;
	}
}

void View::captureImage(StringView name, const Rc<core::ImageObject> &image, AttachmentLayout l) const {
	auto str = name.str<Interface>();
	_device->getTextureSetLayout()->readImage(*_device, *(Loop *)_glLoop.get(), (Image *)image.get(), l,
		[str] (const ImageInfoData &info, BytesView view) mutable {
			if (!StringView(str).ends_with(".png")) {
				str = str + String(".png");
			}
			if (!view.empty()) {
				auto fmt = core::getImagePixelFormat(info.format);
				bitmap::PixelFormat pixelFormat = bitmap::PixelFormat::Auto;
				switch (fmt) {
				case core::PixelFormat::A: pixelFormat = bitmap::PixelFormat::A8; break;
				case core::PixelFormat::IA: pixelFormat = bitmap::PixelFormat::IA88; break;
				case core::PixelFormat::RGB: pixelFormat = bitmap::PixelFormat::RGB888; break;
				case core::PixelFormat::RGBA: pixelFormat = bitmap::PixelFormat::RGBA8888; break;
				default: break;
				}
				if (pixelFormat != bitmap::PixelFormat::Auto) {
					Bitmap bmp(view.data(), info.extent.width, info.extent.height, pixelFormat);
					bmp.save(str);
				}
			}
		});
}

void View::captureImage(Function<void(const ImageInfoData &info, BytesView view)> &&cb, const Rc<core::ImageObject> &image, AttachmentLayout l) const {
	_device->getTextureSetLayout()->readImage(*_device, *(Loop *)_glLoop.get(), (Image *)image.get(), l, move(cb));
}

void View::scheduleFence(Rc<Fence> &&fence) {
	XL_VKVIEW_LOG("scheduleFence");
	if (_running.load()) {
		performOnThread([this, fence = move(fence)] () mutable {
			auto loop = (Loop *)_glLoop.get();
			if (!fence->check(*loop, true)) {
				auto frame = fence->getFrame();
				if (frame != 0 && (_fenceOrder == 0 || _fenceOrder > frame)) {
					_fenceOrder = frame;
				}
				_fences.emplace_back(move(fence));
			}
		}, this, true);
	} else {
		auto loop = (Loop *)_glLoop.get();
		fence->check(*loop, false);
	}
}

void View::mapWindow() {
	if (_options.renderOnDemand) {
		setReadyForNextFrame();
	} else {
		_scheduledTime = 0;
		scheduleNextImage(0, true);
	}
}

void View::setReadyForNextFrame() {
	performOnThread([this] {
		if (!_readyForNextFrame) {
			_scheduledTime = 0;
			if (_swapchain && _options.renderOnDemand && _framesInProgress == 0 && _swapchain->getAcquiredImagesCount() == 0) {
				XL_VKVIEW_LOG("setReadyForNextFrame - scheduleNextImage");
				scheduleNextImage(0, true);
			} else {
				_readyForNextFrame = true;
			}
		}
	}, this, true);
}

void View::setRenderOnDemand(bool value) {
	performOnThread([this, value] {
		_options.renderOnDemand = value;
	}, this, true);
}

bool View::isRenderOnDemand() const {
	return _options.renderOnDemand;
}

bool View::pollInput(bool frameReady) {
	return false;
}

core::SurfaceInfo View::getSurfaceOptions() const {
	return _instance->getSurfaceOptions(_surface->getSurface(), _device->getPhysicalDevice());
}

void View::invalidate() {

}

void View::scheduleNextImage(uint64_t windowOffset, bool immediately) {
	XL_VKVIEW_LOG("scheduleNextImage");
	performOnThread([this, windowOffset, immediately] {
		_scheduledTime = xenolith::platform::clock(core::ClockType::Monotonic) + _info.frameInterval + config::OnDemandFrameInterval;
		if (!_options.renderOnDemand || _readyForNextFrame || immediately) {
			_frameEmitter->setEnableBarrier(_options.enableFrameEmitterBarrier);

			if (_options.renderImageOffscreen) {
				scheduleSwapchainImage(windowOffset, AcquireOffscreenImage);
			} else if (_options.acquireImageImmediately || immediately) {
				scheduleSwapchainImage(windowOffset, AcquireSwapchainImageImmediate);
			} else {
				scheduleSwapchainImage(windowOffset, AcquireSwapchainImageAsync);
			}

			_readyForNextFrame = false;
		}
	}, this, true);
}

void View::scheduleSwapchainImage(uint64_t windowOffset, ScheduleImageMode mode) {
	XL_VKVIEW_LOG("scheduleSwapchainImage");
	Rc<SwapchainImage> swapchainImage;
	Rc<FrameRequest> newFrameRequest;
	auto constraints = _constraints;

	if (mode != ScheduleImageMode::AcquireOffscreenImage) {
        if (!_swapchain) {
            return;
        }

		auto fullOffset = getUpdateInterval() + windowOffset;
		if (fullOffset > _info.frameInterval) {
			swapchainImage = Rc<SwapchainImage>::create(Rc<SwapchainHandle>(_swapchain), _frameOrder, 0);
		} else {
			swapchainImage = Rc<SwapchainImage>::create(Rc<SwapchainHandle>(_swapchain), _frameOrder, _nextPresentWindow);
		}

		swapchainImage->setReady(false);
		constraints.extent = Extent2(swapchainImage->getInfo().extent.width, swapchainImage->getInfo().extent.height);
	}

	++ _framesInProgress;
	if (_framesInProgress > _swapchain->getConfig().imageCount - 1 && _framesInProgress > 1) {
		XL_VKVIEW_LOG("scheduleSwapchainImage: extra frame: ", _framesInProgress);
	}

	newFrameRequest = _frameEmitter->makeRequest(constraints);

	// make new frame request immediately
	_mainLoop->performOnMainThread([this, req = move(newFrameRequest), swapchainImage, swapchain = _swapchain] () mutable {
		XL_VKVIEW_LOG("scheduleSwapchainImage: _director->acquireFrame");
		if (_director->acquireFrame(req)) {
			XL_VKVIEW_LOG("scheduleSwapchainImage: frame acquired");
			_glLoop->performOnGlThread([this, req = move(req), swapchainImage = move(swapchainImage), swapchain] () mutable {
				if (_glLoop->isRunning() && swapchain) {
					XL_VKVIEW_LOG("scheduleSwapchainImage: setup frame request");
					auto &queue = req->getQueue();
					auto a = queue->getPresentImageOutput();
					if (!a) {
						a = queue->getTransferImageOutput();
					}
					if (!a) {
						-- _framesInProgress;
						log::error("vk::View", "Fail to run view with queue '", queue->getName(),  "': no usable output attachments found");
						return;
					}

					req->autorelease(swapchain);
					req->setRenderTarget(a, Rc<core::ImageStorage>(swapchainImage));
					req->setOutput(a, [this, swapchain] (core::FrameAttachmentData &data, bool success, Ref *) {
						XL_VKVIEW_LOG("scheduleSwapchainImage: output on frame");
						if (data.image) {
							if (success) {
								return present(move(data.image));
							} else {
								invalidateTarget(move(data.image));
								performOnThread([this] {
									-- _framesInProgress;
								}, this);
							}
						}
						return true;
					}, this);
					XL_VKVIEW_LOG("scheduleSwapchainImage: submit frame");
					auto nextFrame = _frameEmitter->submitNextFrame(move(req));
					if (nextFrame) {
						auto order = nextFrame->getOrder();
						swapchainImage->setFrameIndex(order);

						performOnThread([this, order] {
							_frameOrder = order;
						}, this);
					}
				}
			}, this);
		}
	}, this);

	// we should wait until all current fences become signaled
	// then acquire image and wait for fence
	if (swapchainImage) {
		if (mode == AcquireSwapchainImageAsync && _options.waitOnSwapchainPassFence && _fenceOrder != 0) {
			updateFences();
			if (_fenceOrder < swapchainImage->getOrder()) {
				scheduleImage(move(swapchainImage));
			} else {
				_fenceImages.emplace_back(move(swapchainImage));
			}
		} else {
			if (!acquireScheduledImageImmediate(swapchainImage)) {
				scheduleImage(move(swapchainImage));
			}
		}
	}
}

bool View::acquireScheduledImageImmediate(const Rc<SwapchainImage> &image) {
	XL_VKVIEW_LOG("acquireScheduledImageImmediate");
	if (image->getSwapchain() != _swapchain) {
		image->invalidate();
		return true;
	}

	if (!_swapchainImages.empty()) {
		auto acquiredImage = _swapchainImages.front();
		_swapchainImages.pop_front();
		_glLoop->performOnGlThread([tmp = image.get(), acquiredImage = move(acquiredImage)] () mutable {
			tmp->setAcquisitionTime(xenolith::platform::clock(core::ClockType::Monotonic));
			tmp->setImage(move(acquiredImage->swapchain), *acquiredImage->data, move(acquiredImage->sem));
			tmp->setReady(true);
		}, image, true);
		return true;
	}

	if (!_requestedSwapchainImage.empty()) {
		return false;
	}

	if (!_scheduledImages.empty() && _requestedSwapchainImage.empty()) {
		acquireScheduledImage();
		return false;
	}

	auto nimages = _swapchain->getConfig().imageCount - _swapchain->getSurfaceInfo().minImageCount;
	if (_swapchain->getAcquiredImagesCount() > nimages) {
		return false;
	}

	auto loop = (Loop *)_glLoop.get();
	auto fence = loop->acquireFence(0);
	if (auto acquiredImage = _swapchain->acquire(false, fence)) {
		fence->check(*loop, false);
		fence = nullptr;
		loop->performOnGlThread([tmp = image.get(), acquiredImage = move(acquiredImage)] () mutable {
			tmp->setAcquisitionTime(xenolith::platform::clock(core::ClockType::Monotonic));
			tmp->setImage(move(acquiredImage->swapchain), *acquiredImage->data, move(acquiredImage->sem));
			tmp->setReady(true);
		}, image, true);
		return true;
	} else {
		fence->schedule(*loop);
		return false;
	}

	return false;
}

bool View::acquireScheduledImage() {
	if (!_requestedSwapchainImage.empty() || _scheduledImages.empty()) {
		return false;
	}

	XL_VKVIEW_LOG("acquireScheduledImage");
	auto loop = (Loop *)_glLoop.get();
	auto fence = loop->acquireFence(0);
	if (auto acquiredImage = _swapchain->acquire(true, fence)) {
		_requestedSwapchainImage.emplace(acquiredImage);
		fence->addRelease([this, f = fence.get(), acquiredImage] (bool success) mutable {
			performOnThread([this, acquiredImage = move(acquiredImage), success] () mutable {
				if (success) {
					onSwapchainImageReady(move(acquiredImage));
				} else {
					_requestedSwapchainImage.erase(acquiredImage);
				}
			}, this, true);
#if XL_VKAPI_DEBUG
			XL_VKAPI_LOG("[", f->getFrame(),  "] vkAcquireNextImageKHR [complete]",
					" [", xenolith::platform::clock(core::ClockType::Monotonic) - f->getArmedTime(), "]");
#endif
		}, this, "View::acquireScheduledImage");
		scheduleFence(move(fence));
		return true;
	} else {
		fence->schedule(*loop);
		return false;
	}
}

void View::scheduleImage(Rc<SwapchainImage> &&swapchainImage) {
	XL_VKVIEW_LOG("scheduleImage");
	if (!_swapchainImages.empty()) {
		// pop one of the previously acquired images
		auto acquiredImage = _swapchainImages.front();
		_swapchainImages.pop_front();
		_glLoop->performOnGlThread([tmp = swapchainImage.get(), acquiredImage = move(acquiredImage)] () mutable {
			tmp->setAcquisitionTime(xenolith::platform::clock(core::ClockType::Monotonic));
			tmp->setImage(move(acquiredImage->swapchain), *acquiredImage->data, move(acquiredImage->sem));
			tmp->setReady(true);
		}, swapchainImage, true);
	} else {
		_scheduledImages.emplace_back(move(swapchainImage));
		acquireScheduledImage();
	}
}

void View::onSwapchainImageReady(Rc<SwapchainHandle::SwapchainAcquiredImage> &&image) {
	XL_VKVIEW_LOG("onSwapchainImageReady");
	auto ptr = image.get();

	if (!_scheduledImages.empty()) {
		// send new swapchain image to framebuffer
		auto target = _scheduledImages.front();
		_scheduledImages.pop_front();

		_glLoop->performOnGlThread([image = move(image), target = move(target)] () mutable {
			target->setAcquisitionTime(xenolith::platform::clock(core::ClockType::Monotonic));
			target->setImage(move(image->swapchain), *image->data, move(image->sem));
			target->setReady(true);
		}, this, true);
	} else {
		// hold image until next framebuffer request, if not active queries
		_swapchainImages.emplace_back(move(image));
	}

	_requestedSwapchainImage.erase(ptr);

	if (!_scheduledImages.empty()) {
		// run next image query if someone waits for it
		acquireScheduledImage();
	}
}

bool View::recreateSwapchain(core::PresentMode mode) {
	XL_VKVIEW_LOG("recreateSwapchain");
	struct ResetData : public Ref {
		Vector<Rc<SwapchainImage>> fenceImages;
		std::deque<Rc<SwapchainImage>> scheduledImages;
		Rc<core::FrameEmitter> frameEmitter;
	};

	auto data = Rc<ResetData>::alloc();
	data->fenceImages = move(_fenceImages);
	data->scheduledImages = move(_scheduledImages);
	data->frameEmitter = _frameEmitter;

	_scheduledTime = 0;
	_framesInProgress -= data->fenceImages.size();
	_framesInProgress -= data->scheduledImages.size();

	_glLoop->performOnGlThread([data] {
		for (auto &it : data->fenceImages) {
			it->invalidate();
		}
		for (auto &it : data->scheduledImages) {
			it->invalidate();
		}
		 data->frameEmitter->dropFrames();
	}, this);

	_fenceImages.clear();
	_scheduledImages.clear();
	_requestedSwapchainImage.clear();
	_swapchainImages.clear();

	if (!_surface || mode == core::PresentMode::Unsupported) {
		_swapchainInvalidated = true;
		return false;
	}

#if DEBUG
	if (core::FrameHandle::GetActiveFramesCount() > 1) {
		core::FrameHandle::DescribeActiveFrames();
	}
#endif

	auto info = getSurfaceOptions();
	auto cfg = _info.selectConfig(*this, info);

	if (!info.isSupported(cfg)) {
		log::error("Vk-Error", "Presentation with config ", cfg.description(), " is not supported for ", info.description());
		_swapchainInvalidated = true;
		return false;
	}

	if (cfg.extent.width == 0 || cfg.extent.height == 0) {
		_swapchainInvalidated = true;
		return false;
	}

	bool ret = false;
	if (mode == core::PresentMode::Unsupported) {
		ret = createSwapchain(info, move(cfg), cfg.presentMode);
	} else {
		ret = createSwapchain(info, move(cfg), mode);
	}
	if (ret) {
		XL_VKVIEW_LOG("recreateSwapchain - scheduleNextImage");
		_swapchainInvalidated = false;
		// run frame as, no present window, no wait on fences
		scheduleNextImage(0, true);
	}
	return ret;
}

bool View::createSwapchain(const core::SurfaceInfo &info, core::SwapchainConfig &&cfg, core::PresentMode presentMode) {
	auto devInfo = _device->getInfo();

	auto swapchainImageInfo = getSwapchainImageInfo(cfg);
	uint32_t queueFamilyIndices[] = { devInfo.graphicsFamily.index, devInfo.presentFamily.index };

	do {
		auto oldSwapchain = move(_swapchain);

		_swapchain = Rc<SwapchainHandle>::create(*_device, info, cfg, move(swapchainImageInfo), presentMode,
				_surface, queueFamilyIndices, oldSwapchain ? oldSwapchain.get() : nullptr);

		if (_swapchain) {
			_constraints.extent = cfg.extent;
			_constraints.transform = cfg.transform;

			Vector<uint64_t> ids;
			auto &cache = _glLoop->getFrameCache();
			for (auto &it : _swapchain->getImages()) {
				for (auto &iit : it.views) {
					auto id = iit.second->getIndex();
					ids.emplace_back(iit.second->getIndex());
					iit.second->setReleaseCallback([loop = _glLoop, cache, id] {
						loop->performOnGlThread([cache, id] {
							cache->removeImageView(id);
						});
					});
				}
			}

			_glLoop->performOnGlThread([loop = _glLoop, ids] {
				auto &cache = loop->getFrameCache();
				for (auto &id : ids) {
					cache->addImageView(id);
				}
			});
		}

		_config = move(cfg);

		log::verbose("vk::View", "Swapchain: ", _config.description());

		++ _gen;
	} while (0);

	return _swapchain != nullptr;
}

bool View::isImagePresentable(const core::ImageObject &image, VkFilter &filter) const {
	auto dev = (Device *)_device;

	auto &sourceImageInfo = image.getInfo();
	if (sourceImageInfo.extent.depth != 1 || sourceImageInfo.format != _config.imageFormat
			|| (sourceImageInfo.usage & core::ImageUsage::TransferSrc) == core::ImageUsage::None) {
		log::error("Swapchain", "Image can not be presented on swapchain");
		return false;
	}

	VkFormatProperties sourceProps;
	VkFormatProperties targetProps;

	dev->getInstance()->vkGetPhysicalDeviceFormatProperties(dev->getInfo().device,
			VkFormat(sourceImageInfo.format), &sourceProps);
	dev->getInstance()->vkGetPhysicalDeviceFormatProperties(dev->getInfo().device,
			VkFormat(_config.imageFormat), &targetProps);

	if (_config.extent.width == sourceImageInfo.extent.width && _config.extent.height == sourceImageInfo.extent.height) {
		if ((targetProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) == 0) {
			return false;
		}

		if (sourceImageInfo.tiling == core::ImageTiling::Optimal) {
			if ((sourceProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0) {
				return false;
			}
		} else {
			if ((sourceProps.linearTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0) {
				return false;
			}
		}
	} else {
		if ((targetProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) == 0) {
			return false;
		}

		if (sourceImageInfo.tiling == core::ImageTiling::Optimal) {
			if ((sourceProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) == 0) {
				return false;
			}

			if ((sourceProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0) {
				filter = VK_FILTER_LINEAR;
			}
		} else {
			if ((sourceProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) == 0) {
				return false;
			}

			if ((sourceProps.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0) {
				filter = VK_FILTER_LINEAR;
			}
		}
	}

	return true;
}

void View::runScheduledPresent(Rc<SwapchainImage> &&object) {
	XL_VKVIEW_LOG("runScheduledPresent");
	if (_options.presentImmediate) {
		auto queue = _device->tryAcquireQueueSync(QueueOperations::Present, true);
		if (object->getSwapchain() == _swapchain && object->isSubmitted()) {
			presentWithQueue(*queue, move(object));
		}
		_glLoop->performOnGlThread([this, queue = move(queue)] () mutable {
			_device->releaseQueue(move(queue));
		}, this);
	} else {
		_glLoop->performOnGlThread([this, object = move(object)] () mutable {
			if (!_glLoop->isRunning()) {
				return;
			}

			_device->acquireQueue(QueueOperations::Present, *(Loop*) _glLoop.get(),
					[this, object = move(object)](Loop&, const Rc<DeviceQueue> &queue) mutable {
				performOnThread([this, queue, object = move(object)]() mutable {
					if (object->getSwapchain() == _swapchain && object->isSubmitted()) {
						presentWithQueue(*queue, move(object));
					}
					_glLoop->performOnGlThread([this, queue = move(queue)]() mutable {
						_device->releaseQueue(move(queue));
					}, this);
				}, this);
			}, [this](Loop&) {
				invalidate();
			}, this);
		}, this);
	}
}

void View::presentWithQueue(DeviceQueue &queue, Rc<ImageStorage> &&image) {
	XL_VKVIEW_LOG("presentWithQueue: ", _framesInProgress);
	auto res = _swapchain->present(queue, move(image));
	auto dt = updateFrameInterval();
	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
		XL_VKVIEW_LOG("presentWithQueue - deprecate swapchain");
		_swapchain->deprecate(false);
	} else if (res != VK_SUCCESS) {
		log::error("vk::View", "presentWithQueue: error:", getVkResultName(res));
	}
	XL_VKVIEW_LOG("presentWithQueue - presented");
	_blockSwapchainRecreation = true;

	// DO NOT decrement active frame counter before poll input
	// Input can call setReadyForNextFrame, that spawns new frame when _framesInProgress is 0
	// so, scheduleNextImage will starts separate frame stream, that can cause deadlock on resources

	if (!pollInput(true)) {
		_blockSwapchainRecreation = false;
		-- _framesInProgress;
		XL_VKVIEW_LOG("presentWithQueue - pollInputExit");
		return;
	}

	_blockSwapchainRecreation = false;
	-- _framesInProgress;

	if (_swapchain->isDeprecated() && _swapchain->getAcquiredImagesCount() == 0) {
		waitForFences(_frameOrder);
		queue.waitIdle();

		recreateSwapchain(_swapchain->getRebuildMode());
	} else {
		if (!_options.renderOnDemand || _readyForNextFrame) {
			if (_options.followDisplayLink) {
				XL_VKVIEW_LOG("presentWithQueue - scheduleNextImage - followDisplayLink");
				scheduleNextImage(0, true);
				XL_VKVIEW_LOG("presentWithQueue - end");
				return;
			}
			_nextPresentWindow = dt.clock + _info.frameInterval - getUpdateInterval();

			// if current or average framerate below preferred - reduce present window to release new frame early
			if (dt.dt > _info.frameInterval || dt.avg > _info.frameInterval) {
				_nextPresentWindow -= (std::max(dt.dt, dt.avg) - _info.frameInterval);
			}

			XL_VKVIEW_LOG("presentWithQueue - scheduleNextImage");
			scheduleNextImage(0, false);
		}
	}

	XL_VKVIEW_LOG("presentWithQueue - end");
}

void View::invalidateSwapchainImage(Rc<ImageStorage> &&image) {
	XL_VKVIEW_LOG("invalidateSwapchainImage");
	_swapchain->invalidateImage(move(image));

	if (_swapchain->isDeprecated() && _swapchain->getAcquiredImagesCount() == 0) {
		// log::debug("View", "recreateSwapchain - View::invalidateSwapchainImage (", renderqueue::FrameHandle::GetActiveFramesCount(), ")");
		recreateSwapchain(_swapchain->getRebuildMode());
	} else {
		XL_VKVIEW_LOG("invalidateSwapchainImage - scheduleNextImage");
		scheduleNextImage(_info.frameInterval, false);
	}
}

View::FrameTimeInfo View::updateFrameInterval() {
	FrameTimeInfo ret;
	ret.clock = xenolith::platform::clock(core::ClockType::Monotonic);
	ret.dt = ret.clock - _lastFrameStart;
	_lastFrameInterval = ret.dt;
	_avgFrameInterval.addValue(ret.dt);
	_avgFrameIntervalValue = _avgFrameInterval.getAverage();
	_lastFrameStart = ret.clock;
	ret.avg = _avgFrameIntervalValue.load();
	return ret;
}

void View::waitForFences(uint64_t min) {
	auto loop = (Loop *)_glLoop.get();
	auto it = _fences.begin();
	while (it != _fences.end()) {
		if ((*it)->getFrame() <= min) {
			// log::debug("View", "waitForFences: ", (*it)->getTag());
			if ((*it)->check(*loop, false)) {
				it = _fences.erase(it);
			} else {
				++ it;
			}
		} else {
			++ it;
		}
	}
}

void View::finalize() {
	_glLoop->performOnGlThread([this] {
		end();
	}, this);

	std::unique_lock<Mutex> lock(_mutex);
	_callbacks.clear();
}

void View::updateFences() {
	uint64_t fenceOrder = 0;
	do {
		auto loop = (Loop *)_glLoop.get();
		auto it = _fences.begin();
		while (it != _fences.end()) {
			if ((*it)->check(*loop, true)) {
				it = _fences.erase(it);
			} else {
				auto frame = (*it)->getFrame();
				if (frame != 0 && (fenceOrder == 0 || fenceOrder > frame)) {
					fenceOrder = frame;
				}
				++ it;
			}
		}
	} while (0);

	_fenceOrder = fenceOrder;
}

void View::clearImages() {
	_mutex.lock();

	auto loop = (Loop *)_glLoop.get();
	for (auto &it : _fences) {
		it->check(*loop, false);
	}
	_fences.clear();
	_mutex.unlock();

	for (auto &it :_fenceImages) {
		it->invalidateSwapchain();
	}
	_fenceImages.clear();

	for (auto &it :_scheduledImages) {
		it->invalidateSwapchain();
	}
	_scheduledImages.clear();

	for (auto &it :_scheduledPresent) {
		it->invalidateSwapchain();
	}
	_scheduledPresent.clear();
}

void View::schedulePresent(SwapchainImage *img, uint64_t) {
	_scheduledPresent.emplace_back(img);
}

}
