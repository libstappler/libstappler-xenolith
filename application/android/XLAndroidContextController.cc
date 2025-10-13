/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "XLAndroidContextController.h"
#include "XLAndroidDisplayConfigManager.h"
#include "XLAndroidActivity.h"
#include "XLAndroid.h"
#include "SPThread.h"

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkInstance.h"
#endif

#include <android/native_activity.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

void AndroidContextController::acquireDefaultConfig(ContextConfig &cfg) {
	auto env = jni::Env::getEnv();
	auto config = stappler::platform::ApplicationInfo::getCurrent();

	auto formatSupport = NativeBufferFormatSupport::get();

	if (config->isEmulator) {
		// emulators often do not support this format for swapchains
		formatSupport.R8G8B8A8_UNORM = false;
		formatSupport.R8G8B8X8_UNORM = false;
	}

	env.checkErrors();

	if (!cfg.context) {
		cfg.context = Rc<ContextInfo>::alloc();
	}

	cfg.context->bundleName = config->bundleName.str<Interface>();
	cfg.context->appName = config->applicationName.str<Interface>();
	cfg.context->appVersion = config->applicationVersion.str<Interface>();
	cfg.context->userLanguage = config->locale.str<Interface>();
	cfg.context->userAgent = config->userAgent.str<Interface>();
	cfg.context->appVersionCode = SP_MAKE_API_VERSION(cfg.context->appVersion);

	if (!cfg.loop) {
		cfg.loop = Rc<core::LoopInfo>::alloc();
	}

	if (formatSupport.valid) {
		if (formatSupport.R8G8B8A8_UNORM || formatSupport.R8G8B8X8_UNORM) {
			cfg.loop->defaultFormat = cfg.window->imageFormat = core::ImageFormat::R8G8B8A8_UNORM;
		} else if (formatSupport.R10G10B10A2_UNORM) {
			cfg.loop->defaultFormat = cfg.window->imageFormat =
					core::ImageFormat::A2B10G10R10_UNORM_PACK32;
		} else if (formatSupport.R16G16B16A16_FLOAT) {
			cfg.loop->defaultFormat = cfg.window->imageFormat =
					core::ImageFormat::R16G16B16A16_SFLOAT;
		} else if (formatSupport.R5G6B5_UNORM) {
			cfg.loop->defaultFormat = cfg.window->imageFormat =
					core::ImageFormat::R5G6B5_UNORM_PACK16;
		}
	} else {
		cfg.loop->defaultFormat = cfg.window->imageFormat = core::ImageFormat::R8G8B8A8_UNORM;
	}

	if (!cfg.instance) {
		cfg.instance = Rc<core::InstanceInfo>::alloc();
	}

	cfg.instance->api = core::InstanceApi::Vulkan;
	cfg.instance->flags = core::InstanceFlags::Validation;
}

AndroidContextController::~AndroidContextController() {
	auto app = jni::Env::getApp();
	if (app) {
		app->setActivityLoader(nullptr);
	}
}

bool AndroidContextController::init(NotNull<Context> ctx, ContextConfig &&config) {
	if (!ContextController::init(ctx)) {
		return false;
	}

	_contextInfo = move(config.context);
	_windowInfo = move(config.window);
	_instanceInfo = move(config.instance);
	_loopInfo = move(config.loop);

	auto classLoader = jni::Env::getClassLoader();
	if (classLoader) {
		//_networkConnectivity = Rc<NetworkConnectivity>::create(thiz,
		//		[this](NetworkFlags flags) { handleNetworkStateChanged(flags); });

		if (_networkConnectivity) {
			handleNetworkStateChanged(_networkConnectivity->flags);
		}
	}

	Value info;

	auto app = jni::Env::getApp();

	auto v = info.emplace("drawables");
	for (auto &it : app->drawables) { v.setInteger(it.second, it.first); }

	info.setString(_contextInfo->bundleName, "bundleName");
	info.setString(_contextInfo->appName, "applicationName");
	info.setString(_contextInfo->appVersion, "applicationVersion");
	info.setString(_contextInfo->userAgent, "userAgent");
	info.setString(_contextInfo->userLanguage, "locale");
	info.setDouble(_windowInfo->density, "density");
	info.setValue(Value({Value(_windowInfo->rect.width), Value(_windowInfo->rect.height)}), "size");
	info.setInteger(app->sdkVersion, "sdk");

	saveApplicationInfo(info);

	// We use epoll-based looper instead of ALooper-based to use wait/run on it. locking all other android processing
	// It's critical to correctly process onNativeWindowRedrawNeeded event
	//
	// Epoll fd can be added to general system ALooper with ALooper_addFd + _looper->getQueue()->getHandle()
	_looper = event::Looper::acquire(event::LooperInfo{
		.workersCount = _contextInfo->mainThreadsCount,
		.engineMask = event::QueueEngine::EPoll,
	});

	return true;
}

int AndroidContextController::run(NotNull<ContextContainer> c) {
	memory::pool::context ctx(thread::ThreadInfo::getThreadInfo()->threadPool);

	_displayConfigManager =
			Rc<AndroidDisplayConfigManager>::create(this, [this](NotNull<DisplayConfigManager> m) {
		handleSystemNotification(SystemNotification::DisplayChanged);
	});

	auto instance = loadInstance();
	if (!instance) {
		log::source().error("AndroidContextController", "Fail to load gAPI instance");
		_resultCode = -1;
		return -1;
	} else {
		if (auto loop = makeLoop(instance)) {
			_context->handleGraphicsLoaded(loop);
			loop = nullptr;
		} else {
			log::source().error("AndroidContextController", "Fail to create gAPI loop");
			_resultCode = -1;
			return -1;
		}
	}

	auto alooper = ALooper_forThread();

	if (_looper) {
		ALooper_addFd(alooper, _looper->getQueue()->getHandle(), 0, ALOOPER_EVENT_INPUT,
				[](int fd, int events, void *data) {
			auto controller = reinterpret_cast<AndroidContextController *>(data);
			if ((events & ALOOPER_EVENT_INPUT) != 0) {
				controller->_looper->poll();
			}
			return 1;
		}, this);
	}

	_container = c;

	auto app = jni::Env::getApp();
	app->setActivityLoader(
			[this](ANativeActivity *a, BytesView data) { return loadActivity(a, data); });

	app->setLowMemoryHandler([this] {
		log::source().info("AndroidContextController", "onLowMemory");
		handleSystemNotification(SystemNotification::LowMemory);
	});

	app->setConfigurationHandler([this](stappler::platform::ApplicationInfo *info) {
		log::source().info("AndroidContextController", "onConfigurationChanged");
		handleSystemNotification(SystemNotification::ConfigurationChanged);
	});

	resume();
	return 0;
}

bool AndroidContextController::isCursorSupported(WindowCursor, bool serverSide) const {
	return false;
}

WindowCapabilities AndroidContextController::getCapabilities() const {
	return WindowCapabilities::None;
}

bool AndroidContextController::loadActivity(ANativeActivity *a, BytesView data) {
	auto activity = Rc<AndroidActivity>::create(this, a, data);
	if (activity) {
		if (activity->run()) {
			_activities.emplace(activity);
			return true;
		}
	}
	return false;
}

void AndroidContextController::destroyActivity(NotNull<AndroidActivity> a) {
	_activities.erase(a.get());
}

jni::Ref AndroidContextController::getSelf() const {
	auto app = jni::Env::getApp();
	auto env = jni::Env::getEnv();

	return app->jApplication.ref(env);
}

Rc<WindowInfo> AndroidContextController::makeWindowInfo(ANativeWindow *w) const {
	auto appInfo = stappler::platform::ApplicationInfo::getCurrent();
	auto window = Rc<WindowInfo>::alloc();

	auto info = _context->getInfo();

	window->id = info->bundleName;
	window->title = info->appName;
	window->rect = IRect{0, 0, static_cast<uint32_t>(ANativeWindow_getWidth(w)),
		uint32_t(ANativeWindow_getHeight(w))};
	window->density = appInfo->density;

	if (auto loop = _context->getGlLoop()) {
		window->imageFormat = loop->getInfo()->defaultFormat;
	}

	window->flags = WindowCreationFlags::Regular;
	return window;
}

Rc<core::Instance> AndroidContextController::loadInstance() {
	Rc<core::Instance> instance;
#if MODULE_XENOLITH_BACKEND_VK
	auto instanceInfo = move(_instanceInfo);
	_instanceInfo = nullptr;

	auto instanceBackendInfo = Rc<vk::InstanceBackendInfo>::create();
	instanceBackendInfo->setup = [this](vk::InstanceData &data, const vk::InstanceInfo &info) {
		auto ctxInfo = _context->getInfo();

		if (info.availableBackends.test(toInt(vk::SurfaceBackend::Android))) {
			data.enableBackends.set(toInt(vk::SurfaceBackend::Android));
		}

		data.applicationName = ctxInfo->appName;
		data.applicationVersion = ctxInfo->appVersion;
		data.checkPresentationSupport = [](const vk::Instance *inst, VkPhysicalDevice device,
												uint32_t queueIdx) {
			// On Android, all physical devices and queue families must be capable of presentation with
			// any native window. As a result there is no Android-specific query for these capabilities.
			vk::SurfaceBackendMask ret;
			ret.set(toInt(vk::SurfaceBackend::Android));
			return ret;
		};
		return true;
	};

	instanceInfo->backend = move(instanceBackendInfo);

	instance = core::Instance::create(move(instanceInfo));
#else
	log::source().error("LinuxContextController", "No available gAPI backends found");
	_resultCode = -1;
#endif
	return instance;
}

} // namespace stappler::xenolith::platform
