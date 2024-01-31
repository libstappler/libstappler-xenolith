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

#include "XLCoreQueuePass.h"
#include "XLCoreQueue.h"
#include "XLCoreFrameQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

QueuePass::~QueuePass() { }

bool QueuePass::init(QueuePassBuilder &builder) {
	_data = builder.getData();
	return true;
}

void QueuePass::invalidate() {

}

StringView QueuePass::getName() const {
	return _data->key;
}

RenderOrdering QueuePass::getOrdering() const {
	return _data->ordering;
}

size_t QueuePass::getSubpassCount() const {
	return _data->subpasses.size();
}

PassType QueuePass::getType() const {
	return _data->type;
}

Rc<QueuePassHandle> QueuePass::makeFrameHandle(const FrameQueue &handle) {
	if (_frameHandleCallback) {
		return _frameHandleCallback(*this, handle);
	}
	return Rc<QueuePassHandle>::create(*this, handle);
}

void QueuePass::setFrameHandleCallback(FrameHandleCallback &&cb) {
	_frameHandleCallback = move(cb);
}

bool QueuePass::acquireForFrame(FrameQueue &frame, Function<void(bool)> &&onAcquired) {
	if (_owner) {
		if (_next.queue) {
			_next.acquired(false);
		}
		_next = FrameQueueWaiter{
			&frame,
			move(onAcquired)
		};
		return false;
	} else {
		_owner = &frame;
		return true;
	}
}

bool QueuePass::releaseForFrame(FrameQueue &frame) {
	if (_owner == &frame) {
		if (_next.queue) {
			_owner = move(_next.queue);
			_next.acquired(true);
			_next.queue = nullptr;
			_next.acquired = nullptr;
		} else {
			_owner = nullptr;
		}
		return true;
	} else if (_next.queue == &frame) {
		auto tmp = move(_next.acquired);
		_next.queue = nullptr;
		_next.acquired = nullptr;
		tmp(false);
		return true;
	}
	return false;
}

void QueuePass::prepare(Device &device) {

}

QueuePassHandle::~QueuePassHandle() { }

bool QueuePassHandle::init(QueuePass &pass, const FrameQueue &queue) {
	_queuePass = &pass;
	_data = pass.getData();
	return true;
}

void QueuePassHandle::setQueueData(FramePassData &data) {
	_queueData = &data;
}

const FramePassData *QueuePassHandle::getQueueData() const {
	return _queueData;
}

StringView QueuePassHandle::getName() const {
	return _data->key;
}

const Rc<Framebuffer> &QueuePassHandle::getFramebuffer() const {
	return _queueData->framebuffer;
}

bool QueuePassHandle::isAvailable(const FrameQueue &handle) const {
	return true;
}

bool QueuePassHandle::isSubmitted() const {
	return toInt(_queueData->state) >= toInt(FrameRenderPassState::Submitted);
}

bool QueuePassHandle::isCompleted() const {
	return toInt(_queueData->state) >= toInt(FrameRenderPassState::Complete);
}

bool QueuePassHandle::isFramebufferRequired() const {
	return _queuePass->getType() == PassType::Graphics;
}

bool QueuePassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	prepareSubpasses(q);
	return true;
}

void QueuePassHandle::submit(FrameQueue &, Rc<FrameSync> &&, Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) {

}

void QueuePassHandle::finalize(FrameQueue &, bool successful) {

}

AttachmentHandle *QueuePassHandle::getAttachmentHandle(const AttachmentData *a) const {
	auto it = _queueData->attachmentMap.find(a);
	if (it != _queueData->attachmentMap.end()) {
		return it->second->handle.get();
	}
	return nullptr;
}

void QueuePassHandle::autorelease(Ref *ref) const {
	if (!ref) {
		return;
	}

	std::unique_lock lock(_autoreleaseMutex);
	_autorelease.emplace_back(ref);
}

const AttachmentPassData *QueuePassHandle::getAttachemntData(const AttachmentData *a) const {
	for (auto &it : a->passes) {
		if (it->pass == _data) {
			return it;
		}
	}
	return nullptr;
}

void QueuePassHandle::prepareSubpasses(FrameQueue &q) {
	for (auto &subpass : _queuePass->getData()->subpasses) {
		if (subpass->prepareCallback) {
			subpass->prepareCallback(*subpass, q);
		}
	}
}

}
