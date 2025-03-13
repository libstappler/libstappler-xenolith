/**
 Copyright (c) 2024-2025 Stappler LLC <admin@stappler.dev>

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

#include "XLCommon.h"
#include "XLPlatform.h"

#include "XL2dBootstrapApplication.h"
#include "XL2dScene.h"
#include "XL2dLabel.h"
#include "XL2dSceneContent.h"
#include "SPSharedModule.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class BootstrapScene : public basic2d::Scene2d {
public:
	virtual ~BootstrapScene() = default;

	virtual bool init(Application *, const core::FrameConstraints &constraints) override;

	virtual void handleContentSizeDirty() override;

protected:
	using Scene::init;

	basic2d::Label *_helloWorldLabel = nullptr;
};

bool BootstrapScene::init(Application *app, const core::FrameConstraints &constraints) {
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

void BootstrapScene::handleContentSizeDirty() {
	Scene2d::handleContentSizeDirty();

	_helloWorldLabel->setPosition(_content->getContentSize() / 2.0f);
}

XL_DECLARE_EVENT_CLASS(BootstrapApplication, onSwapchainConfig);

bool BootstrapApplication::init(ApplicationInfo &&info) {
	_initInfo = move(info);

	_storageParams = Value({
		pair("driver", Value("sqlite")),
		pair("dbname", Value(filesystem::cachesPath<Interface>("root.sqlite"))),
		pair("serverName", Value("RootStorage"))
	});

	return GuiApplication::init(ApplicationInfo(_initInfo));
}

void BootstrapApplication::run() {
	_info.initCallback = [&] (const PlatformApplication &) {
		GuiApplication::addView(ViewInfo{
			.window = platform::WindowInfo{
				.title = _info.applicationName,
				.bundleId = _info.bundleName,
				.rect = URect(UVec2{0, 0}, _info.screenSize),
				.decoration = _info.viewDecoration,
				.density = _info.density,
			},
			.selectConfig = [this] (const xenolith::View &view, const core::SurfaceInfo &info) -> core::SwapchainConfig {
				return selectConfig(static_cast<const vk::View &>(view), info);
			},
			.onCreated = [this] (xenolith::View &view, const core::FrameConstraints &constraints) {
				auto scene = createSceneForView(static_cast<vk::View &>(view), constraints);
				view.getDirector()->runScene(move(scene));
			},
			.onClosed = [this] (xenolith::View &view) {
				finalizeView(static_cast<vk::View &>(view));
				end();
			}
		});
	};
	_info.updateCallback = [&] (const PlatformApplication &, const UpdateTime &time) { };

	GuiApplication::run();
}

void BootstrapApplication::setPreferredPresentMode(core::PresentMode mode) {
	std::unique_lock<Mutex> lock(_configMutex);
	_preferredPresentMode = mode;
}

Rc<Scene> BootstrapApplication::createSceneForView(vk::View &view, const core::FrameConstraints &constraints) {
#if MODULE_XENOLITH_RENDERER_BASIC2D
	return Rc<BootstrapScene>::create(this, constraints);
#else
	return nullptr;
#endif
}

void BootstrapApplication::finalizeView(vk::View &view) {

}

core::SwapchainConfig BootstrapApplication::selectConfig(const vk::View &, const core::SurfaceInfo &info) {
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

	performOnAppThread([this, info, ret] {
		_surfaceInfo = info;
		_swapchainConfig = ret;

		onSwapchainConfig(this);
	});

	return ret;
}

void BootstrapApplication::loadExtensions() {
	GuiApplication::loadExtensions();

	if (_storageParams.getString("driver") == "sqlite") {
		auto path = _storageParams.getString("dbname");
		filesystem::mkdir(filepath::root(filepath::root(path)));
		filesystem::mkdir(filepath::root(path));
	}

#if MODULE_XENOLITH_RESOURCES_STORAGE
	auto createServer = SharedModule::acquireTypedSymbol<decltype(&storage::Server::createServer)>("xenolith_resources_storage",
			"Server::createServer(Application*,Value const&)");

	if (createServer) {
		_storageServer = createServer(static_cast<Application *>(this), _storageParams);

		if (_storageServer) {
			addExtension(Rc<xenolith::ApplicationExtension>(_storageServer));
		} else {
			log::error("Application", "Fail to create storage server");
		}
	}
#endif

#if MODULE_XENOLITH_RESOURCES_NETWORK
	auto createController = SharedModule::acquireTypedSymbol<decltype(&network::Controller::createController)>("xenolith_resources_network",
			"Controller::createController(Application*,StringView,Bytes&&)");
	if (createController) {
		_networkController = createController(static_cast<Application *>(this), "Root", Bytes());
		if (_networkController) {
			addExtension(Rc<xenolith::ApplicationExtension>(_networkController));
		}
	}
#endif

#if MODULE_XENOLITH_RESOURCES_ASSETS
	if (_networkController) {
		auto createLibrary = SharedModule::acquireTypedSymbol<decltype(&storage::AssetLibrary::createLibrary)>("xenolith_resources_assets",
				"AssetLibrary::createLibrary(Application*,network::Controller*,Value const&)");
		if (createLibrary) {
			_assetLibrary = createLibrary(static_cast<Application *>(this), static_cast<network::Controller *>(_networkController.get()), Value({
				pair("driver", Value("sqlite")),
				pair("dbname", Value(filesystem::cachesPath<Interface>("assets.sqlite"))),
				pair("serverName", Value("AssetStorage"))
			}));
			if (_assetLibrary) {
				addExtension(Rc<xenolith::ApplicationExtension>(_assetLibrary));
			}
		}
	}
#endif
}

void BootstrapApplication::finalizeExtensions() {
	GuiApplication::finalizeExtensions();

#if MODULE_XENOLITH_RESOURCES_STORAGE
	_storageServer = nullptr;
#endif

#if MODULE_XENOLITH_RESOURCES_NETWORK
	_networkController = nullptr;
#endif

#if MODULE_XENOLITH_RESOURCES_ASSETS
	_assetLibrary = nullptr;
#endif
}

}
