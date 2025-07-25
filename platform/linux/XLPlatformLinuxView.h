/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXVIEW_H_
#define XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXVIEW_H_

#include "XLPlatformViewInterface.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct LinuxPollState {
	uint64_t frameOrder = 0;
	bool shouldClose = false;
	bool deprecateSwapchain = false;
	bool deprecateToFastMode = false;
};

class SP_PUBLIC LinuxViewInterface : public Ref {
public:
	virtual ~LinuxViewInterface() { }

	virtual int getSocketFd() const = 0;

	virtual bool poll(LinuxPollState &) = 0;
	virtual uint64_t getScreenFrameInterval() const = 0;

	virtual void mapWindow() = 0;

	virtual void scheduleFrame() { }
	virtual void onSurfaceInfo(core::SurfaceInfo &) const { }

	virtual void commit(uint32_t width, uint32_t height) { }

	virtual void handleFramePresented(uint64_t) { }

	virtual void readFromClipboard(Function<void(BytesView, StringView)> &&, Ref *) = 0;
	virtual void writeToClipboard(BytesView, StringView contentType) = 0;

	virtual core::FrameConstraints exportConstraints(core::FrameConstraints &&) const = 0;

	virtual void handleLayerUpdate(const ViewLayer &) = 0;
};

} // namespace stappler::xenolith::platform

#endif /* XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXVIEW_H_ */
