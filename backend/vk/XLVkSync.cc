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

#include "platform/fd/SPEventPollFd.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

Semaphore::~Semaphore() { }

static void Semaphore_destroy(core::Device *dev, core::ObjectType, core::ObjectHandle ptr, void *) {
	auto d = static_cast<Device *>(dev);
	d->getTable()->vkDestroySemaphore(d->getDevice(), (VkSemaphore)ptr.get(), nullptr);
}

static void Fence_destroy(core::Device *dev, core::ObjectType, core::ObjectHandle ptr, void *) {
	auto d = static_cast<Device *>(dev);
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

bool Fence::init(Device &dev, core::FenceType type) {
	VkExportFenceCreateInfo exportInfo;
	exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO;
	exportInfo.pNext = nullptr;
	exportInfo.handleTypes = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;


	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	if (dev.hasExternalFences()) {
		fenceInfo.pNext = &exportInfo;
	} else {
		fenceInfo.pNext = nullptr;
	}
	fenceInfo.flags = 0;

	_state = Disabled;

	if (dev.getTable()->vkCreateFence(dev.getDevice(), &fenceInfo, nullptr, &_fence) == VK_SUCCESS) {
		_type = type;
		return core::Fence::init(dev, Fence_destroy, core::ObjectType::Fence, core::ObjectHandle(_fence));
	}
	return false;
}

Rc<event::Handle> Fence::exportFence(Loop &loop, Function<void()> &&cb) {
	if (_type != core::FenceType::Default) {
		return nullptr;
	}

	auto dev = static_cast<Device *>(_object.device);

	int fd = -1;
	if (dev->hasExternalFences()) {
		VkFenceGetFdInfoKHR getFenceFdInfo;
		getFenceFdInfo.sType = VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR;
		getFenceFdInfo.pNext = nullptr;
		getFenceFdInfo.fence = _fence;
		getFenceFdInfo.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;

		auto status = dev->getTable()->vkGetFenceFdKHR(dev->getDevice(), &getFenceFdInfo, &fd);

		if (status == VK_SUCCESS) {
			if (fd < 0) {
				return nullptr;
			}

			auto refId = retain();
			return event::PollFdHandle::create(loop.getLooper()->getQueue(), fd,
					event::PollFlags::In | event::PollFlags::CloseFd,
					[this, refId, completeCb = sp::move(cb), l = &loop] (int fd, event::PollFlags flags) -> Status {
				if (hasFlag(flags, event::PollFlags::In)) {
					if (completeCb) {
						completeCb();
					}
					setSignaled(*l);
					release(refId);
					return Status::Done;
				}
				return Status::Ok;
			});
		} else if (status != VK_ERROR_FEATURE_NOT_PRESENT) {
			log::error("Fence", "Fail to export fence fd");
		}
	}
	return nullptr;
}

Status Fence::doCheckFence(bool lockfree) {
	VkResult status;
	auto dev = static_cast<Device *>(_object.device);

	dev->makeApiCall([&, this] (const DeviceTable &table, VkDevice device) {
		if (lockfree) {
			status = table.vkGetFenceStatus(device, _fence);
		} else {
			status = table.vkWaitForFences(device, 1, &_fence, VK_TRUE, UINT64_MAX);
		}
	});
	return getStatus(status);
}

void Fence::doResetFence() {
	auto dev = static_cast<Device *>(_object.device);
	dev->getTable()->vkResetFences(dev->getDevice(), 1, &_fence);
}

}
