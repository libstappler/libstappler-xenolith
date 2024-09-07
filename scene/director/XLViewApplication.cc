/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "XLViewApplication.h"
#include "SPSharedModule.h"

#if MODULE_XENOLITH_BACKEND_VKGUI
#include "XLVkGuiPlatform.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

void ViewApplication::wakeup() {
	if (isOnMainThread()) {
		_immediateUpdate = true;
		for (auto &it : _activeViews) {
			it->setReadyForNextFrame();
		}
	} else {
		performOnMainThread([this] {
			_immediateUpdate = true;
			for (auto &it : _activeViews) {
				it->setReadyForNextFrame();
			}
		}, this);
	}
}

bool ViewApplication::addView(ViewInfo &&info) {
	_hasViews = true;
	performOnGlThread([this, info = move(info)] () mutable {
		if (_device) {
			if (info.onClosed) {
				auto tmp = move(info.onClosed);
				info.onClosed = [this, tmp = move(tmp)] (xenolith::View &view) {
					performOnMainThread([this, view = Rc<xenolith::View>(&view)] {
						_activeViews.erase(view);
					}, this);
					tmp(view);
				};
			} else {
				info.onClosed = [this] (xenolith::View &view) {
					performOnMainThread([this, view = Rc<xenolith::View>(&view)] {
						_activeViews.erase(view);
					}, this);
				};
			}

#if MODULE_XENOLITH_BACKEND_VKGUI
			auto createView = SharedModule::acquireTypedSymbol<decltype(&vk::platform::createView)>("xenolith_backend_vkgui",
					"platform::createView(Application&,core::Device const&,ViewInfo&&)");
			if (createView) {
				auto v = createView(*this, *_device, move(info));
				performOnMainThread([this, v] {
					_activeViews.emplace(v.get());
				}, this);
			}
#endif

		} else {
			_tmpViews.emplace_back(move(info));
		}
	}, this);

	return true;
}

void ViewApplication::handleDeviceStarted(const core::Loop &loop, const core::Device &dev) {
	Application::handleDeviceStarted(loop, dev);

	for (auto &it : _tmpViews) {
		addView(move(it));
	}

	_tmpViews.clear();
}

}
