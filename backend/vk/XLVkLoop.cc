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

#define XL_VK_DEPS_DEBUG 0

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

struct DependencyRequest : public Ref {
	Vector<Rc<core::DependencyEvent>> events;
	Function<void(bool)> callback;
	uint64_t initial = 0;
	uint32_t signaled = 0;
	bool success = true;
};

struct PresentationData {
	PresentationData() { }

	uint64_t getLastUpdateInterval() {
		auto tmp = lastUpdate;
		lastUpdate = platform::clock(core::ClockType::Monotonic);
		return lastUpdate - tmp;
	}

	uint64_t now = platform::clock(core::ClockType::Monotonic);
	uint64_t last = 0;
	uint64_t updateInterval = config::PresentationSchedulerInterval;
	uint64_t lastUpdate = 0;
};

struct Loop::Internal final : memory::AllocPool {
	Internal(memory::pool_t *pool, Loop *l) : pool(pool), loop(l) {
		timers = new (pool) memory::vector<Timer>(); timers->reserve(8);
		reschedule = new (pool) memory::vector<Timer>(); reschedule->reserve(8);
		autorelease = new (pool) memory::vector<Rc<Ref>>(); autorelease->reserve(8);
	}

	void setDevice(Rc<Device> &&dev) {
		requiredTasks += 3;

		device = move(dev);

		device->begin(*loop, *queue, [this] (bool success) {
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

		fences.clear();
		transferQueue = nullptr;
		renderQueueCompiler = nullptr;
		device->end();

		if (info->onDeviceFinalized) {
			info->onDeviceFinalized(*loop, *device);
		}

		device = nullptr;
	}

	void waitIdle() {
		auto r = running->exchange(false);

		queue->lock(); // lock to prevent new tasks

		// wait for all pending tasks on fences
		for (auto &it : scheduledFences) {
			it->check(*loop, false);
		}
		scheduledFences.clear();

		if (device) {
			// wait for device
			device->waitIdle();
		}

		queue->unlock();

		// wait for all CPU tasks
		queue->waitForAll();

		// after this, there should be no CPU or GPU tasks pending or executing

		if (r) {
			running->exchange(true);
		}
	}

	void compileResource(Rc<TransferResource> &&req) {
		auto h = loop->makeFrame(transferQueue->makeRequest(sp::move(req)), 0);
		h->update(true);
	}

	void compileQueue(const Rc<core::Queue> &req, Function<void(bool)> &&cb) {
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
			-- requiredTasks;
			if (requiredTasks == 0 && signalInit) {
				signalInit();
			}
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
		req->initial = platform::clock(core::ClockType::Monotonic);

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

	void wakeup() { }

	memory::pool_t *pool = nullptr;

	Loop *loop = nullptr;
	const core::LoopInfo *info = nullptr;

	memory::vector<Timer> *timers;
	memory::vector<Timer> *reschedule;
	memory::vector<Rc<Ref>> *autorelease;

	std::multimap<DependencyEvent *, Rc<DependencyRequest>, std::less<void>> dependencyRequests;

	Mutex resourceMutex;

	Rc<Device> device;
	Rc<thread::TaskQueue> queue;
	Vector<Rc<Fence>> fences;
	Set<Rc<Fence>> scheduledFences;

	Rc<RenderQueueCompiler> renderQueueCompiler;
	Rc<TransferQueue> transferQueue;
	Rc<MaterialCompiler> materialQueue;
	Rc<MeshCompiler> meshQueue;
	std::atomic<bool> *running = nullptr;
	uint32_t requiredTasks = 0;

	Function<void()> signalInit;

	PresentationData presentationData;
};

struct Loop::Timer final {
	Timer(uint64_t interval, Function<bool(Loop &)> &&cb, StringView t)
	: interval(interval), callback(sp::move(cb)), tag(t) { }

	uint64_t interval;
	uint64_t value = 0;
	Function<bool(Loop &)> callback; // return true if timer is complete and should be removed
	StringView tag;
};

Loop::~Loop() { }

Loop::Loop() { }

bool Loop::init(Rc<Instance> &&instance, LoopInfo &&info) {
	if (!core::Loop::init(instance, move(info))) {
		return false;
	}

	_vkInstance = move(instance);

	run();

	return true;
}

void Loop::threadInit() {
	thread::ThreadInfo::setThreadInfo("vk::Loop");
	_thisThreadId = std::this_thread::get_id();

	auto pool = thread::ThreadInfo::getThreadInfo()->threadPool;

	_internal = new (pool) vk::Loop::Internal(pool, this);
	_internal->pool = pool;
	_internal->running = &_running;
	_internal->info = &_info;

	_internal->signalInit = [this] {
		// signal to main thread
		Thread::threadInit();
	};

	_internal->queue = Rc<thread::TaskQueue>::alloc("Vk::Loop::Queue");
	_internal->queue->spawnWorkers(LoopThreadId, _internal->info->threadsCount);

	if (auto dev = _vkInstance->makeDevice(_info)) {
		_internal->setDevice(move(dev));
		_frameCache = Rc<FrameCache>::create(*this, *_internal->device);
	} else if (_info.deviceIdx != core::Instance::DefaultDevice) {
		log::warn("vk::Loop", "Unable to create device with index: ", _info.deviceIdx, ", fallback to default");
		_info.deviceIdx = core::Instance::DefaultDevice;
		if (auto dev = _vkInstance->makeDevice(_info)) {
			_internal->setDevice(move(dev));
			_frameCache = Rc<FrameCache>::create(*this, *_internal->device);
		} else {
			log::error("vk::Loop", "Unable to create device");
		}
	} else {
		log::error("vk::Loop", "Unable to create device");
	}
}

void Loop::threadDispose() {
	if (_frameCache) {
		_frameCache->invalidate();
	}

	_internal->waitIdle();

	_internal->queue->waitForAll();

	_internal->queue->lock();
	_internal->timers->clear();
	_internal->reschedule->clear();
	_internal->autorelease->clear();
	_internal->queue->unlock();

	_internal->queue->cancelWorkers();
	_internal->queue = nullptr;

	_internal->endDevice();

	delete _internal;
	_internal = nullptr;

	Thread::threadDispose();
}

static bool Loop_pollEvents(Loop::Internal *internal) {
	bool timeoutPassed = false;
	auto counter = internal->queue->getOutputCounter();
	if (counter > 0) {
		XL_PROFILE_BEGIN(queue, "gl::Loop::Queue", "queue", 500);
		internal->queue->update();
		XL_PROFILE_END(queue)

		internal->presentationData.now = platform::clock(core::ClockType::Monotonic);
		if (internal->presentationData.now - internal->presentationData.last > internal->presentationData.updateInterval) {
			timeoutPassed = true;
		}
	} else {
		internal->presentationData.now = platform::clock(core::ClockType::Monotonic);
		if (internal->presentationData.now - internal->presentationData.last > internal->presentationData.updateInterval) {
			timeoutPassed = true;
		} else {
			if (internal->timers->empty() && internal->scheduledFences.empty()) {
				// no timers - just wait for events with 60FPS wakeups
				auto t = std::max(internal->presentationData.updateInterval, uint64_t(1'000'000 / 60));
				internal->queue->wait(TimeInterval::microseconds(t));
			} else {
				if (!internal->queue->wait(TimeInterval::microseconds(
						internal->presentationData.updateInterval - (internal->presentationData.now - internal->presentationData.last)))) {
					internal->presentationData.now = platform::clock(core::ClockType::Monotonic);
					timeoutPassed = true;
				}
			}
		}
	}
	return timeoutPassed;
}

static void Loop_runFences(Loop::Internal *internal) {
	auto it = internal->scheduledFences.begin();
	while (it != internal->scheduledFences.end()) {
		if ((*it)->check(*internal->loop, true)) {
			it = internal->scheduledFences.erase(it);
		}
	}
}

static void Loop_runTimers(Loop::Internal *internal, uint64_t dt) {
	auto timers = internal->timers;
	internal->timers = internal->reschedule;

	auto it = timers->begin();
	while (it != timers->end()) {
		if (it->interval) {
			it->value += dt;
			if (it->value > it->interval) {

				XL_PROFILE_BEGIN(timers, "gl::Loop::Timers", it->tag, 1000);

				auto ret = it->callback(*internal->loop);

				XL_PROFILE_END(timers);

				if (!ret) {
					it->value -= it->interval;
				} else {
					it = timers->erase(it);
					continue;
				}
			}
			++ it;
		} else {
			XL_PROFILE_BEGIN(timers, "gl::Loop::Timers", it->tag, 1000);

			auto ret = it->callback(*internal->loop);

			XL_PROFILE_END(timers);

			if (ret) {
				it = timers->erase(it);
			} else {
				++ it;
			}
		}
	}

	if (!internal->timers->empty()) {
		for (auto &it : *internal->timers) {
			timers->emplace_back(sp::move(it));
		}
		internal->timers->clear();
	}
	internal->timers = timers;
}

bool Loop::worker() {
	if (!_internal->device) {
		_running.store(false);
		return false;
	}

	if (_continueExecution.test_and_set()) {
		bool timeoutPassed = false;

		++ _clock;

		XL_PROFILE_BEGIN(loop, "vk::Loop", "loop", 1000);

		XL_PROFILE_BEGIN(poll, "vk::Loop::Poll", "poll", 500);
		timeoutPassed = Loop_pollEvents(_internal);
		XL_PROFILE_END(poll)

		Loop_runFences(_internal);

		if (timeoutPassed) {
			auto dt = _internal->presentationData.now - _internal->presentationData.last;
			XL_PROFILE_BEGIN(timers, "vk::Loop::Timers", "timers", 500);
			Loop_runTimers(_internal, dt);
			XL_PROFILE_END(timers)
			_internal->presentationData.last = _internal->presentationData.now;
		}

		XL_PROFILE_BEGIN(autorelease, "vk::Loop::Autorelease", "autorelease", 500);
		_internal->autorelease->clear();
		XL_PROFILE_END(autorelease)

		_frameCache->clear();

		XL_PROFILE_END(loop)

		return true;
	}

	_running.store(false);
	return false;
}

void Loop::compileResource(Rc<core::Resource> &&req, Function<void(bool)> &&cb, bool preload) const {
	auto res = Rc<TransferResource>::create(_internal->device->getAllocator(), sp::move(req), sp::move(cb));
	if (preload) {
		res->initialize();
	}
	performOnGlThread([this, res = sp::move(res)] () mutable {
		_internal->compileResource(sp::move(res));
	}, const_cast<Loop *>(this), true);
}

void Loop::compileQueue(const Rc<Queue> &req, Function<void(bool)> &&callback) const {
	performOnGlThread([this, req, callback = sp::move(callback)]() mutable {
		_internal->compileQueue(req, sp::move(callback));
	}, const_cast<Loop *>(this), true);
}

void Loop::compileMaterials(Rc<core::MaterialInputData> &&req, const Vector<Rc<DependencyEvent>> &deps) const {
	performOnGlThread([this, req = move(req), deps = deps] () mutable {
		_internal->compileMaterials(sp::move(req), sp::move(deps));
	}, const_cast<Loop *>(this), true);
}

void Loop::compileImage(const Rc<core::DynamicImage> &img, Function<void(bool)> &&callback) const {
	performOnGlThread([this, img, callback = sp::move(callback)]() mutable {
		_internal->device->compileImage(*this, img, sp::move(callback));
	}, const_cast<Loop *>(this), true);
}

void Loop::runRenderQueue(Rc<FrameRequest> &&req, uint64_t gen, Function<void(bool)> &&callback) {
	performOnGlThread([this, req = sp::move(req), gen, callback = sp::move(callback)]() mutable {
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

void Loop::schedule(Function<bool(core::Loop &)> &&cb, StringView tag) {
	if (isOnThisThread()) {
		_internal->timers->emplace_back(0, sp::move(cb), tag);
	} else {
		performOnGlThread([this, cb = sp::move(cb), tag] () mutable {
			_internal->timers->emplace_back(0, sp::move(cb), tag);
		});
	}
}

void Loop::schedule(Function<bool(core::Loop &)> &&cb, uint64_t delay, StringView tag) {
	if (isOnThisThread()) {
		_internal->timers->emplace_back(delay, sp::move(cb), tag);
	} else {
		performOnGlThread([this, cb = sp::move(cb), delay, tag] () mutable {
			_internal->timers->emplace_back(delay, sp::move(cb), tag);
		});
	}
}

void Loop::performInQueue(Rc<thread::Task> &&task) const {
	if (!_internal || !_internal->queue) {
		auto &tasks = task->getCompleteTasks();
		for (auto &it : tasks) {
			it(*task, false);
		}
		return;
	}

	_internal->queue->perform(move(task));
}

void Loop::performInQueue(Function<void()> &&func, Ref *target) const {
	if (!_internal || !_internal->queue) {
		return;
	}

	_internal->queue->perform(sp::move(func), target);
}

void Loop::performOnGlThread(Function<void()> &&func, Ref *target, bool immediate) const {
	if (!_internal || !_internal->queue) {
		return;
	}

	if (immediate) {
		if (isOnThisThread()) {
			func();
			return;
		}
	}
	_internal->queue->onMainThread(sp::move(func), target);
}

auto Loop::makeFrame(Rc<FrameRequest> &&req, uint64_t gen) -> Rc<FrameHandle> {
	if (_running.load()) {
		return Rc<DeviceFrameHandle>::create(*this, *_internal->device, move(req), gen);
	}
	return nullptr;
}

Rc<core::Framebuffer> Loop::acquireFramebuffer(const PassData *data, SpanView<Rc<core::ImageView>> views) {
	if (!_running.load()) {
		return nullptr;
	}

	return _frameCache->acquireFramebuffer(data, views);
}

void Loop::releaseFramebuffer(Rc<core::Framebuffer> &&fb) {
	_frameCache->releaseFramebuffer(move(fb));
}

auto Loop::acquireImage(const ImageAttachment *a, const AttachmentHandle *h, const core::ImageInfoData &i) -> Rc<ImageStorage> {
	if (!_running.load()) {
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
	performOnGlThread([this, image = move(image)] () mutable {
		_frameCache->releaseImage(move(image));
	}, this, true);
}

Rc<core::Semaphore> Loop::makeSemaphore() {
	return _internal->device->makeSemaphore();
}

const Vector<core::ImageFormat> &Loop::getSupportedDepthStencilFormat() const {
	return _internal->device->getSupportedDepthStencilFormat();
}

Rc<Fence> Loop::acquireFence(uint64_t v, bool init) {
	auto initFence = [&] (const Rc<Fence> &fence) {
		if (!init) {
			return;
		}

		fence->setFrame([this, fence] () mutable {
			if (isOnThisThread()) {
				// schedule fence
				_internal->scheduledFences.emplace(move(fence));
				return true;
			} else {
				performOnGlThread([this, fence = move(fence)] () mutable {
					if (!fence->check(*this, true)) {
						return;
					}

					_internal->scheduledFences.emplace(move(fence));
				}, this, true);
				return true;
			}
		}, [this, fence] () mutable {
			fence->clear();
			std::unique_lock<Mutex> lock(_internal->resourceMutex);
			_internal->fences.emplace_back(move(fence));
		}, v);
	};

	std::unique_lock<Mutex> lock(_internal->resourceMutex);
	if (!_internal->fences.empty()) {
		auto ref = move(_internal->fences.back());
		_internal->fences.pop_back();
		initFence(ref);
		return ref;
	}
	lock.unlock();
	auto ref = Rc<Fence>::create(*_internal->device);
	initFence(ref);
	return ref;
}

void Loop::signalDependencies(const Vector<Rc<DependencyEvent>> &events, Queue *q, bool success) {
	if (!events.empty()) {
		if (isOnThisThread() && _internal) {
			_internal->signalDependencies(events, q, success);
			return;
		}

		performOnGlThread([this, events, success, q = Rc<Queue>(q)] () {
			_internal->signalDependencies(events, q, success);
		}, this, false);
	}
}

void Loop::waitForDependencies(const Vector<Rc<DependencyEvent>> &events, Function<void(bool)> &&cb) {
	if (events.empty()) {
		cb(true);
	} else {
		performOnGlThread([this, events = events, cb = sp::move(cb)] () mutable {
			_internal->waitForDependencies(sp::move(events), sp::move(cb));
		}, this, true);
	}
}

void Loop::wakeup() {
	if (_internal) {
		_internal->wakeup();
	}
}

void Loop::waitIdle() {
	if (_internal) {
		_internal->waitIdle();
	}
}

void Loop::captureImage(Function<void(const ImageInfoData &info, BytesView view)> &&cb, const Rc<core::ImageObject> &image, core::AttachmentLayout l) {
	performOnGlThread([this, cb = sp::move(cb), image, l] () mutable {
		_internal->device->getTextureSetLayout()->readImage(*_internal->device, *this, image.cast<Image>(), l, sp::move(cb));
	}, this, true);
}

void Loop::captureBuffer(Function<void(const BufferInfo &info, BytesView view)> &&cb, const Rc<core::BufferObject> &buf) {
	if ((buf->getInfo().usage & core::BufferUsage::TransferSrc) != core::BufferUsage::TransferSrc) {
		log::error("vk::Loop::captureBuffer", "Buffer '", buf->getName(), "' has no BufferUsage::TransferSrc flag to being captured");
	}

	performOnGlThread([this, cb = sp::move(cb), buf] () mutable {
		_internal->device->getTextureSetLayout()->readBuffer(*_internal->device, *this, buf.cast<Buffer>(), sp::move(cb));
	}, this, true);
}

}
