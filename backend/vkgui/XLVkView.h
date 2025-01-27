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

#ifndef XENOLITH_BACKEND_VKGUI_XLVKVIEW_H_
#define XENOLITH_BACKEND_VKGUI_XLVKVIEW_H_

#include "XLVk.h"
#include "XLView.h"
#include "XLVkSwapchain.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class SP_PUBLIC View : public xenolith::View {
public:
	struct EngineOptions {
		// on some systems, we can not acquire next image until queue operations on previous image is finished
		// on this system, we wait on last swapchain pass fence before acquire swapchain image
		// swapchain-independent passes is not affected by this option
		bool waitOnSwapchainPassFence = false;

		// by default, we use vkAcquireNextImageKHR in lockfree manner, but in some cases blocking variant
		// is more preferable. If this option is set, vkAcquireNextImageKHR called with UIN64_MAX timeout
		// be careful not to block whole view's thread operation on this
		bool acquireImageImmediately = false;

		// Использовать внешний сигнал вертикальной синхронизации (система должна поддерживать)
		// В этом режиме готовые к презентации кадры ожидают сигнала, прежде, чем отправиться
		// Также, по сигналу система запрашиваает новый буфер для отрисовки следующего кадра
		// Если система не успела подготовить новый кадр - обновление пропускается
		bool followDisplayLink = false;

		// По умолчанию, FrameEmitter допускает только один кадр определенной RenderQueue в процессе отправки
		// (между vkQueueSubmit и освобождением соответсвующей VkFence). Исключение составляют проходы, помеченные как асинхронные
		// Если отключить это ограничение, единственным ограничением между vkQueueSubmit останутся внутренние примитивы синхронизации
		// В ряде случаев, неограниченная отправка буферов может привести к некорректной работе vkQueueSubmit и блокированию треда на
		// этой функции. Рекомендуется сохранять это ограничение. а для полной загрузки GPU использовать асинхронные пре- и пост-проходы
		bool enableFrameEmitterBarrier = false;

		// Использовать внеэкранный рендеринг для подготовки изображений. В этом режиме презентация нового изображения выполняется
		// строго синхронно (см. presentImmediate)
		bool renderImageOffscreen = false;

		// Не использовать переключение потоков для вывода изображения. Вместо этого, блокироваться на ожидании очереди в текущем потоке
		bool presentImmediate = false;

		// Запускать следующий кадр только по запросу либо наличию действий в процессе
		bool renderOnDemand = true;

		// Запускать кадр синхронно после пересоздания swapchain
		bool syncFrameAfterSwapchainRecreation = true;
	};

	virtual ~View();

	virtual bool init(Application &, const Device &, ViewInfo &&);

	virtual void threadInit() override;
	virtual void threadDispose() override;

	virtual void update(bool displayLink) override;

	virtual void run();
	virtual void runWithQueue(const Rc<RenderQueue> &) override;

	virtual void onAdded(Device &);
	virtual void onRemoved();

	virtual void deprecateSwapchain(bool fast = false) override;

	virtual bool present(ImageStorage *) override;
	virtual bool presentImmediate(ImageStorage *, Function<void(bool)> &&scheduleCb, bool isRegularFrame) override;
	virtual void invalidateTarget(ImageStorage *) override;

	virtual Rc<Ref> getSwapchainHandle() const override;

	virtual void captureImage(StringView, const Rc<core::ImageObject> &image, AttachmentLayout l) const override;
	virtual void captureImage(Function<void(const ImageInfoData &info, BytesView view)> &&,
			const Rc<core::ImageObject> &image, AttachmentLayout l) const override;

	void scheduleFence(Rc<Fence> &&);

	virtual uint64_t getUpdateInterval() const { return 0; }

	virtual void mapWindow();

	vk::Device *getDevice() const { return _device; }

	virtual void setReadyForNextFrame() override;

	virtual void setRenderOnDemand(bool value) override;
	virtual bool isRenderOnDemand() const override;

protected:
	using xenolith::View::init;

#if SP_REF_DEBUG
	virtual bool isRetainTrackerEnabled() const override {
		return false;
	}
#endif

	enum ScheduleImageMode {
		AcquireSwapchainImageAsync,
		AcquireSwapchainImageImmediate,
		AcquireOffscreenImage,
	};

	struct FrameTimeInfo {
		uint64_t dt;
		uint64_t avg;
		uint64_t clock;
	};

	struct ImageSyncInfo : public Ref {
		std::mutex mutex;
		std::condition_variable cond;

		bool success = false;
		Rc<ImageStorage> resultImage;
		Function<void()> resultCallback;
	};

	virtual bool pollInput(bool frameReady);

	virtual core::SurfaceInfo getSurfaceOptions() const;

	void invalidate();

	void scheduleNextImage(uint64_t windowOffset, bool immediately);

	void scheduleNextImageSync(uint64_t timeout);

	// Начать подготовку нового изображения для презентации
	// Создает объект кадра и начинает сбор данных для его рисования
	// Создает объект изображения и начинает цикл его захвата
	// Если режим acquireImageImmediately - блокирует поток до успешного захвата изображения
	// windowOffset - интервал от текущего момента. когда предполагается презентовать изображение
	//   Служит для ограничения частоты кадров
	bool scheduleSwapchainImage(uint64_t windowOffset, ScheduleImageMode, ImageSyncInfo * = nullptr);

	// Попытаться захватить изображение для отрисовки кадра. Если задан флаг immediate
	// или включен режим followDisplayLink - блокирует поток до успешного захвата
	// В противном случае, если захват не удался - необходимо поробовать позже
	bool acquireScheduledImageImmediate(const Rc<SwapchainImage> &);
	bool acquireScheduledImage();

	void scheduleImage(Rc<SwapchainImage> &&);
	void onSwapchainImageReady(Rc<SwapchainHandle::SwapchainAcquiredImage> &&);

	virtual bool recreateSwapchain(core::PresentMode);
	virtual bool createSwapchain(const core::SurfaceInfo &, core::SwapchainConfig &&cfg, core::PresentMode presentMode);

	bool isImagePresentable(const core::ImageObject &image, VkFilter &filter) const;

	// Презентует отложенное подготовленное (кадр завершён) изображение
	void runScheduledPresent(SwapchainImage *);
	void presentSwapchainImage(Rc<DeviceQueue> &&queue, SwapchainImage *);

	virtual void presentWithQueue(DeviceQueue &, Rc<ImageStorage> &&);
	void invalidateSwapchainImage(Rc<ImageStorage> &&);

	FrameTimeInfo updateFrameInterval();

	void waitForFences(uint64_t min);

	virtual void finalize();

	void updateFences();

	void clearImages();

	virtual void schedulePresent(SwapchainImage *, uint64_t);

	EngineOptions _options;

	bool _readyForNextFrame = false;
	bool _blockSwapchainRecreation = false;
	bool _swapchainInvalidated = false;
	uint64_t _refId = 0;
	uint64_t _framesInProgress = 0;
	uint64_t _fenceOrder = 0;
	uint64_t _frameOrder = 0;
	uint64_t _onDemandOrder = 1;
	uint64_t _scheduledTime = 0;
	uint64_t _nextPresentWindow = 0;
	Rc<Surface> _surface;
	Rc<Instance> _instance;
	Rc<Device> _device;
	Rc<SwapchainHandle> _swapchain;
	String _threadName;

	Rc<core::ImageStorage> _initImage;
	Vector<Rc<Fence>> _fences;

	Vector<Rc<SwapchainImage>> _fenceImages;
	std::deque<Rc<SwapchainImage>> _scheduledImages;
	Vector<Rc<SwapchainImage>> _scheduledPresent;
	Set<SwapchainHandle::SwapchainAcquiredImage *> _requestedSwapchainImage;
	std::deque<Rc<SwapchainHandle::SwapchainAcquiredImage>> _swapchainImages;
};

}

#endif /* XENOLITH_BACKEND_VKGUI_XLVKVIEW_H_ */
