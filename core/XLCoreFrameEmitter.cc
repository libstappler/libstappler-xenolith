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

#include "XLCoreFrameEmitter.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameCache.h"
#include "XLCoreQueue.h"
#include "XLCoreLoop.h"
#include "XLCorePlatform.h"
#include "XLCoreFrameRequest.h"

namespace stappler::xenolith::core {

FrameEmitter::~FrameEmitter() { }

bool FrameEmitter::init(const Rc<Loop> &loop, uint64_t frameInterval) {
	_frameInterval = frameInterval;
	_loop = loop;

	_avgFrameTime.reset(0);
	_avgFrameTimeValue = 0;

	return true;
}

void FrameEmitter::invalidate() {
	_valid = false;
	auto frames = _frames;
	for (auto &it : frames) {
		it->invalidate();
	}
	_frames.clear();
}

void FrameEmitter::setFrameSubmitted(FrameHandle &frame) {
	if (!_loop->isOnGlThread()) {
		return;
	}

	XL_FRAME_EMITTER_LOG("FrameTime:        ", _frame.load(), "   ", platform::clock(ClockType::Monotonic) - _frame.load(), " mks");

	auto it = _frames.begin();
	while (it != _frames.end()) {
		if ((*it) == &frame) {
			if (frame.isValid()) {
				_framesPending.emplace_back(&frame);
			}
			it = _frames.erase(it);
		} else {
			++ it;
		}
	}

	XL_PROFILE_BEGIN(success, "FrameEmitter::setFrameSubmitted", "success", 500);
	XL_PROFILE_BEGIN(onFrameSubmitted, "FrameEmitter::setFrameSubmitted", "onFrameSubmitted", 500);
	onFrameSubmitted(frame);
	XL_PROFILE_END(onFrameSubmitted)

	++ _submitted;
	XL_PROFILE_BEGIN(onFrameRequest, "FrameEmitter::setFrameSubmitted", "onFrameRequest", 500);
	if (!_onDemand) {
		onFrameRequest(false);
	}
	XL_PROFILE_END(onFrameRequest)
	XL_PROFILE_END(success)
}

bool FrameEmitter::isFrameValid(const FrameHandle &frame) {
	if (_valid && frame.getGen() == _gen && std::find(_frames.begin(), _frames.end(), &frame) != _frames.end()) {
		return true;
	}
	return false;
}

void FrameEmitter::acquireNextFrame() { }

void FrameEmitter::dropFrameTimeout() {
	_loop->performOnGlThread([this] {
		if (!_frameTimeoutPassed) {
			++ _order; // increment timeout timeline
			onFrameTimeout(_order);
		}
	}, this, true);
}

void FrameEmitter::dropFrames() {
	if (!_loop->isOnGlThread()) {
		return;
	}

	for (auto &it : _frames) {
		it->invalidate();
	}
	_frames.clear();
	_framesPending.clear();
}

uint64_t FrameEmitter::getLastFrameTime() const {
	return _lastFrameTime;
}
uint64_t FrameEmitter::getAvgFrameTime() const {
	return _avgFrameTimeValue;
}
uint64_t FrameEmitter::getAvgFenceTime() const {
	return _avgFenceIntervalValue;
}

bool FrameEmitter::isReadyForSubmit() const {
	return _frames.empty() && _framesPending.empty();
}

void FrameEmitter::setEnableBarrier(bool value) {
	_enableBarrier = value;
}

void FrameEmitter::onFrameEmitted(FrameHandle &) { }

void FrameEmitter::onFrameSubmitted(FrameHandle &) { }

void FrameEmitter::onFrameComplete(FrameHandle &frame) {
	if (!_loop->isOnGlThread()) {
		return;
	}

	_lastFrameTime = frame.getTimeEnd() - frame.getTimeStart();
	_avgFrameTime.addValue(frame.getTimeEnd() - frame.getTimeStart());
	_avgFrameTimeValue = _avgFrameTime.getAverage(true);

	if (auto t = frame.getSubmissionTime()) {
		_avgFenceInterval.addValue(t);
		_avgFenceIntervalValue = _avgFenceInterval.getAverage(true);
	}

	auto it = _framesPending.begin();
	while (it != _framesPending.end()) {
		if ((*it) == &frame) {
			it = _framesPending.erase(it);
		} else {
			++ it;
		}
	}

	if (_framesPending.size() <= 1 && _frames.empty() && !_onDemand) {
		onFrameRequest(false);
	}

	if (_framesPending.empty()) {
		for (auto &it : _frames) {
			if (!it->isReadyForSubmit()) {
				it->setReadyForSubmit(true);
				break;
			}
		}
	}
}

void FrameEmitter::onFrameTimeout(uint64_t order) {
	if (order == _order) {
		_frameTimeoutPassed = true;
		onFrameRequest(true);
	}
}

void FrameEmitter::onFrameRequest(bool timeout) {
	if (canStartFrame()) {
		auto next = platform::clock();

		if (_nextFrameRequest) {
			scheduleFrameTimeout();
			submitNextFrame(move(_nextFrameRequest));
		} else if (!_nextFrameAcquired) {
			if (_frame.load()) {
				XL_FRAME_EMITTER_LOG(timeout ? "FrameRequest [T]: " : "FrameRequest [S]: ", _frame.load(), "   ",
						next - _frame.load(), " mks");
			}
			_frame = next;
			_nextFrameAcquired = true;
			scheduleFrameTimeout();
			acquireNextFrame();
		}
	}
}

Rc<FrameHandle> FrameEmitter::makeFrame(Rc<FrameRequest> &&req, bool readyForSubmit) {
	if (!_valid) {
		return nullptr;
	}

	req->setReadyForSubmit(readyForSubmit);
	return _loop->makeFrame(move(req), _gen);
}

bool FrameEmitter::canStartFrame() const {
	if (!_valid || !_frameTimeoutPassed) {
		return false;
	}

	if (_frames.empty()) {
		return _framesPending.size() <= 1;
	}

	for (auto &it : _frames) {
		if (!it->isSubmitted()) {
			return false;
		}
	}

	return _framesPending.size() <= 1;
}

void FrameEmitter::scheduleNextFrame(Rc<FrameRequest> &&req) {
	_nextFrameRequest = move(req);
}

void FrameEmitter::scheduleFrameTimeout() {
	if (_valid && _frameInterval && _frameTimeoutPassed && !_onDemand) {
		_frameTimeoutPassed = false;
		++ _order;
		[[maybe_unused]] auto t = platform::clock();
		_loop->schedule([=, this, guard = Rc<FrameEmitter>(this), idx = _order] (Loop &ctx) {
			XL_FRAME_EMITTER_LOG("TimeoutPassed:    ", _frame.load(), "   ", platform::clock() - _frame.load(), " (",
					platform::clock(ClockType::Monotonic) - t, ") mks");
			guard->onFrameTimeout(idx);
			return true; // end spinning
		}, _frameInterval - config::FrameIntervalSafeOffset, "FrameEmitter::scheduleFrameTimeout");
	}
}

Rc<FrameRequest> FrameEmitter::makeRequest(const FrameContraints &constraints) {
	_frame = platform::clock();
	return Rc<FrameRequest>::create(this, constraints);
}

Rc<FrameHandle> FrameEmitter::submitNextFrame(Rc<FrameRequest> &&req) {
	if (!_valid) {
		return nullptr;
	}

	bool readyForSubmit = !_enableBarrier || (_frames.empty() && _framesPending.empty());
	auto frame = makeFrame(move(req), readyForSubmit);
	_nextFrameRequest = nullptr;
	if (frame && frame->isValidFlag()) {
		auto now = platform::clock();
		_lastSubmit = now;

		frame->setCompleteCallback([this] (FrameHandle &frame) {
			onFrameComplete(frame);
		});

		XL_FRAME_EMITTER_LOG("SubmitNextFrame:  ", _frame.load(), "   ", platform::clock(ClockType::Monotonic) - _frame.load(), " mks ", readyForSubmit);

		_nextFrameAcquired = false;
		onFrameEmitted(*frame);
		frame->update(true);
		if (frame->isValidFlag()) {
			if (_frames.empty() && _framesPending.empty() && !frame->isReadyForSubmit()) {
				_frames.push_back(frame);
				frame->setReadyForSubmit(true);
			} else {
				_frames.push_back(frame);
			}
		}
		return frame;
	}
	return nullptr;
}

}
