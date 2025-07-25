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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DBOOTSTRAPSCENE_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DBOOTSTRAPSCENE_H_

#include "XLCore.h"

#if MODULE_XENOLITH_BACKEND_VKGUI

#include "XLVkGuiApplication.h"

#if MODULE_XENOLITH_RESOURCES_STORAGE
#include "XLStorageServer.h"
#endif

#if MODULE_XENOLITH_RESOURCES_NETWORK
#include "XLNetworkController.h"
#endif

#if MODULE_XENOLITH_RESOURCES_ASSETS
#include "XLAssetLibrary.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {
/*
class SP_PUBLIC BootstrapApplication : public vk::GuiApplication {
public:
	static EventHeader onSwapchainConfig;

	virtual ~BootstrapApplication() = default;

	virtual bool init(ApplicationInfo &&);

	virtual void run() override;

	const core::SurfaceInfo &getSurfaceInfo() const { return _surfaceInfo; }
	const core::SwapchainConfig &getSwapchainConfig() const { return _swapchainConfig; }

	void setPreferredPresentMode(core::PresentMode);

protected:
	virtual Rc<Scene> createSceneForView(vk::View &view, const core::FrameConstraints &constraints);
	virtual void finalizeView(vk::View &view);

	virtual core::SwapchainConfig selectConfig(const vk::View &, const core::SurfaceInfo &info);

	virtual void loadExtensions() override;
	virtual void finalizeExtensions() override;

	Value _storageParams;

	ApplicationInfo _initInfo;

	Mutex _configMutex;
	core::PresentMode _preferredPresentMode = core::PresentMode::Unsupported;

	core::SurfaceInfo _surfaceInfo;
	core::SwapchainConfig _swapchainConfig;

	Rc<xenolith::ApplicationExtension> _networkController;
	Rc<xenolith::ApplicationExtension> _storageServer;
	Rc<xenolith::ApplicationExtension> _assetLibrary;
};
*/
}

#endif

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DBOOTSTRAPSCENE_H_ */
