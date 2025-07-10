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

#include "SPFilepath.h"
#include "XLCommon.h"

/*
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

	auto light = Rc<basic2d::SceneLight>::create(basic2d::SceneLightType::Ambient, Vec2(0.0f, 0.3f),
			1.5f, color);
	auto ambient = Rc<basic2d::SceneLight>::create(basic2d::SceneLightType::Ambient,
			Vec2(0.0f, 0.0f), 1.5f, color);

	content->setGlobalLight(Color4F::WHITE);
	content->removeAllLights();
	content->addLight(move(light));
	content->addLight(move(ambient));

	return true;
}

void BootstrapScene::handleContentSizeDirty() {
	Scene2d::handleContentSizeDirty();

	_helloWorldLabel->setPosition(_content->getContentSize() / 2.0f);
}

XL_DECLARE_EVENT_CLASS(BootstrapApplication, onSwapchainConfig);

bool BootstrapApplication::init(ApplicationInfo &&info) {
	_initInfo = move(info);

	String dbPath;
	filesystem::enumerateWritablePaths(FileInfo{"root.sqlite", FileCategory::AppCache},
			[&](StringView path, FileFlags) {
		dbPath = path.str<Interface>();
		return false;
	});

	// clang-format off
	_storageParams = Value({
		pair("driver", Value("sqlite")),
		pair("dbname", Value(dbPath)),
		pair("serverName", Value("RootStorage"))
	});
	// clang-format on

	return GuiApplication::init(ApplicationInfo(_initInfo));
}

void BootstrapApplication::run() {
	// clang-format off
	addView(ViewInfo{
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
			stop();
		}
	});
	// clang-format on
	GuiApplication::run();
}

void BootstrapApplication::setPreferredPresentMode(core::PresentMode mode) {
	std::unique_lock<Mutex> lock(_configMutex);
	_preferredPresentMode = mode;
}

Rc<Scene> BootstrapApplication::createSceneForView(vk::View &view,
		const core::FrameConstraints &constraints) {
#if MODULE_XENOLITH_RENDERER_BASIC2D
	return Rc<BootstrapScene>::create(this, constraints);
#else
	return nullptr;
#endif
}

void BootstrapApplication::finalizeView(vk::View &view) { }

core::SwapchainConfig BootstrapApplication::selectConfig(const vk::View &,
		const core::SurfaceInfo &info) {
}

void BootstrapApplication::loadExtensions() {
	GuiApplication::loadExtensions();

	if (_storageParams.getString("driver") == "sqlite") {
		auto path = _storageParams.getString("dbname");
		filesystem::mkdir(filepath::root(filepath::root(path)));
		filesystem::mkdir(filepath::root(path));
	}

#if MODULE_XENOLITH_RESOURCES_STORAGE
	auto createServer = SharedModule::acquireTypedSymbol<decltype(&storage::Server::createServer)>(
			buildconfig::MODULE_XENOLITH_RESOURCES_STORAGE_NAME, "Server::createServer");

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
	auto createController =
			SharedModule::acquireTypedSymbol<decltype(&network::Controller::createController)>(
					buildconfig::MODULE_XENOLITH_RESOURCES_NETWORK_NAME,
					"Controller::createController");
	if (createController) {
		_networkController = createController(static_cast<Application *>(this), "Root", Bytes());
		if (_networkController) {
			addExtension(Rc<xenolith::ApplicationExtension>(_networkController));
		}
	}
#endif

#if MODULE_XENOLITH_RESOURCES_ASSETS
	if (_networkController) {
		auto createLibrary =
				SharedModule::acquireTypedSymbol< decltype(&storage::AssetLibrary::createLibrary)>(
						buildconfig::MODULE_XENOLITH_RESOURCES_ASSETS_NAME,
						"AssetLibrary::createLibrary");
		if (createLibrary) {
			String assetsPath;

			_assetLibrary = createLibrary(static_cast<Application *>(this),
					static_cast<network::Controller *>(_networkController.get()), "AssetLibrary",
					FileInfo{"assets", FileCategory::AppCache}, Value());
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

} // namespace stappler::xenolith::basic2d
*/
