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

#include "XLVkLoop.h"
#include "XLVkGuiPlatform.h"
#include "platform/linux/XLVkGuiViewImpl.h"
#include "XLVkGuiApplication.h"
#include "XLPlatform.h"

#include "XLScene.h"

#if MODULE_XENOLITH_RENDERER_BASIC2D

#include "XL2dScene.h"
#include "XL2dLabel.h"
#include "XL2dSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class BootstrapScene : public basic2d::Scene2d {
public:
	virtual ~BootstrapScene() = default;

	virtual bool init(Application *, const core::FrameContraints &constraints);

	virtual void onContentSizeDirty() override;

protected:
	using Scene::init;

	basic2d::Label *_helloWorldLabel = nullptr;
};

bool BootstrapScene::init(Application *app, const core::FrameContraints &constraints) {
	if (!Scene2d::init(app, constraints)) {
		return false;
	}

	auto content = Rc<basic2d::SceneContent2d>::create();

	_helloWorldLabel = content->addChild(Rc<basic2d::Label>::create());
	_helloWorldLabel->setString("Hello World");
	_helloWorldLabel->setAnchorPoint(Anchor::Middle);

	setContent(content);

	auto color = Color4F::WHITE;
	color.a = 0.5f;

	auto light = Rc<basic2d::SceneLight>::create(basic2d::SceneLightType::Ambient, Vec2(0.0f, 0.3f), 1.5f, color);
	auto ambient = Rc<basic2d::SceneLight>::create(basic2d::SceneLightType::Ambient, Vec2(0.0f, 0.0f), 1.5f, color);

	content->setGlobalLight(Color4F::WHITE);
	content->removeAllLights();
	content->addLight(move(light));
	content->addLight(move(ambient));

	filesystem::mkdir(filesystem::cachesPath<Interface>());

	return true;
}

void BootstrapScene::onContentSizeDirty() {
	Scene2d::onContentSizeDirty();

	_helloWorldLabel->setPosition(_content->getContentSize() / 2.0f);
}

}

#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

GuiApplication::~GuiApplication() { }

bool GuiApplication::init(CommonInfo &&appInfo, Rc<core::Instance> &&instance) {
	if (instance) {
		if (Application::init(move(appInfo), move(instance))) {
			return true;
		}
	} else {
		auto inst = vk::platform::createInstance([&] (vk::platform::VulkanInstanceData &data, const vk::platform::VulkanInstanceInfo &info) {
			data.applicationName = appInfo.applicationName;
			data.applicationVersion = appInfo.applicationVersion;
			data.checkPresentationSupport = vk::platform::checkPresentationSupport;
			return vk::platform::initInstance(data, info);
		});
		if (Application::init(move(appInfo), move(inst))) {
			return true;
		}
	}
	return false;
}

bool GuiApplication::init(CommonInfo &&appInfo, const Callback<bool(VulkanInstanceData &, const VulkanInstanceInfo &)> &cb) {
	auto instance = vk::platform::createInstance([&] (VulkanInstanceData &data, const VulkanInstanceInfo &info) {
		if (cb(data, info)) {
			data.applicationName = appInfo.applicationName;
			data.applicationVersion = appInfo.applicationVersion;
			return true;
		}
		return false;
	});

	if (Application::init(move(appInfo), move(instance))) {
		return true;
	}

	return false;
}

void GuiApplication::run(const CallbackInfo &cb, uint32_t threadCount, TimeInterval ival) {
	core::LoopInfo info;

	run(cb, move(info), threadCount, ival);
}

void GuiApplication::run(const CallbackInfo &cb, core::LoopInfo &&info, uint32_t threadCount, TimeInterval ival) {
	if (!info.platformData) {
		auto data = Rc<LoopData>::alloc();
		data->deviceSupportCallback = [] (const vk::DeviceInfo &dev) {
			return dev.supportsPresentation() &&
					std::find(dev.availableExtensions.begin(), dev.availableExtensions.end(),
							String(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) != dev.availableExtensions.end();
		};
		data->deviceExtensionsCallback = [] (const vk::DeviceInfo &dev) -> Vector<StringView> {
			Vector<StringView> ret;
			ret.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
			return ret;
		};
		info.platformData = data;
	}

	Application::run(cb, move(info), threadCount, ival);
}

XL_DECLARE_EVENT_CLASS(BootstrapApplication, onSwapchainConfig);

bool BootstrapApplication::init(ViewCommandLineData &&data, void *native) {
	_data = move(data);

	Application::CommonInfo info({
		.bundleName = _data.bundleName,
		.applicationName = _data.applicationName,
		.applicationVersion = _data.applicationVersion,
		.userAgent = _data.userAgent,
		.locale = _data.userLanguage,
		.nativeHandle = native
	});

	_storageParams = Value({
		pair("driver", Value("sqlite")),
		pair("dbname", Value(filesystem::cachesPath<Interface>("root.sqlite"))),
		pair("serverName", Value("RootStorage"))
	});

	return GuiApplication::init(move(info));
}

void BootstrapApplication::run(Function<void()> &&initCb) {
	if (_storageParams.getString("driver") == "sqlite") {
		auto path = _storageParams.getString("dbname");
		filesystem::mkdir(filepath::root(filepath::root(path)));
		filesystem::mkdir(filepath::root(path));
	}

#if MODULE_XENOLITH_RESOURCES_STORAGE
	_storageServer = Rc<storage::Server>::create(static_cast<Application *>(this), _storageParams);

	if (!_storageServer) {
		log::error("Application", "Fail to create storage server");
	}

	addExtension(Rc<storage::Server>(_storageServer));
#endif

#if MODULE_XENOLITH_RESOURCES_NETWORK
	_networkController = Rc<network::Controller>::alloc(static_cast<Application *>(this), "Root");
	addExtension(Rc<network::Controller>(_networkController));
#endif

#if MODULE_XENOLITH_RESOURCES_ASSETS
	_assetLibrary = Rc<storage::AssetLibrary>::create(static_cast<Application *>(this), _networkController, Value({
		pair("driver", Value("sqlite")),
		pair("dbname", Value(filesystem::cachesPath<Interface>("assets.sqlite"))),
		pair("serverName", Value("AssetStorage"))
	}));
	addExtension(Rc<storage::AssetLibrary>(_assetLibrary));
#endif

	GuiApplication::CallbackInfo callbacks({
		.initCallback = [&] (const Application &) {
			GuiApplication::addView(ViewInfo{
				.name = _data.applicationName,
				.bundleId = _data.bundleName,
				.rect = URect(UVec2{0, 0}, _data.screenSize),
				.density = _data.density,
				.selectConfig = [this] (xenolith::View &view, const core::SurfaceInfo &info) -> core::SwapchainConfig {
					return selectConfig(static_cast<View &>(view), info);
				},
				.onCreated = [this] (xenolith::View &view, const core::FrameContraints &constraints) {
					auto scene = createSceneForView(static_cast<View &>(view), constraints);
					view.getDirector()->runScene(move(scene));
				},
				.onClosed = [this] (xenolith::View &view) {
					finalizeView(static_cast<View &>(view));
					end();
				}
			});
			if (initCb) {
				initCb();
			}
		},
		.updateCallback = [&] (const Application &, const UpdateTime &time) { }
	});

	GuiApplication::run(callbacks);

#if MODULE_XENOLITH_RESOURCES_ASSETS
	_assetLibrary = nullptr;
#endif

#if MODULE_XENOLITH_RESOURCES_NETWORK
	_networkController = nullptr;
#endif

#if MODULE_XENOLITH_RESOURCES_STORAGE
	_storageServer = nullptr;
#endif
}

void BootstrapApplication::setPreferredPresentMode(core::PresentMode mode) {
	std::unique_lock<Mutex> lock(_configMutex);
	_preferredPresentMode = mode;
}

Rc<Scene> BootstrapApplication::createSceneForView(vk::View &view, const core::FrameContraints &constraints) {
#if MODULE_XENOLITH_RENDERER_BASIC2D
	return Rc<BootstrapScene>::create(this, constraints);
#else
	return nullptr;
#endif
}

void BootstrapApplication::finalizeView(vk::View &view) {

}

core::SwapchainConfig BootstrapApplication::selectConfig(vk::View &, const core::SurfaceInfo &info) {
	std::unique_lock<Mutex> lock(_configMutex);
	core::SwapchainConfig ret;
	ret.extent = info.currentExtent;
	ret.imageCount = std::max(uint32_t(3), info.minImageCount);

	ret.presentMode = info.presentModes.front();
	if (_preferredPresentMode != core::PresentMode::Unsupported) {
		for (auto &it : info.presentModes) {
			if (it == _preferredPresentMode) {
				ret.presentMode = it;
				break;
			}
		}
	}

	if (std::find(info.presentModes.begin(), info.presentModes.end(), core::PresentMode::Immediate) != info.presentModes.end()) {
		ret.presentModeFast = core::PresentMode::Immediate;
	}

	auto it = info.formats.begin();
	while (it != info.formats.end()) {
		if (it->first != xenolith::platform::getCommonFormat()) {
			++ it;
		} else {
			break;
		}
	}

	if (it == info.formats.end()) {
		ret.imageFormat = info.formats.front().first;
		ret.colorSpace = info.formats.front().second;
	} else {
		ret.imageFormat = it->first;
		ret.colorSpace = it->second;
	}

	if ((info.supportedCompositeAlpha & core::CompositeAlphaFlags::Opaque) != core::CompositeAlphaFlags::None) {
		ret.alpha = core::CompositeAlphaFlags::Opaque;
	} else if ((info.supportedCompositeAlpha & core::CompositeAlphaFlags::Inherit) != core::CompositeAlphaFlags::None) {
		ret.alpha = core::CompositeAlphaFlags::Inherit;
	}

	ret.transfer = (info.supportedUsageFlags & core::ImageUsage::TransferDst) != core::ImageUsage::None;

	if (ret.presentMode == core::PresentMode::Mailbox) {
		ret.imageCount = std::max(uint32_t(3), ret.imageCount);
	}

	ret.transform = info.currentTransform;

	performOnMainThread([this, info, ret] {
		_surfaceInfo = info;
		_swapchainConfig = ret;

		onSwapchainConfig(this);
	});

	return ret;
}

}
