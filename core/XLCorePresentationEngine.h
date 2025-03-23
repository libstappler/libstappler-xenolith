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
#include "SPEventHandle.h"

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
		// TODO -пока не реализовано
		bool renderImageOffscreen = false;

		// Начинать новый кадр как только предыдущий был отправлен на исполнение (то есть, до его завершения и презентации)
		bool preStartFrame = true;

		// Использовать временное окно для презентации кадра
		// Вместо презентации по готовности, система будет стараться удерживать целевую частоту кадров за счёт откладывания
		// презентации до следующего окна времени
		// Не работает в режиме followDisplayLink
		bool usePresentWindow = true;

		// Экспериментально: отправлять кадр на презентацию сразу после его отправки в обработку. Может снизить видимую задержку ввода.
		// На текущий момент, работает нестабильно для режима FIFO
		bool earlyPresent = false;
	};

	virtual ~PresentationEngine();

	virtual bool run();
	virtual void end();

	virtual bool recreateSwapchain(core::PresentMode) = 0;
	virtual bool createSwapchain(const core::SurfaceInfo &, core::SwapchainConfig &&cfg, core::PresentMode presentMode) = 0;
	virtual void deprecateSwapchain(bool fast);

	virtual bool present(PresentationFrame *frame);
	virtual bool presentImmediate(PresentationFrame *frame) { return false; }

	virtual void update(bool displayLink);

	void setTargetFrameInterval(uint64_t);
	uint64_t getTargetFrameInterval() const { return _targetFrameInterval; }

	bool isFrameValid(const PresentationFrame *) const;

	Rc<FrameHandle> submitNextFrame(Rc<FrameRequest> &&);

	bool waitUntilFramePresentation();

	void scheduleNextImage(Function<void(PresentationFrame *, bool)> && = nullptr,
			PresentationFrame::Flags frameFlags = PresentationFrame::None);

	bool scheduleSwapchainImage(Rc<PresentationFrame> &&);

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

	void setContentPadding(const Padding &padding);

	bool isRunning() const;

	virtual bool handleFrameStarted(PresentationFrame *);
	virtual void handleFrameInvalidated(PresentationFrame *);
	virtual void handleFrameReady(PresentationFrame *);
	virtual void handleFramePresented(PresentationFrame *);
	virtual void handleFrameComplete(PresentationFrame *);

protected:
#if SP_REF_DEBUG
	virtual bool isRetainTrackerEnabled() const override {
		return true;
	}
#endif

	virtual void acquireFrameData(PresentationFrame *, Function<void(core::PresentationFrame *)> &&) = 0;

	void scheduleSwapchainRecreation();

	void resetFrames();

	void scheduleImage(PresentationFrame *frame);

	bool acquireScheduledImage();

	void handleSwapchainImageReady(Rc<Swapchain::SwapchainAcquiredImage> &&image);

	void runScheduledPresent(PresentationFrame *frame);
	void presentSwapchainImage(Rc<DeviceQueue> &&queue, PresentationFrame *frame);

	void presentWithQueue(DeviceQueue &queue, PresentationFrame *frame);

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

	bool _running = false;
	bool _readyForNextFrame = false;
	bool _waitUntilFrame = false;

	// New frames, that waits next swapchain image
	std::deque<Rc<PresentationFrame>> _framesAwaitingImages;

	// Frames, waiting to be presented
	Vector<Rc<PresentationFrame>> _scheduledForPresent;

	// Handles, waiting for their present windows
	Set<Rc<event::Handle>> _scheduledPresentHandles;

	// Async request for a swapchain images
	Set<Swapchain::SwapchainAcquiredImage *> _requestedSwapchainImage;

	// Already acquired swapchain images
	std::deque<Rc<Swapchain::SwapchainAcquiredImage>> _acquiredSwapchainImages;

	Set<PresentationFrame *> _activeFrames;
	Set<PresentationFrame *> _totalFrames;
};

}
#endif /* XENOLITH_CORE_XLCOREPRESENTATIONENGINE_H_ */
