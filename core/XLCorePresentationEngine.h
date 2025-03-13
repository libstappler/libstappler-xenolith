/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_CORE_XLCOREPRESENTATIONENGINE_H_
#define XENOLITH_CORE_XLCOREPRESENTATIONENGINE_H_

#include "XLCore.h"
#include "XLCoreDeviceQueue.h"
#include "XLCoreSwapchain.h"
#include "XLCorePresentationFrame.h"
#include "SPMovingAverage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class PresentationFrame;

class PresentationEngine : public Ref {
public:
	static constexpr size_t FrameAverageCount = 20;

	struct FrameTimeInfo {
		uint64_t dt;
		uint64_t avg;
		uint64_t clock;
	};

	struct Options {
		// Запускать следующий кадр только по запросу либо наличию действий в процессе
		// В таком случае, PresentationInterval будет считаться на основе реальной презентации
		// Показательными для производительности остаются интервал подготовки кадра и
		// таймер использования GPU
		bool renderOnDemand = true;

		// Использовать внешний сигнал вертикальной синхронизации (система должна поддерживать)
		// В этом режиме готовые к презентации кадры ожидают сигнала, прежде, чем отправиться
		// Также, по сигналу система запрашиваает новый буфер для отрисовки следующего кадра
		// Если система не успела подготовить новый кадр - обновление пропускается
		bool followDisplayLink = false;

		// Использовать внеэкранный рендеринг для подготовки изображений. В этом режиме презентация нового изображения выполняется
		// строго синхронно (см. presentImmediate)
		bool renderImageOffscreen = false;

		// by default, we use vkAcquireNextImageKHR in lockfree manner, but in some cases blocking variant
		// is more preferable. If this option is set, vkAcquireNextImageKHR called with UIN64_MAX timeout
		// be careful not to block whole view's thread operation on this
		bool acquireImageImmediately = true;

		// on some systems, we can not acquire next image until queue operations on previous image is finished
		// on this system, we wait on last swapchain pass fence before acquire swapchain image
		// swapchain-independent passes is not affected by this option
		bool waitOnSwapchainPassFence = false;
	};

	virtual ~PresentationEngine() = default;

	virtual bool run();
	virtual void end();

	virtual bool recreateSwapchain(core::PresentMode) = 0;
	virtual bool createSwapchain(const core::SurfaceInfo &, core::SwapchainConfig &&cfg, core::PresentMode presentMode) = 0;
	virtual void deprecateSwapchain(bool fast);

	virtual bool present(PresentationFrame *frame);
	virtual bool presentImmediate(PresentationFrame *frame) { return false; }

	virtual void update(bool displayLink);
	virtual void invalidate();

	void setTargetFrameInterval(uint64_t);

	bool isFrameValid(const PresentationFrame *) const;

	Rc<FrameHandle> submitNextFrame(Rc<FrameRequest> &&);

	void scheduleNextImage(Function<void(PresentationFrame *, bool)> && = nullptr,
			PresentationFrame::Flags frameFlags = PresentationFrame::None);

	bool scheduleSwapchainImage(Rc<PresentationFrame> &&);

	void presentWithQueue(DeviceQueue &queue, PresentationFrame *frame);

	Swapchain *getSwapchain() const { return _swapchain; }

	const FrameConstraints &getFrameConstraints() const { return _constraints; }

	FrameTimeInfo updatePresentationInterval();

	uint64_t getLastFrameInterval() const;
	uint64_t getAvgFrameInterval() const;
	uint64_t getLastFrameTime() const;
	uint64_t getLastDeviceFrameTime() const;

	void setReadyForNextFrame();

	void setRenderOnDemand(bool value);
	bool isRenderOnDemand() const;

	bool isRunning() const;

	virtual bool handleFrameStarted(PresentationFrame *);
	virtual void handleFrameInvalidated(PresentationFrame *);
	virtual void handleFramePresented(PresentationFrame *);
	virtual void handleFrameCancel(PresentationFrame *);

protected:
	virtual void acquireFrameData(PresentationFrame *, Function<void(core::PresentationFrame *)> &&) = 0;

	void scheduleSwapchainRecreation();

	void resetFrames();

	bool acquireScheduledImageImmediate(PresentationFrame *frame);
	bool acquireScheduledImage();
	void handleSwapchainImageReady(Rc<Swapchain::SwapchainAcquiredImage> &&image);

	void scheduleImage(PresentationFrame *frame);

	void scheduleFence(Rc<Fence> &&fence);

	// Синхронно ожидать до завершения всех операций ранее переданного порядка
	void waitForFences(uint64_t min);

	// Обойхи ожидающие операции, убрать завершённые
	void updateFences();

	void runScheduledPresent(PresentationFrame *frame);
	void presentSwapchainImage(Rc<DeviceQueue> &&queue, PresentationFrame *frame);

	void schedulePresent(PresentationFrame *frame, uint64_t);

	void clearImages();

	Options _options;
	FrameConstraints _constraints;

	Device *_device = nullptr;

	Rc<Swapchain> _swapchain;
	Rc<Loop> _loop;

	// время, после которого нужно выпускать следующий кадрр
	// расчитывается как премя последней презентации + целевой кадроый интервал
	uint64_t _nextPresentWindow = 0;

	// целевой кадроый интервал в режиме постоянной презентации (в микросекундах)
	uint64_t _targetFrameInterval = 0; // 1'000'000 / 60;

	// интервал обновления системы (приблизительная частота вызова update) (в микросекундах)
	uint64_t _engineUpdateInterval = 250;

	// Presentation interval is not the same, as frame interval, it's time between two present event
	uint64_t _lastPresentationTime = 0;
	std::atomic<uint64_t> _lastPresentationInterval = 0;
	math::MovingAverage<FrameAverageCount, uint64_t> _avgPresentationInterval;
	std::atomic<uint64_t> _avgPresentationIntervalValue = 0;

	uint64_t _lastFrameTime = 0;
	math::MovingAverage<FrameAverageCount, uint64_t> _avgFrameTime;
	std::atomic<uint64_t> _avgFrameTimeValue = 0;

	uint64_t _lastDeviceFrameTime = 0;
	math::MovingAverage<FrameAverageCount, uint64_t> _avgFenceInterval;
	std::atomic<uint64_t> _avgFenceIntervalValue = 0;

	uint64_t _frameOrder = 0; // current scheduled frame order
	uint64_t _fenceOrder = 0; // last released frame order

	bool _running = false;
	bool _readyForNextFrame = false;

	// Presentation or acquisition fences
	Vector<Rc<Fence>> _fences;

	// New frames, that waits on view's fences
	Vector<Rc<PresentationFrame>> _fenceFrames;

	// New frames, that waits next swapchain image
	std::deque<Rc<PresentationFrame>> _framesAwaitingImages;

	// Frames, waiting to be presented
	// Frames can be scheduled for present in DisplayLink mode
	// or if frame become ready for presentation before it's present window and targetFrameInterval is set.
	// Frame presentation scheduling normally should do most of vsync job instead of internal driver fence,
	// that can improve general system (not application, but whole system) performance.
	// Triggering vsync fence for a long time can cause stuttering on other GPU-accelerated windows.
	Vector<Rc<PresentationFrame>> _scheduledForPresent;

	// Async request for a swapchain images
	Set<Swapchain::SwapchainAcquiredImage *> _requestedSwapchainImage;

	// Already acquired swapchain images
	std::deque<Rc<Swapchain::SwapchainAcquiredImage>> _acquiredSwapchainImages;

	Set<PresentationFrame *> _activeFrames;
};

}
#endif /* XENOLITH_CORE_XLCOREPRESENTATIONENGINE_H_ */
