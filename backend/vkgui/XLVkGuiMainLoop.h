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

#ifndef XENOLITH_BACKEND_VKGUI_XLVKGUIMAINLOOP_H_
#define XENOLITH_BACKEND_VKGUI_XLVKGUIMAINLOOP_H_

#include "XLVkGuiConfig.h"
#include "XLMainLoop.h"
#include "XLVkPlatform.h"
#include "XLView.h"

namespace stappler::xenolith::vk {

class GuiMainLoop : public MainLoop {
public:
	using VulkanInstanceData = platform::VulkanInstanceData;
	using VulkanInstanceInfo = platform::VulkanInstanceInfo;

	virtual ~GuiMainLoop();

	virtual bool init(StringView name);
	virtual bool init(StringView name, const Callback<bool(VulkanInstanceData &, const VulkanInstanceInfo &)> &);

	virtual void run(const CallbackInfo &, uint32_t threadsCount = config::getMainThreadCount(),
			TimeInterval = TimeInterval(config::GuiMainLoopDefaultInterval));
	virtual void run(const CallbackInfo &, core::LoopInfo &&, uint32_t threadsCount = config::getMainThreadCount(),
			TimeInterval = TimeInterval(config::GuiMainLoopDefaultInterval));

	bool addView(ViewInfo &&);
	void removeView(xenolith::View *);

protected:
	virtual void handleDeviceStarted(const core::Loop &loop, const core::Device &dev) override;
	virtual void handleDeviceFinalized(const core::Loop &loop, const core::Device &dev) override;

	const core::Device *_device = nullptr;
	Vector<ViewInfo> _tmpViews;
};

}

#endif /* XENOLITH_BACKEND_VKGUI_XLVKGUIMAINLOOP_H_ */
