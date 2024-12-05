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

#include "XLCoreFrameHandle.h"
#include "XLCorePlatform.h"
#include "XLCoreLoop.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

static constexpr ClockType FrameClockType = ClockType::Monotonic;

#ifdef XL_FRAME_LOG
#define XL_FRAME_LOG_INFO _request->getEmitter() ? "[Emitted] " : "", \
	"[", _order, "] [", s_frameCount.load(), \
	"] [", platform::clock(FrameClockType) - _timeStart, "] "
#else
#define XL_FRAME_LOG_INFO
#define XL_FRAME_LOG(...)
#endif

#ifndef XL_FRAME_PROFILE
#define XL_FRAME_PROFILE(fn, tag, max) \
	do { } while (0);
#endif

static std::atomic<uint32_t> s_frameCount = 0;

static Mutex s_frameMutex;
static std::set<FrameHandle *> s_activeFrames;

uint32_t FrameHandle::GetActiveFramesCount() {
	return s_frameCount.load();
}

void FrameHandle::DescribeActiveFrames() {
	s_frameMutex.lock();
#if SP_REF_DEBUG
	auto hasFailed = false;
	for (auto &it : s_activeFrames) {
		if (!it->isValidFlag()) {
			hasFailed = true;
			break;
		}
	}

	if (hasFailed) {
		StringStream stream;
		stream << "\n";
		for (auto &it : s_activeFrames) {
			stream << "\tFrame: " << it->getOrder() << " refcount: " << it->getReferenceCount() << "; success: " << it->isValidFlag() << "; backtrace:\n";
			it->foreachBacktrace([&] (uint64_t id, Time time, const std::vector<std::string> &vec) {
				stream << "[" << id << ":" << time.toHttp<Interface>() << "]:\n";
				for (auto &it : vec) {
					stream << "\t" << it << "\n";
				}
			});
		}
		stappler::log::debug("FrameHandle", stream.str());
	}
#endif

	s_frameMutex.unlock();
}

FrameHandle::~FrameHandle() {
	XL_FRAME_LOG(XL_FRAME_LOG_INFO, "Destroy");

	s_frameMutex.lock();
	-- s_frameCount;
	s_activeFrames.erase(this);
	s_frameMutex.unlock();

	if (_request) {
		_request->detachFrame();
		_request = nullptr;
	}
	_pool = nullptr;
}

bool FrameHandle::init(Loop &loop, Device &dev, Rc<FrameRequest> &&req, uint64_t gen) {
	s_frameMutex.lock();
	s_activeFrames.emplace(this);
	++ s_frameCount;
	s_frameMutex.unlock();

	_loop = &loop;
	_device = &dev;
	_request = move(req);
	_pool = _request->getPool();
	_timeStart = platform::clock(FrameClockType);
	if (!_request || !_request->getQueue()) {
		return false;
	}

	_gen = gen;
	_order = _request->getQueue()->incrementOrder();

	XL_FRAME_LOG(XL_FRAME_LOG_INFO, "Init; ready: ", _request->isReadyForSubmit());
	return setup();
}

void FrameHandle::update(bool init) {
	if (!_valid) {
		return;
	}

	XL_FRAME_LOG(XL_FRAME_LOG_INFO, "update");

	for (auto &it : _queues) {
		it->update();
	}
}

const Rc<FrameEmitter> &FrameHandle::getEmitter() const {
	return _request->getEmitter();
}

const Rc<Queue> &FrameHandle::getQueue() const {
	return _request->getQueue();
}

const FrameContraints &FrameHandle::getFrameConstraints() const {
	return _request->getFrameConstraints();
}

const ImageInfoData *FrameHandle::getImageSpecialization(const ImageAttachment *a) const {
	return _request->getImageSpecialization(a);
}

const FrameOutputBinding *FrameHandle::getOutputBinding(const Attachment *a) const {
	return _request->getOutputBinding(a->getData());
}

const FrameOutputBinding *FrameHandle::getOutputBinding(const AttachmentData *a) const {
	return _request->getOutputBinding(a);
}

Rc<ImageStorage> FrameHandle::getRenderTarget(const Attachment *a) const {
	return _request->getRenderTarget(a->getData());
}

Rc<ImageStorage> FrameHandle::getRenderTarget(const AttachmentData *a) const {
	return _request->getRenderTarget(a);
}

const Vector<Rc<DependencyEvent>> &FrameHandle::getSignalDependencies() const {
	return _request->getSignalDependencies();
}

FrameQueue *FrameHandle::getFrameQueue(Queue *queue) const {
	for (auto &it : _queues) {
		if (it->getQueue() == queue) {
			return it;
		}
	}
	return nullptr;
}

void FrameHandle::schedule(Function<bool(FrameHandle &)> &&cb, StringView tag) {
	auto linkId = retain();
	_loop->schedule([this, cb = sp::move(cb), linkId] (Loop &ctx) {
		if (!isValid()) {
			release(linkId);
			return true;
		}
		if (cb(*this)) {
			release(linkId);
			return true; // end
		}
		return false;
	}, tag);
}

void FrameHandle::performInQueue(Function<void(FrameHandle &)> &&cb, Ref *ref, StringView tag) {
	auto linkId = retain();
	_loop->performInQueue(Rc<thread::Task>::create([this, cb = sp::move(cb)] (const thread::Task &) -> bool {
		cb(*this);
		return true;
	}, [=, this] (const thread::Task &, bool) {
		XL_FRAME_LOG(XL_FRAME_LOG_INFO, "thread performed: '", tag, "'");
		release(linkId);
	}, ref));
}

void FrameHandle::performInQueue(Function<bool(FrameHandle &)> &&perform, Function<void(FrameHandle &, bool)> &&complete,
		Ref *ref, StringView tag) {
	auto linkId = retain();
	_loop->performInQueue(Rc<thread::Task>::create([this, perform = sp::move(perform)] (const thread::Task &) -> bool {
		return perform(*this);
	}, [=, this, complete = sp::move(complete)] (const thread::Task &, bool success) {
		XL_FRAME_PROFILE(complete(*this, success), tag, 1000);
		XL_FRAME_LOG(XL_FRAME_LOG_INFO, "thread performed: '", tag, "'");
		release(linkId);
	}, ref));
}

void FrameHandle::performOnGlThread(Function<void(FrameHandle &)> &&cb, Ref *ref, bool immediate, StringView tag) {
	if (immediate && _loop->isOnThisThread()) {
		XL_FRAME_PROFILE(cb(*this), tag, 1000);
	} else {
		auto linkId = retain();
		_loop->performOnGlThread([=, this, cb = sp::move(cb)] () {
			XL_FRAME_PROFILE(cb(*this);, tag, 1000);
			XL_FRAME_LOG(XL_FRAME_LOG_INFO, "thread performed: '", tag, "'");
			release(linkId);
		}, ref);
	}
}

void FrameHandle::performRequiredTask(Function<bool(FrameHandle &)> &&cb, Ref *ref, StringView tag) {
	++ _tasksRequired;
	auto linkId = retain();
	_loop->performInQueue(Rc<thread::Task>::create([this, cb = sp::move(cb)] (const thread::Task &) -> bool {
		return cb(*this);
	}, [this, linkId, tag = tag.str<Interface>()] (const thread::Task &, bool success) {
		XL_FRAME_LOG(XL_FRAME_LOG_INFO, "thread performed: '", tag, "'");
		if (success) {
			onRequiredTaskCompleted(tag);
		} else {
			invalidate();
			log::error("FrameHandle", "Async task failed: ", tag);
		}
		release(linkId);
	}, ref));
}

void FrameHandle::performRequiredTask(Function<bool(FrameHandle &)> &&perform, Function<void(FrameHandle &, bool)> &&complete,
		Ref *ref, StringView tag) {
	++ _tasksRequired;
	auto linkId = retain();
	_loop->performInQueue(Rc<thread::Task>::create([this, perform = sp::move(perform)] (const thread::Task &) -> bool {
		return perform(*this);
	}, [this, complete = sp::move(complete), linkId, tag = tag.str<Interface>()] (const thread::Task &, bool success) {
		XL_FRAME_PROFILE(complete(*this, success), tag, 1000);
		XL_FRAME_LOG(XL_FRAME_LOG_INFO, "thread performed: '", tag, "'");
		if (success) {
			onRequiredTaskCompleted(tag);
		} else {
			invalidate();
			log::error("FrameHandle", "Async task failed: ", tag);
		}
		release(linkId);
	}, ref));
}

bool FrameHandle::isValid() const {
	return _valid && (!_request->getEmitter() || _request->getEmitter()->isFrameValid(*this));
}

bool FrameHandle::isPersistentMapping() const {
	return _request->isPersistentMapping();
}

Rc<AttachmentInputData> FrameHandle::getInputData(const AttachmentData *attachment) {
	return _request->getInputData(attachment);
}

bool FrameHandle::isReadyForSubmit() const {
	return _request->isReadyForSubmit();
}

void FrameHandle::setReadyForSubmit(bool value) {
	if (!isValid()) {
		XL_FRAME_LOG(XL_FRAME_LOG_INFO, "[invalid] frame ready to submit");
		return;
	}

	XL_FRAME_LOG(XL_FRAME_LOG_INFO, "frame ready to submit");
	_request->setReadyForSubmit(value);
	if (_request->isReadyForSubmit()) {
		_loop->performOnGlThread([this] {
			update();
		}, this);
	}
}

void FrameHandle::invalidate() {
	if (_loop->isOnThisThread()) {
		if (_valid) {
			if (!_timeEnd) {
				_timeEnd = platform::clock(FrameClockType);
			}

			if (auto e = _request->getEmitter()) {
				XL_FRAME_LOG(XL_FRAME_LOG_INFO, "complete: ", e->getFrameTime());
			}

			_valid = false;
			_completed = true;

			HashMap<const AttachmentData *, FrameAttachmentData *> attachments;
			for (auto &it : _queues) {
				for (auto &iit : it->getAttachments()) {
					if (iit.second.handle->isOutput()) {
						attachments.emplace(iit.first, (FrameAttachmentData *)&iit.second);
					}
				}
				it->invalidate();
			}

			if (!_submitted) {
				_submitted = true;
				if (_request->getEmitter()) {
					_request->getEmitter()->setFrameSubmitted(*this);
				}
			}

			if (_complete) {
				_complete(*this);
			}

			if (_request) {
				_request->finalize(*_loop, attachments, _valid);
				for (auto &it : _queues) {
					_request->signalDependencies(*_loop, it->getQueue(), _valid);
				}
			}
		}
	} else {
		_loop->performOnGlThread([this] {
			invalidate();
		}, this);
	}
}

void FrameHandle::setCompleteCallback(Function<void(FrameHandle &)> &&cb) {
	_complete = sp::move(cb);
}

bool FrameHandle::setup() {
	_pool->perform([&, this] {
		auto q = Rc<FrameQueue>::create(_pool, _request->getQueue(), *this);
		q->setup();

		_queues.emplace_back(sp::move(q));
	});

	if (!_valid) {
		for (auto &it : _queues) {
			it->invalidate();
		}
	}

	if (_request) {
		_request->attachFrame(this);
	}

	return true;
}

void FrameHandle::onQueueSubmitted(FrameQueue &queue) {
	++ _queuesSubmitted;
	if (_queuesSubmitted == _queues.size()) {
		_submitted = true;
		if (_request->getEmitter()) {
			_request->getEmitter()->setFrameSubmitted(*this);
		}
	}
}

void FrameHandle::onQueueComplete(FrameQueue &queue) {
	_submissionTime += queue.getSubmissionTime();
	++ _queuesCompleted;
	tryComplete();
}

void FrameHandle::onRequiredTaskCompleted(StringView tag) {
	++ _tasksCompleted;
	tryComplete();
}

bool FrameHandle::onOutputAttachment(FrameAttachmentData &data) {
	return _request->onOutputReady(*_loop, data);
}

void FrameHandle::onOutputAttachmentInvalidated(FrameAttachmentData &data) {
	_request->onOutputInvalidated(*_loop, data);
}

void FrameHandle::waitForDependencies(const Vector<Rc<DependencyEvent>> &events, Function<void(FrameHandle &, bool)> &&cb) {
	auto linkId = retain();
	_loop->waitForDependencies(events, [this, cb = sp::move(cb), linkId] (bool success) {
		cb(*this, success);
		release(linkId);
	});
}

void FrameHandle::waitForInput(FrameQueue &queue, const Rc<AttachmentHandle> &a, Function<void(bool)> &&cb) {
	_request->waitForInput(queue, a, sp::move(cb));
}

void FrameHandle::signalDependencies(bool success) {
	for (auto &it : _queues) {
		_request->signalDependencies(*_loop, it->getQueue(), _valid);
	}
}

void FrameHandle::onQueueInvalidated(FrameQueue &) {
	++ _queuesCompleted;
	invalidate();
}

void FrameHandle::tryComplete() {
	if (_tasksCompleted == _tasksRequired.load() && _queuesCompleted == _queues.size()) {
		onComplete();
	}
}

void FrameHandle::onComplete() {
	if (!_completed && _valid) {
		_timeEnd = platform::clock(FrameClockType);
		if (auto e = getEmitter()) {
			XL_FRAME_LOG(XL_FRAME_LOG_INFO, "complete: ", e->getFrameTime());
		}
		_completed = true;

		HashMap<const AttachmentData *, FrameAttachmentData *> attachments;
		for (auto &it : _queues) {
			for (auto &iit : it->getAttachments()) {
				if (iit.second.handle->isOutput()) {
					attachments.emplace(iit.first, (FrameAttachmentData *)&iit.second);
				}
			}
		}

		if (_complete) {
			_complete(*this);
		}

		_request->finalize(*_loop, attachments, _valid);

		for (auto &it : _queues) {
			_request->signalDependencies(*_loop, it->getQueue(), _valid);
		}
	}
}

}
