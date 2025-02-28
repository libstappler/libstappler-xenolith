/**
Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLVkSync.h"
#include "XLVkDevice.h"

#ifndef XL_VKAPI_LOG
#define XL_VKAPI_LOG(...)
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

Semaphore::~Semaphore() { }

static void Semaphore_destroy(core::Device *dev, core::ObjectType, core::ObjectHandle ptr, void *) {
	auto d = ((Device *)dev);
	d->getTable()->vkDestroySemaphore(d->getDevice(), (VkSemaphore)ptr.get(), nullptr);
}

static void Fence_destroy(core::Device *dev, core::ObjectType, core::ObjectHandle ptr, void *) {
	auto d = ((Device *)dev);
	d->getTable()->vkDestroyFence(d->getDevice(), (VkFence)ptr.get(), nullptr);
}

bool Semaphore::init(Device &dev) {
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.pNext = nullptr;
	semaphoreInfo.flags = 0;

	if (dev.getTable()->vkCreateSemaphore(dev.getDevice(), &semaphoreInfo, nullptr, &_sem) == VK_SUCCESS) {
		return core::Semaphore::init(dev, Semaphore_destroy, core::ObjectType::Semaphore, core::ObjectHandle(_sem));
	}

	return false;
}

Fence::~Fence() {
	doRelease(false);
}

bool Fence::init(Device &dev) {
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = nullptr;
	fenceInfo.flags = 0;

	_state = Disabled;

	if (dev.getTable()->vkCreateFence(dev.getDevice(), &fenceInfo, nullptr, &_fence) == VK_SUCCESS) {
		return core::Object::init(dev, Fence_destroy, core::ObjectType::Fence, core::ObjectHandle(_fence));
	}
	return false;
}

void Fence::clear() {
	if (_releaseFn) {
		_releaseFn = nullptr;
	}
	if (_scheduleFn) {
		_scheduleFn = nullptr;
	}
}

void Fence::setFrame(Function<bool()> &&schedule, Function<void()> &&release, uint64_t f) {
	_frame = f;
	_scheduleFn = sp::move(schedule);
	_releaseFn = sp::move(release);
}

void Fence::setScheduleCallback(Function<bool()> &&schedule) {
	_scheduleFn = sp::move(schedule);
}

void Fence::setReleaseCallback(Function<bool()> &&release) {
	_releaseFn = sp::move(release);
}

void Fence::setArmed(DeviceQueue &q) {
	std::unique_lock<Mutex> lock(_mutex);
	_state = Armed;
	_queue = &q;
	_queue->retainFence(*this);
	_armedTime = sp::platform::clock(ClockType::Monotonic);
}

void Fence::setArmed() {
	std::unique_lock<Mutex> lock(_mutex);
	_state = Armed;
	_armedTime = sp::platform::clock(ClockType::Monotonic);
}

void Fence::setTag(StringView tag) {
	_tag = tag;
}

void Fence::addRelease(Function<void(bool)> &&cb, Ref *ref, StringView tag) {
	std::unique_lock<Mutex> lock(_mutex);
	_release.emplace_back(ReleaseHandle({sp::move(cb), ref, tag}));
}

bool Fence::schedule(Loop &loop) {
	std::unique_lock<Mutex> lock(_mutex);
	if (_state != Armed) {
		lock.unlock();
		if (_releaseFn) {
			loop.performOnGlThread([this] {
				doRelease(false);

				if (_releaseFn) {
					auto releaseFn = sp::move(_releaseFn);
					_releaseFn = nullptr;
					_scheduleFn = nullptr;
					releaseFn();
				} else {
					_scheduleFn = nullptr;
				}
			}, this, true);
		} else {
			doRelease(false);
			_scheduleFn = nullptr;
		}
		return false;
	} else {
		lock.unlock();

		if (check(loop, true)) {
			_scheduleFn = nullptr;
			return false;
		}
	}

	if (!_scheduleFn) {
		return false;
	}

	auto scheduleFn = sp::move(_scheduleFn);
	_scheduleFn = nullptr;

	if (lock.owns_lock()) {
		lock.unlock();
	}

	return scheduleFn();
}

bool Fence::check(Loop &loop, bool lockfree) {
	std::unique_lock<Mutex> lock(_mutex);
	if (_state != Armed) {
		return true;
	}

	auto dev = ((Device *)_object.device);
	enum VkResult status;

	dev->makeApiCall([&, this] (const DeviceTable &table, VkDevice device) {
		if (lockfree) {
			status = table.vkGetFenceStatus(device, _fence);
		} else {
			status = table.vkWaitForFences(device, 1, &_fence, VK_TRUE, UINT64_MAX);
		}
	});

	switch (status) {
	case VK_SUCCESS:
		_state = Signaled;
		XL_VKAPI_LOG("Fence [", _frame, "] ", _tag, ": signaled: ", sp::platform::clock(ClockType::Monotonic) - _armedTime);
		lock.unlock();
		if (loop.isOnThisThread()) {
			doRelease(true);
			scheduleReset(loop);
		} else {
			scheduleReleaseReset(loop, true);
		}
		return true;
		break;
	case VK_TIMEOUT:
	case VK_NOT_READY:
		_state = Armed;
		if (sp::platform::clock(ClockType::Monotonic) - _armedTime > 1'000'000) {
			lock.unlock();
			if (_queue) {
				XL_VKAPI_LOG("Fence [", _queue->getFrameIndex(), "] Fence is possibly broken: ", _tag);
			} else {
				XL_VKAPI_LOG("Fence [", _frame, "] Fence is possibly broken: ", _tag);
			}
			return check(loop, false);
		}
		return false;
	default:
		break;
	}
	return false;
}

void Fence::autorelease(Rc<Ref> &&ref) {
	_autorelease.emplace_back(move(ref));
}

void Fence::scheduleReset(Loop &loop) {
	if (_releaseFn) {
		loop.performInQueue(Rc<thread::Task>::create([this] (const thread::Task &) {
			auto dev = ((Device *)_object.device);
			dev->getTable()->vkResetFences(dev->getDevice(), 1, &_fence);
			return true;
		}, [this] (const thread::Task &, bool success) {
			auto releaseFn = sp::move(_releaseFn);
			_releaseFn = nullptr;
			releaseFn();
		}, this));
	} else {
		auto dev = ((Device *)_object.device);
		dev->getTable()->vkResetFences(dev->getDevice(), 1, &_fence);
	}
}

void Fence::scheduleReleaseReset(Loop &loop, bool s) {
	if (_releaseFn) {
		loop.performInQueue(Rc<thread::Task>::create([this] (const thread::Task &) {
			auto dev = ((Device *)_object.device);
			dev->getTable()->vkResetFences(dev->getDevice(), 1, &_fence);
			return true;
		}, [this, s] (const thread::Task &, bool success) {
			doRelease(s);

			auto releaseFn = sp::move(_releaseFn);
			_releaseFn = nullptr;
			releaseFn();
		}, this));
	} else {
		auto dev = ((Device *)_object.device);
		dev->getTable()->vkResetFences(dev->getDevice(), 1, &_fence);
		doRelease(s);
	}
}

void Fence::doRelease(bool success) {
	if (_queue) {
		_queue->releaseFence(*this);
		_queue = nullptr;
	}

	auto autorelease = sp::move(_autorelease);
	_autorelease.clear();

	if (!_release.empty()) {
		XL_PROFILE_BEGIN(total, "vk::Fence::reset", "total", 250);
		for (auto &it : _release) {
			if (it.callback) {
				XL_PROFILE_BEGIN(fence, "vk::Fence::reset", it.tag, 250);
				it.callback(success);
				XL_PROFILE_END(fence);
			}
		}
		XL_PROFILE_END(total);
		_release.clear();
	}
	_tag = StringView();
	autorelease.clear();
}

}
