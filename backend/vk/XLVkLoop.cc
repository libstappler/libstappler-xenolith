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

#include "XLVkLoop.h"
#include "XLVkInstance.h"
#include "XLVkDevice.h"
#include "XLVkTextureSet.h"
#include "XLVkConfig.h"
#include "XLCoreFrameCache.h"

#include "XLVkTransferQueue.h"
#include "XLVkRenderQueueCompiler.h"
#include "XLVkMeshCompiler.h"
#include "XLVkMaterialCompiler.h"

#include "SPEventLooper.h"
#include "SPEventTimerHandle.h"

#define XL_VK_DEPS_DEBUG 0

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

struct DependencyRequest : public Ref {
	Vector<Rc<core::DependencyEvent>> events;
	Function<void(bool)> callback;
	uint64_t initial = 0;
	uint32_t signaled = 0;
	bool success = true;
};

struct Loop::Internal final : memory::AllocPool {
	Internal(memory::pool_t *pool, Loop *l) : pool(pool), loop(l) { }

	void setDevice(Rc<Device> &&dev) {
		pendingTasks += 3;

		device = move(dev);

		device->begin(*loop, [this] (bool success) {
			if (info->onDeviceStarted) {
				info->onDeviceStarted(*loop, *device);
			}

			onInitTaskPerformed(success, "DeviceResources");
		});

		transferQueue = Rc<TransferQueue>::create();
		materialQueue = Rc<MaterialCompiler>::create();
		renderQueueCompiler = Rc<RenderQueueCompiler>::create(*device, transferQueue, materialQueue);

		compileQueue(transferQueue, [this] (bool success) {
			onInitTaskPerformed(success, "TransferQueue");
		});
		compileQueue(materialQueue, [this] (bool success) {
			onInitTaskPerformed(success, "MaterialCompiler");
		});
	}

	void endDevice() {
		if (!device) {
			return;
		}

		defaultFences.clear();
		swapchainFences.clear();
		transferQueue = nullptr;
		renderQueueCompiler = nullptr;
		materialQueue = nullptr;
		meshQueue = nullptr;
		device->end();

		if (info->onDeviceFinalized) {
			info->onDeviceFinalized(*loop, *device);
		}

		device = nullptr;
	}

	void update() {
		auto it = scheduledFences.begin();
		while (it != scheduledFences.end()) {
			if ((*it)->check(*loop, true)) {
				it = scheduledFences.erase(it);
			}
		}

		if (scheduledFences.empty()) {
			updateTimerHandle->pause();
		}
	}

	void waitIdle() {
		// wait for all pending tasks on fences
		for (auto &it : scheduledFences) {
			it->check(*loop, false);
		}
		scheduledFences.clear();

		if (device) {
			// wait for device
			device->waitIdle();
		}
	}

	void compileResource(Rc<TransferResource> &&req) {
		if (!_running.load()) {
			return;
		}

		auto h = loop->makeFrame(transferQueue->makeRequest(sp::move(req)), 0);
		h->update(true);
	}

	void compileQueue(const Rc<core::Queue> &req, Function<void(bool)> &&cb) {
		if (!_running.load()) {
			return;
		}

		if (!device) {
			log::error("vk::Loop", "No device to compileQueue");
			return;
		}

		auto input = Rc<RenderQueueInput>::alloc();
		input->queue = req;

		auto h = Rc<DeviceFrameHandle>::create(*loop, *device, renderQueueCompiler->makeRequest(move(input)), 0);
		if (cb) {
			h->setCompleteCallback([cb = sp::move(cb), req] (FrameHandle &handle) {
				cb(handle.isValid());
			});
		}

		h->update(true);
	}

	void compileMaterials(Rc<core::MaterialInputData> &&req, Vector<Rc<DependencyEvent>> &&deps) {
		if (!_running.load()) {
			return;
		}

		if (materialQueue->inProgress(req->attachment)) {
			materialQueue->appendRequest(req->attachment, sp::move(req), sp::move(deps));
		} else {
			auto attachment = req->attachment;
			materialQueue->setInProgress(attachment);
			materialQueue->runMaterialCompilationFrame(*loop, sp::move(req), sp::move(deps));
		}
	}

	void onInitTaskPerformed(bool success, StringView view) {
		if (success) {
			-- pendingTasks;
		} else {
			log::error("Loop", "Fail to initalize: ", view);
		}
	}

	void signalDependencies(const Vector<Rc<DependencyEvent>> &events, Queue *queue, bool success) {
		for (auto &it : events) {
			if (it->signal(queue, success)) {
				auto iit = dependencyRequests.equal_range(it.get());
				auto tmp = iit;
				while (iit.first != iit.second) {
					auto &v = iit.first->second;
					if (!success) {
						v->success = false;
					}
					++ v->signaled;
					if (v->signaled == v->events.size()) {
#if XL_VK_DEPS_DEBUG
						StringStream str; str << "signalDependencies:";
						for (auto &it : v->events) {
							str << " " << it->getId();
						}
						str << "\n";
						log::debug("vk::Loop", "Signal: ", str.str());
#endif
						v->callback(iit.first->second->success);
					}
					++ iit.first;
				}

				dependencyRequests.erase(tmp.first, tmp.second);
			}
		}
	}

	void waitForDependencies(Vector<Rc<DependencyEvent>> &&events, Function<void(bool)> &&cb) {
#if XL_VK_DEPS_DEBUG
		StringStream str; str << "waitForDependencies:";
		for (auto &it : events) {
			str << " " << it->getId();
		}
		log::debug("vk::Loop", "Wait: ", str.str());
#endif

		auto req = Rc<DependencyRequest>::alloc();
		req->events = sp::move(events);
		req->callback = sp::move(cb);
		req->initial = sp::platform::clock(ClockType::Monotonic);

		for (auto &it : req->events) {
			if (it->isSignaled()) {
				if (!it->isSuccessful()) {
					req->success = false;
				}
				++ req->signaled;
			} else {
				dependencyRequests.emplace(it.get(), req);
			}
		}

		if (req->signaled == req->events.size()) {
#if XL_VK_DEPS_DEBUG
			log::debug("vk::Loop", "Run: ", str.str());
#endif
			req->callback(req->success);
		}
	}

	void scheduleFence(Rc<Fence> &&fence) {
		if (auto handle = fence->exportFence(*loop, nullptr)) {
			loop->getLooper()->performHandle(handle);
		} else {
			if (scheduledFences.empty()) {
				auto status = updateTimerHandle->resume();
				if (status != Status::Ok) {
					log::error("vk::Loop", "Fail to resume fence scheduler: ", status);
				}
			}
			scheduledFences.emplace(move(fence));
		}
	}

	memory::pool_t *pool = nullptr;

	Loop *loop = nullptr;
	const core::LoopInfo *info = nullptr;

	Rc<event::TimerHandle> updateTimerHandle;

	std::multimap<DependencyEvent *, Rc<DependencyRequest>, std::less<void>> dependencyRequests;

	Mutex resourceMutex;

	Rc<Device> device;
	Vector<Rc<Fence>> defaultFences;
	Vector<Rc<Fence>> swapchainFences;
	Set<Rc<Fence>> scheduledFences;

	Rc<RenderQueueCompiler> renderQueueCompiler;
	Rc<TransferQueue> transferQueue;
	Rc<MaterialCompiler> materialQueue;
	Rc<MeshCompiler> meshQueue;

	std::atomic<uint32_t> pendingTasks = 0;
	std::atomic<bool> _running = true;
};

struct Loop::Timer final {
	Timer(uint64_t interval, Function<bool(Loop &)> &&cb, StringView t)
	: interval(interval), callback(sp::move(cb)), tag(t) { }

	uint64_t interval;
	uint64_t value = 0;
	Function<bool(Loop &)> callback; // return true if timer is complete and should be removed
	StringView tag;
};

bool Loop::init(event::Looper *looper, Rc<Instance> &&instance, LoopInfo &&info) {
	if (!core::Loop::init(looper, instance, move(info))) {
		return false;
	}

	looper->performOnThread([&] {
		auto pool = memory::pool::create(_looper->getThreadPool());

		_internal = new (pool) vk::Loop::Internal(pool, this);
		_internal->pool = pool;
		_internal->info = &_info;

		if (auto dev = _instance.get_cast<Instance>()->makeDevice(_info)) {
			_internal->setDevice(move(dev));
			_frameCache = Rc<FrameCache>::create(*this, *_internal->device);
		} else if (_info.deviceIdx != core::Instance::DefaultDevice) {
			log::warn("vk::Loop", "Unable to create device with index: ", _info.deviceIdx, ", fallback to default");
			_info.deviceIdx = core::Instance::DefaultDevice;
			if (auto dev = _instance.get_cast<Instance>()->makeDevice(_info)) {
				_internal->setDevice(move(dev));
				_frameCache = Rc<FrameCache>::create(*this, *_internal->device);
			} else {
				log::error("vk::Loop", "Unable to create device");
				return;
			}
		} else {
			log::error("vk::Loop", "Unable to create device");
			return;
		}

		_internal->updateTimerHandle = _looper->scheduleTimer(event::TimerInfo{
			.completion = event::TimerInfo::Completion::create<Loop>(this,
					[] (Loop *loop, event::TimerHandle *, uint32_t valuu, Status status) {
				loop->_internal->update();
				loop->_frameCache->clear();
			}),
			.interval = TimeInterval::microseconds(config::PresentationSchedulerInterval),
			.count = event::TimerInfo::Infinite
		});
	}, this);

	return true;
}

void Loop::stop() {
	std::mutex mutex;
	std::condition_variable condvar;
	std::unique_lock lock(mutex);

	_looper->performOnThread([&] {
		std::unique_lock ulock(mutex);

		_internal->_running = false;

		_internal->updateTimerHandle->cancel();
		_internal->updateTimerHandle = nullptr;

		if (_frameCache) {
			_frameCache->invalidate();
		}

		_internal->waitIdle();
		_internal->endDevice();

		auto mempool = _internal->pool;

		delete _internal;
		_internal = nullptr;

		memory::pool::destroy(mempool);

		condvar.notify_all();
	}, this);

	condvar.wait(lock);
}

bool Loop::isRunning() const {
	return _internal->_running;
}

void Loop::compileResource(Rc<core::Resource> &&req, Function<void(bool)> &&cb, bool preload) const {
	auto res = Rc<TransferResource>::create(_internal->device->getAllocator(), sp::move(req), sp::move(cb));
	if (preload) {
		res->initialize();
	}
	performOnThread([this, res = sp::move(res)] () mutable {
		_internal->compileResource(sp::move(res));
	}, const_cast<Loop *>(this), true);
}

void Loop::compileQueue(const Rc<Queue> &req, Function<void(bool)> &&callback) const {
	performOnThread([this, req, callback = sp::move(callback)]() mutable {
		_internal->compileQueue(req, sp::move(callback));
	}, const_cast<Loop *>(this), true);
}

void Loop::compileMaterials(Rc<core::MaterialInputData> &&req, const Vector<Rc<DependencyEvent>> &deps) const {
	performOnThread([this, req = move(req), deps = deps] () mutable {
		_internal->compileMaterials(sp::move(req), sp::move(deps));
	}, const_cast<Loop *>(this), true);
}

void Loop::compileImage(const Rc<core::DynamicImage> &img, Function<void(bool)> &&callback) const {
	performOnThread([this, img, callback = sp::move(callback)]() mutable {
		_internal->device->compileImage(*this, img, sp::move(callback));
	}, const_cast<Loop *>(this), true);
}

void Loop::runRenderQueue(Rc<FrameRequest> &&req, uint64_t gen, Function<void(bool)> &&callback) {
	performOnThread([this, req = sp::move(req), gen, callback = sp::move(callback)]() mutable {
		if (!_internal->_running.load()) {
			return;
		}

		auto frame = makeFrame(move(req), gen);
		if (frame && callback) {
			frame->setCompleteCallback([callback = sp::move(callback)] (FrameHandle &handle) {
				callback(handle.isValid());
			});
		}
		if (frame) {
			frame->update(true);
		}
	}, this, true);
}

void Loop::performInQueue(Rc<thread::Task> &&task) const {
	if (!_internal || !_internal->_running.load()) {
		task->cancel();
		return;
	}

	_looper->performAsync(move(task));
}

void Loop::performInQueue(Function<void()> &&func, Ref *target) const {
	if (!_internal || !_internal->_running.load()) {
		return;
	}

	_looper->performAsync(sp::move(func), target);
}

void Loop::performOnThread(Function<void()> &&func, Ref *target, bool immediate) const {
	if (!_internal || !_internal->_running.load()) {
		return;
	}

	if (immediate) {
		if (_looper->isOnThisThread()) {
			func();
			return;
		}
	}

	_looper->performOnThread(sp::move(func), target);
}

auto Loop::makeFrame(Rc<FrameRequest> &&req, uint64_t gen) -> Rc<FrameHandle> {
	if (!_internal->pendingTasks.load()) {
		return Rc<DeviceFrameHandle>::create(*this, *_internal->device, move(req), gen);
	}
	return nullptr;
}

Rc<core::Framebuffer> Loop::acquireFramebuffer(const PassData *data, SpanView<Rc<core::ImageView>> views) {
	if (_internal->pendingTasks.load()) {
		return nullptr;
	}

	return _frameCache->acquireFramebuffer(data, views);
}

void Loop::releaseFramebuffer(Rc<core::Framebuffer> &&fb) {
	_frameCache->releaseFramebuffer(move(fb));
}

auto Loop::acquireImage(const ImageAttachment *a, const AttachmentHandle *h, const core::ImageInfoData &i) -> Rc<ImageStorage> {
	if (_internal->pendingTasks.load()) {
		return nullptr;
	}

	core::ImageInfoData info(i);
	if (a->isTransient()) {
		if ((info.usage & (core::ImageUsage::ColorAttachment | core::ImageUsage::DepthStencilAttachment | core::ImageUsage::InputAttachment))
				!= core::ImageUsage::None) {
			info.usage |= core::ImageUsage::TransientAttachment;
		}
	}

	auto views = a->getImageViews(info);
	return _frameCache->acquireImage(a->getId(), info, views);
}

void Loop::releaseImage(Rc<ImageStorage> &&image) {
	performOnThread([this, image = move(image)] () mutable {
		_frameCache->releaseImage(move(image));
	}, this, true);
}

Rc<core::Semaphore> Loop::makeSemaphore() {
	return _internal->device->makeSemaphore();
}

const Vector<core::ImageFormat> &Loop::getSupportedDepthStencilFormat() const {
	return _internal->device->getSupportedDepthStencilFormat();
}

Rc<core::Fence> Loop::acquireFence(core::FenceType type) {
	auto initFence = [&] (const Rc<Fence> &fence) {
		fence->setFrame([this, fence] () mutable {
			if (_looper->isOnThisThread()) {
				// schedule fence
				_internal->scheduleFence(Rc<Fence>(fence));
				return true;
			} else {
				performOnThread([this, fence = move(fence)] () mutable {
					if (!fence->check(*this, true)) {
						return;
					}

					_internal->scheduleFence(move(fence));
				}, this, true);
				return true;
			}
		}, [this, fence] () mutable {
			fence->clear();
			std::unique_lock<Mutex> lock(_internal->resourceMutex);
			switch (fence->getType()) {
			case core::FenceType::Default:
				_internal->defaultFences.emplace_back(move(fence));
				break;
			case core::FenceType::Swapchain:
				_internal->swapchainFences.emplace_back(move(fence));
				break;
			}
		}, 0);
	};

	std::unique_lock<Mutex> lock(_internal->resourceMutex);
	switch (type) {
	case core::FenceType::Default:
		if (!_internal->defaultFences.empty()) {
			auto ref = move(_internal->defaultFences.back());
			_internal->defaultFences.pop_back();
			initFence(ref);
			return ref;
		}
		break;
	case core::FenceType::Swapchain:
		if (!_internal->swapchainFences.empty()) {
			auto ref = move(_internal->swapchainFences.back());
			_internal->swapchainFences.pop_back();
			initFence(ref);
			return ref;
		}
		break;
	}
	lock.unlock();
	auto ref = Rc<Fence>::create(*_internal->device, type);
	initFence(ref);
	return ref;
}

void Loop::signalDependencies(const Vector<Rc<DependencyEvent>> &events, Queue *q, bool success) {
	if (!events.empty()) {
		if (_looper->isOnThisThread() && _internal) {
			_internal->signalDependencies(events, q, success);
			return;
		}

		performOnThread([this, events, success, q = Rc<Queue>(q)] () {
			_internal->signalDependencies(events, q, success);
		}, this, false);
	}
}

void Loop::waitForDependencies(const Vector<Rc<DependencyEvent>> &events, Function<void(bool)> &&cb) {
	if (events.empty()) {
		cb(true);
	} else {
		performOnThread([this, events = events, cb = sp::move(cb)] () mutable {
			_internal->waitForDependencies(sp::move(events), sp::move(cb));
		}, this, true);
	}
}

void Loop::waitIdle() {
	if (_internal) {
		_internal->waitIdle();
	}
}

void Loop::captureImage(Function<void(const ImageInfoData &info, BytesView view)> &&cb, const Rc<core::ImageObject> &image, core::AttachmentLayout l) {
	performOnThread([this, cb = sp::move(cb), image, l] () mutable {
		_internal->device->getTextureSetLayout()->readImage(*_internal->device, *this, image.cast<Image>(), l, sp::move(cb));
	}, this, true);
}

void Loop::captureBuffer(Function<void(const BufferInfo &info, BytesView view)> &&cb, const Rc<core::BufferObject> &buf) {
	if ((buf->getInfo().usage & core::BufferUsage::TransferSrc) != core::BufferUsage::TransferSrc) {
		log::error("vk::Loop::captureBuffer", "Buffer '", buf->getName(), "' has no BufferUsage::TransferSrc flag to being captured");
	}

	performOnThread([this, cb = sp::move(cb), buf] () mutable {
		_internal->device->getTextureSetLayout()->readBuffer(*_internal->device, *this, buf.cast<Buffer>(), sp::move(cb));
	}, this, true);
}

void Loop::performInit() {
}

void Loop::finalizeInit() {

}

}
