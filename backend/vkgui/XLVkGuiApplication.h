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

#ifndef XENOLITH_BACKEND_VKGUI_XLVKGUIAPPLICATION_H_
#define XENOLITH_BACKEND_VKGUI_XLVKGUIAPPLICATION_H_

#include "XLApplication.h"
#include "XLVkGuiConfig.h"
#include "XLVkPlatform.h"
#include "XLVkView.h"

#include "XLViewCommandLine.h"

#if MODULE_XENOLITH_RESOURCES_STORAGE
#include "XLStorageServer.h"
#endif

#if MODULE_XENOLITH_RESOURCES_NETWORK
#include "XLNetworkController.h"
#endif

#if MODULE_XENOLITH_RESOURCES_ASSETS
#include "XLAssetLibrary.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class GuiApplication : public Application {
public:
	using VulkanInstanceData = platform::VulkanInstanceData;
	using VulkanInstanceInfo = platform::VulkanInstanceInfo;

	virtual ~GuiApplication();

	virtual bool init(CommonInfo &&, Rc<core::Instance> && = nullptr);
	virtual bool init(CommonInfo &&, const Callback<bool(VulkanInstanceData &, const VulkanInstanceInfo &)> &);

	virtual void run(const CallbackInfo &, uint32_t threadsCount = config::getMainThreadCount(),
			TimeInterval = TimeInterval(config::GuiMainLoopDefaultInterval));
	virtual void run(const CallbackInfo &, core::LoopInfo &&, uint32_t threadsCount = config::getMainThreadCount(),
			TimeInterval = TimeInterval(config::GuiMainLoopDefaultInterval));
};

class BootstrapApplication : public GuiApplication {
public:
	static EventHeader onSwapchainConfig;

	virtual ~BootstrapApplication() = default;

	virtual bool init(ViewCommandLineData &&, void *native = nullptr);

	virtual void run(Function<void()> &&initCb = nullptr);

	const core::SurfaceInfo &getSurfaceInfo() const { return _surfaceInfo; }
	const core::SwapchainConfig &getSwapchainConfig() const { return _swapchainConfig; }

	void setPreferredPresentMode(core::PresentMode);

protected:
	virtual Rc<Scene> createSceneForView(vk::View &view, const core::FrameContraints &constraints);
	virtual void finalizeView(vk::View &view);

	virtual core::SwapchainConfig selectConfig(vk::View &, const core::SurfaceInfo &info);

	Value _storageParams;

	ViewCommandLineData _data;

	Mutex _configMutex;
	core::PresentMode _preferredPresentMode = core::PresentMode::Unsupported;

	core::SurfaceInfo _surfaceInfo;
	core::SwapchainConfig _swapchainConfig;

#if MODULE_XENOLITH_RESOURCES_NETWORK
public:
	xenolith::network::Controller *getNetworkController() const { return _networkController; }
protected:
	Rc<xenolith::network::Controller> _networkController;
#endif

#if MODULE_XENOLITH_RESOURCES_STORAGE
public:
	xenolith::storage::Server *getStorageServer() const { return _storageServer; }
protected:
	Rc<xenolith::storage::Server> _storageServer;
#endif

#if MODULE_XENOLITH_RESOURCES_ASSETS
public:
	xenolith::storage::AssetLibrary *getAssetLibrary() const { return _assetLibrary; }
protected:
	Rc<xenolith::storage::AssetLibrary> _assetLibrary;
#endif
};

}

#endif /* XENOLITH_BACKEND_VKGUI_XLVKGUIAPPLICATION_H_ */
