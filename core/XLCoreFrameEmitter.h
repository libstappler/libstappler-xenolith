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

#ifndef XENOLITH_CORE_XLCOREFRAMEEMITTER_H_
#define XENOLITH_CORE_XLCOREFRAMEEMITTER_H_

#include "XLCoreAttachment.h"
#include "XLCoreFrameHandle.h"
#include "SPMovingAverage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

// Frame emitter is an interface, that continuously spawns frames, and can control validity of a frame
class SP_PUBLIC FrameEmitter : public Ref {
public:
	virtual ~FrameEmitter();

	virtual bool init(const Rc<Loop> &, uint64_t frameInterval);
	virtual void invalidate();

	virtual void setFrameSubmitted(FrameHandle &);
	virtual bool isFrameValid(const FrameHandle &handle);

	virtual void acquireNextFrame();

	virtual void dropFrameTimeout();

	void dropFrames();

	bool isValid() const { return _valid; }

	void setFrameTime(uint64_t v) { _frame = v; }
	uint64_t getFrameTime() const { return _frame; }

	void setFrameInterval(uint64_t v) { _frameInterval = v; }
	uint64_t getFrameInterval() const { return _frameInterval; }

	const Rc<Loop> &getLoop() const { return _loop; }

	uint64_t getLastFrameTime() const;
	uint64_t getAvgFrameTime() const;
	uint64_t getAvgFenceTime() const;

	bool isReadyForSubmit() const;

	void setEnableBarrier(bool value);

	Rc<FrameRequest> makeRequest(const FrameContraints &);
	Rc<FrameHandle> submitNextFrame(Rc<FrameRequest> &&);

protected:
	virtual void onFrameEmitted(FrameHandle &);
	virtual void onFrameSubmitted(FrameHandle &);
	virtual void onFrameComplete(FrameHandle &);
	virtual void onFrameTimeout(uint64_t order);
	virtual void onFrameRequest(bool timeout);

	virtual Rc<FrameHandle> makeFrame(Rc<FrameRequest> &&, bool readyForSubmit);
	virtual bool canStartFrame() const;
	virtual void scheduleNextFrame(Rc<FrameRequest> &&);
	virtual void scheduleFrameTimeout();

	uint64_t _submitted = 0;
	uint64_t _order = 0;
	uint64_t _gen = 0;

	bool _valid = true;
	std::atomic<uint64_t> _frame = 0;
	uint64_t _frameInterval = 1'000'000 / 60;
	uint64_t _suboptimal = 0;

	bool _frameTimeoutPassed = true;
	bool _nextFrameAcquired = false;
	bool _onDemand = true;
	bool _enableBarrier = true;
	Rc<FrameRequest> _nextFrameRequest;
	std::deque<Rc<FrameHandle>> _frames;
	std::deque<Rc<FrameHandle>> _framesPending;

	Rc<Loop> _loop;

	uint64_t _lastSubmit = 0;

	std::atomic<uint64_t> _lastFrameTime = 0;
	math::MovingAverage<20, uint64_t> _avgFrameTime;
	std::atomic<uint64_t> _avgFrameTimeValue = 0;

	math::MovingAverage<20, uint64_t> _avgFenceInterval;
	std::atomic<uint64_t> _avgFenceIntervalValue = 0;

	uint64_t _lastTotalFrameTime = 0;
};

}

#endif /* XENOLITH_CORE_XLCOREFRAMEEMITTER_H_ */
