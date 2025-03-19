/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

void ViewInterface::update(bool displayLink) {
	_presentationEngine->update(displayLink);
}

void ViewInterface::updateConfig() {
	_glLoop->performOnThread([this] {
		_presentationEngine->deprecateSwapchain(false);
	}, this, true);
}

void ViewInterface::setReadyForNextFrame() {
	_glLoop->performOnThread([this] {
		if (_presentationEngine) {
			_presentationEngine->setReadyForNextFrame();
		}
	}, this, true);
}

void ViewInterface::setRenderOnDemand(bool value) {
	_glLoop->performOnThread([this, value] {
		if (_presentationEngine) {
			_presentationEngine->setRenderOnDemand(value);
		}
	}, this, true);
}

bool ViewInterface::isRenderOnDemand() const {
	return _presentationEngine ? _presentationEngine->isRenderOnDemand() : false;
}

void ViewInterface::setFrameInterval(uint64_t value) {
	_glLoop->performOnThread([this, value] {
		if (_presentationEngine) {
			_presentationEngine->setTargetFrameInterval(value);
		}
	}, this, true);
}

uint64_t ViewInterface::getFrameInterval() const {
	return _presentationEngine ? _presentationEngine->getTargetFrameInterval() : 0;
}

void ViewInterface::setContentPadding(const Padding &padding) {
	_glLoop->performOnThread([this, padding] {
		_presentationEngine->setContentPadding(padding);
	}, this, true);
}

}
