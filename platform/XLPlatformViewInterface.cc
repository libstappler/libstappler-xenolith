/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#include "XLPlatformViewInterface.h"
#include "XLPlatformApplication.h"
#include "XLCoreLoop.h"
#include "SPEventLooper.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

bool ViewInterface::init(PlatformApplication &app, core::Loop &loop) {
	_application = &app;
	_loop = &loop;
	return true;
}

void ViewInterface::update(bool displayLink) { _presentationEngine->update(displayLink); }

bool ViewInterface::isOnThisThread() const { return _loop->isOnThisThread(); }

void ViewInterface::performOnThread(Function<void()> &&func, Ref *target, bool immediate,
		StringView tag) {
	if (immediate && isOnThisThread()) {
		func();
	} else {
		_loop->getLooper()->performOnThread(sp::move(func), target, immediate, tag);
	}
}

void ViewInterface::updateConfig() {
	_loop->performOnThread([this] { _presentationEngine->deprecateSwapchain(false); }, this, true);
}

void ViewInterface::setReadyForNextFrame() {
	_loop->performOnThread([this] {
		if (_presentationEngine) {
			_presentationEngine->setReadyForNextFrame();
		}
	}, this, true);
}

void ViewInterface::setRenderOnDemand(bool value) {
	_loop->performOnThread([this, value] {
		if (_presentationEngine) {
			_presentationEngine->setRenderOnDemand(value);
		}
	}, this, true);
}

bool ViewInterface::isRenderOnDemand() const {
	return _presentationEngine ? _presentationEngine->isRenderOnDemand() : false;
}

void ViewInterface::setFrameInterval(uint64_t value) {
	_loop->performOnThread([this, value] {
		if (_presentationEngine) {
			_presentationEngine->setTargetFrameInterval(value);
		}
	}, this, true);
}

uint64_t ViewInterface::getFrameInterval() const {
	return _presentationEngine ? _presentationEngine->getTargetFrameInterval() : 0;
}

void ViewInterface::setContentPadding(const Padding &padding) {
	_loop->performOnThread([this, padding] { _presentationEngine->setContentPadding(padding); },
			this, true);
}

void ViewInterface::waitUntilFrame() {
	if (_presentationEngine) {
		_presentationEngine->waitUntilFramePresentation();
	}
}

} // namespace stappler::xenolith::platform
