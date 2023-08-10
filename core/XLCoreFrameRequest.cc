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

#include "XLCoreFrameRequest.h"
#include "XLCoreFrameEmitter.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreQueue.h"
#include "XLCoreLoop.h"

namespace stappler::xenolith::core {

FrameOutputBinding::FrameOutputBinding(const AttachmentData *a, CompleteCallback &&cb, Rc<Ref> &&ref)
: attachment(a), callback(move(cb)), handle(move(ref)) { }

FrameOutputBinding::~FrameOutputBinding() { }

bool FrameOutputBinding::handleReady(FrameAttachmentData &data, bool success) {
	if (callback) {
		return callback(data, success, handle);
	}
	return false;
}

FrameRequest::~FrameRequest() {
	if (_queue) {
		_queue->endFrame(*this);
	}
	_renderTargets.clear();
	_pool = nullptr;
}

bool FrameRequest::init(const Rc<FrameEmitter> &emitter, const FrameContraints &constraints) {
	_pool = Rc<PoolRef>::alloc();
	_emitter = emitter;
	_constraints = constraints;
	_readyForSubmit = false;
	return true;
}

bool FrameRequest::init(const Rc<Queue> &q) {
	_pool = Rc<PoolRef>::alloc();
	_queue = q;
	_queue->beginFrame(*this);
	return true;
}

bool FrameRequest::init(const Rc<Queue> &q, const FrameContraints &constraints) {
	if (init(q)) {
		_constraints = constraints;
		return true;
	}
	return false;
}

bool FrameRequest::init(const Rc<Queue> &q, const Rc<FrameEmitter> &emitter, const FrameContraints &constraints) {
	if (init(q)) {
		_emitter = emitter;
		_constraints = constraints;
		_readyForSubmit = emitter->isReadyForSubmit();
		return true;
	}
	return false;
}

void FrameRequest::addSignalDependency(Rc<DependencyEvent> &&dep) {
	if (dep) {
		_signalDependencies.emplace_back(move(dep));
	}
}

void FrameRequest::addSignalDependencies(Vector<Rc<DependencyEvent>> &&deps) {
	if (_signalDependencies.empty()) {
		_signalDependencies = move(deps);
	} else {
		for (auto &it : deps) {
			_signalDependencies.emplace_back(move(it));
		}
	}
}

void FrameRequest::addImageSpecialization(const ImageAttachment *image, ImageInfoData &&data) {
	auto it = _imageSpecialization.find(image);
	if (it != _imageSpecialization.end()) {
		it->second = move(data);
	} else {
		_imageSpecialization.emplace(image, data);
	}
}

const ImageInfoData *FrameRequest::getImageSpecialization(const ImageAttachment *image) const {
	auto it = _imageSpecialization.find(image);
	if (it != _imageSpecialization.end()) {
		return &it->second;
	}
	return nullptr;
}

bool FrameRequest::addInput(const Attachment *a, Rc<AttachmentInputData> &&data) {
	return addInput(a->getData(), move(data));
}

bool FrameRequest::addInput(const AttachmentData *a, Rc<AttachmentInputData> &&data) {
	if (a && a->attachment->validateInput(data)) {
		auto wIt = _waitForInputs.find(a);
		if (wIt != _waitForInputs.end()) {
			wIt->second.handle->submitInput(*wIt->second.queue, move(data), move(wIt->second.callback));
		} else {
			_input.emplace(a, move(data));
		}
		return true;
	}
	if (a) {
		log::vtext("FrameRequest", "Invalid input for attachment ", a->key);
	}
	return false;
}

void FrameRequest::setQueue(const Rc<Queue> &q) {
	if (_queue != q) {
		if (_queue) {
			_queue->endFrame(*this);
		}
		_queue = q;
		if (_queue) {
			_queue->beginFrame(*this);
		}
	}
}

void FrameRequest::setOutput(Rc<FrameOutputBinding> &&binding) {
	_output.emplace(binding->attachment, move(binding));
}

void FrameRequest::setOutput(const AttachmentData *a, CompleteCallback &&cb, Rc<Ref> &&ref) {
	setOutput(Rc<FrameOutputBinding>::alloc(a, move(cb), move(ref)));
}

void FrameRequest::setOutput(const Attachment *a, CompleteCallback &&cb, Rc<Ref> &&ref) {
	setOutput(a->getData(), move(cb));
}

void FrameRequest::setRenderTarget(const AttachmentData *a, Rc<ImageStorage> &&img) {
	_renderTargets.emplace(a, move(img));
}

bool FrameRequest::onOutputReady(Loop &loop, FrameAttachmentData &data) {
	auto it = _output.find(data.handle->getAttachment()->getData());
	if (it != _output.end()) {
		if (it->second->handleReady(data, true)) {
			_output.erase(it);
			return true;
		}
		return false;
	}

	return false;
}

void FrameRequest::onOutputInvalidated(Loop &loop, FrameAttachmentData &data) {
	auto it = _output.find(data.handle->getAttachment()->getData());
	if (it != _output.end()) {
		if (it->second->handleReady(data, false)) {
			_output.erase(it);
			return;
		}
	}
}

void FrameRequest::finalize(Loop &loop, HashMap<const AttachmentData *, FrameAttachmentData *> &attachments, bool success) {
	_waitForInputs.clear();

	if (!success) {
		for (auto &it : _output) {
			auto iit = attachments.find(it.second->attachment);
			if (iit != attachments.end()) {
				it.second->handleReady(*(iit->second), false);
			}
		}
		_output.clear();
	}
	if (_emitter) {
		_emitter = nullptr;
	}
}

void FrameRequest::signalDependencies(Loop &loop, Queue *q, bool success) {
	if (!_signalDependencies.empty()) {
		loop.signalDependencies(_signalDependencies, q, success);
	}
}

Rc<AttachmentInputData> FrameRequest::getInputData(const AttachmentData *attachment) {
	auto it = _input.find(attachment);
	if (it != _input.end()) {
		auto ret = it->second;
		_input.erase(it);
		return ret;
	}
	return nullptr;
}

Rc<ImageStorage> FrameRequest::getRenderTarget(const AttachmentData *a) {
	auto it = _renderTargets.find(a);
	if (it != _renderTargets.end()) {
		return it->second;
	}
	return nullptr;
}

Set<Rc<Queue>> FrameRequest::getQueueList() const {
	return Set<Rc<Queue>>{_queue};
}

void FrameRequest::waitForInput(FrameQueue &queue, const Rc<AttachmentHandle> &a, Function<void(bool)> &&cb) {
	auto it = _waitForInputs.find(a->getAttachment()->getData());
	if (it != _waitForInputs.end()) {
		it->second.callback(false);
		it->second.callback = move(cb);
	} else {
		_waitForInputs.emplace(a->getAttachment()->getData(), WaitInputData{&queue, a, move(cb)});
	}
}

const FrameOutputBinding *FrameRequest::getOutputBinding(const AttachmentData *a) const {
	auto it = _output.find(a);
	if (it != _output.end()) {
		return it->second;
	}
	return nullptr;
}

}
