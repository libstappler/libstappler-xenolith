/**
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

#include "XLContext.h"
#include "XLAppThread.h"
#include "platform/XLContextController.h"
#include "SPSharedModule.h"
#include "XLAppWindow.h"

#if MODULE_XENOLITH_FONT
#include "XLFontLocale.h"
#include "XLFontComponent.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

XL_DECLARE_EVENT_CLASS(Context, onMessageToken)
XL_DECLARE_EVENT_CLASS(Context, onRemoteNotification)

Context::Context() {
	_alloc = memory::allocator::create();
	_pool = memory::pool::create(_alloc);
	_tmpPool = memory::pool::create(_pool);

	int result = 0;
	if (!sp::initialize(result)) {
		_valid = false;
	} else {
		_valid = true;
	}
}

Context::~Context() {
	memory::pool::destroy(_tmpPool);
	memory::pool::destroy(_pool);
	memory::allocator::destroy(_alloc);
	sp::terminate();
}

bool Context::init(ContextConfig &&info) {
	_info = info.context;
	_controller = platform::ContextController::create(this, move(info));

	_looper = _controller->getLooper();

#if MODULE_XENOLITH_FONT
	auto setLocale = SharedModule::acquireTypedSymbol<decltype(&locale::setLocale)>(
			buildconfig::MODULE_XENOLITH_FONT_NAME, "locale::setLocale");
	if (setLocale) {
		setLocale(_info->userLanguage);
	}
#endif

	return true;
}

int Context::run() { return _controller->run(); }

void Context::performOnThread(Function<void()> &&func, Ref *target, bool immediate,
		StringView tag) const {
	_looper->performOnThread(sp::move(func), target, immediate, tag);
}

void Context::readFromClipboard(Function<void(BytesView, StringView)> &&, Ref *) { }

void Context::writeToClipboard(BytesView, StringView contentType) { }

void Context::handleConfigurationChanged(Rc<ContextInfo> &&info) { _info = move(info); }

void Context::handleGraphicsLoaded(NotNull<core::Loop> loop) {
	_loop = loop;
#if MODULE_XENOLITH_FONT
	auto createFontComponent =
			SharedModule::acquireTypedSymbol< decltype(&font::FontComponent::createFontComponent)>(
					buildconfig::MODULE_XENOLITH_FONT_NAME, "FontComponent::createFontComponent");
	if (createFontComponent) {
		if (auto comp = createFontComponent(this)) {
			addComponent(move(comp));
		}
	}
#endif
}

Value Context::saveState() { return Value(); }

void Context::handleAppThreadCreated(NotNull<AppThread>) { }
void Context::handleAppThreadDestroyed(NotNull<AppThread>) { }
void Context::handleAppThreadUpdate(NotNull<AppThread>, const UpdateTime &time) { }

core::SwapchainConfig Context::handleAppWindowSurfaceUpdate(NotNull<AppWindow> w,
		const SurfaceInfo &info) {
	SwapchainConfig ret;
	ret.extent = info.currentExtent;
	ret.imageCount = std::max(uint32_t(3), info.minImageCount);

	ret.presentMode = core::PresentMode::Unsupported;

	auto windowInfo = w->getInfo();

	core::PresentMode preferredPresentMode =
			windowInfo ? windowInfo->preferredPresentMode : core::PresentMode::Mailbox;
	core::ImageFormat imageFormat =
			windowInfo ? windowInfo->imageFormat : core::ImageFormat::Undefined;
	core::ColorSpace colorSpace =
			windowInfo ? windowInfo->colorSpace : core::ColorSpace::SRGB_NONLINEAR_KHR;

	if (preferredPresentMode != core::PresentMode::Unsupported) {
		for (auto &it : info.presentModes) {
			if (it == preferredPresentMode) {
				ret.presentMode = it;
				break;
			}
		}
	}

	if (ret.presentMode == core::PresentMode::Unsupported) {
		log::info("Context", "handleAppWindowSurfaceUpdate: fail to set up with ",
				core::getPresentModeName(preferredPresentMode), " PresentMode, fallback to ",
				core::getPresentModeName(info.presentModes.front()));
		ret.presentMode = info.presentModes.front();
	}

	// use Immediate mode as fastest for quick transitions (like on manual window resize)
	if (std::find(info.presentModes.begin(), info.presentModes.end(), core::PresentMode::Immediate)
			!= info.presentModes.end()) {
		ret.presentModeFast = core::PresentMode::Immediate;
	}

	auto it = info.formats.begin();
	while (it != info.formats.end()) {
		if (it->first != imageFormat && it->second != colorSpace) {
			++it;
		} else {
			break;
		}
	}

	if (it == info.formats.end()) {
		ret.imageFormat = info.formats.front().first;
		ret.colorSpace = info.formats.front().second;
	} else {
		log::info("Context",
			"handleAppWindowSurfaceUpdate: fail to find (imageFormat:colorspace) pair "
			"for a window, fallback to (",
			core::getImageFormatName(it->first), ":",
			core::getColorSpaceName(it->second), ")");
		ret.imageFormat = it->first;
		ret.colorSpace = it->second;
	}

	if ((info.supportedCompositeAlpha & core::CompositeAlphaFlags::Opaque)
			!= core::CompositeAlphaFlags::None) {
		ret.alpha = core::CompositeAlphaFlags::Opaque;
	} else if ((info.supportedCompositeAlpha & core::CompositeAlphaFlags::Inherit)
			!= core::CompositeAlphaFlags::None) {
		ret.alpha = core::CompositeAlphaFlags::Inherit;
	}

	ret.transfer =
			(info.supportedUsageFlags & core::ImageUsage::TransferDst) != core::ImageUsage::None;

	if (ret.presentMode == core::PresentMode::Mailbox) {
		ret.imageCount = std::max(uint32_t(3), ret.imageCount);
	}

	ret.transform = info.currentTransform;

	return ret;
}

void Context::handleAppWindowCreated(NotNull<AppWindow>, const core::FrameConstraints &) { }
void Context::handleAppWindowDestroyed(NotNull<AppWindow>) { }

void Context::handleNativeWindowCreated(NotNull<NativeWindow> w) { }
void Context::handleNativeWindowDestroyed(NotNull<NativeWindow> w) { }
void Context::handleNativeWindowRedrawNeeded(NotNull<NativeWindow>) { }
void Context::handleNativeWindowResized(NotNull<NativeWindow>) { }
void Context::handleNativeWindowInputEvents(NotNull<NativeWindow>,
		Vector<core::InputEventData> &&) { }

void Context::handleNativeWindowTextInput(NotNull<NativeWindow>, const core::TextInputState &) { }

void Context::handleLowMemory() {
	for (auto &it : _components) { it.second->handleLowMemory(this); }
}

void Context::handleWillDestroy() {
	for (auto &it : _components) { it.second->handleDestroy(this); }
	_components.clear();

	_controller = nullptr;
	_loop->stop();
	_loop = nullptr;
}
void Context::handleDidDestroy() { }

void Context::handleWillStop() {
	if (_running) {
		for (auto &it : _components) { it.second->handleStop(this); }

		_running = false;
	}
}

void Context::handleDidStop() {
	_application->stop();
	_application = nullptr;
}

void Context::handleWillPause() {
	for (auto &it : _components) { it.second->handlePause(this); }
}
void Context::handleDidPause() { }

void Context::handleWillResume() { }

void Context::handleDidResume() {
	for (auto &it : _components) { it.second->handleResume(this); }
}

void Context::handleWillStart() { _application = Rc<AppThread>::create(this); }

void Context::handleDidStart() {
	if (!_running) {

		/*

		addTokenCallback(this, [this](StringView str) {
			_application->performOnAppThread([this, str = str.str<Interface>()]() mutable {
				_application->updateMessageToken(BytesView(StringView(str)));
			}, this);
		});

		addRemoteNotificationCallback(this, [this](const Value &val) {
			_application->performOnAppThread([this, val = val]() mutable {
				_application->receiveRemoteNotification(move(val));
			}, this);
		});*/

		for (auto &it : _components) { it.second->handleStart(this); }
		_running = true;
	}
}

void Context::handleWindowFocusChanged(int focused) { }

bool Context::handleBackButton() { return false; }

void Context::addNetworkCallback(Ref *key, Function<void(NetworkFlags)> &&cb) {
	_controller->addNetworkCallback(key, sp::move(cb));
}
void Context::removeNetworkCallback(Ref *key) { _controller->removeNetworkCallback(key); }

void Context::updateMessageToken(BytesView tok) {
	if (tok != _messageToken) {
		onMessageToken(this, _messageToken);
	}
}

void Context::receiveRemoteNotification(Value &&val) { onRemoteNotification(this, move(val)); }

Rc<AppWindow> Context::makeAppWindow(NotNull<NativeWindow> w) {
	return _controller->makeAppWindow(_application, w);
}

Rc<core::PresentationEngine> Context::makePresentationEngine(NotNull<core::PresentationWindow> w) {
	return _loop->makePresentationEngine(w.get());
}

void Context::initializeComponent(NotNull<ContextComponent> comp) {
	_controller->initializeComponent(comp);
}
} // namespace stappler::xenolith
