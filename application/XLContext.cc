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
#include "SPStringDetail.h"
#include "XLAppThread.h"
#include "XLAppWindow.h"
#include "XLContextInfo.h"
#include "XLCoreEnum.h"
#include "XLDirector.h"
#include "XLScene.h"
#include "platform/XLContextController.h"
#include "platform/XLContextNativeWindow.h"

#if MODULE_XENOLITH_FONT
#include "XLFontLocale.h"
#include "XLFontComponent.h"
#endif

#include "SPSharedModule.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

XL_DECLARE_EVENT_CLASS(Context, onMessageToken)
XL_DECLARE_EVENT_CLASS(Context, onRemoteNotification)

int Context::run(int argc, const char **argv) {
	auto runWithConfig = [&](ContextConfig &&config) -> int {
		if (hasFlag(config.flags, CommonFlags::Help)) {

			auto printHelpSymbol = SharedModule::acquireTypedSymbol<SymbolPrintHelpSignature>(
					buildconfig::MODULE_APPCOMMON_NAME, SymbolPrintHelpName);
			if (printHelpSymbol) {
				printHelpSymbol(config, argc, argv);
			} else {
				auto appName = filepath::lastComponent(StringView(argv[0]));

				std::cout << appName << " <options>:\n";
				ContextConfig::getCommandLineParser().describe(
						[&](StringView str) { std::cout << str; });

				auto helpString = SharedModule::acquireTypedSymbol<SymbolHelpStringSignature>(
						buildconfig::MODULE_APPCOMMON_NAME, SymbolHelpStringName);
				if (helpString) {
					std::cout << helpString << "\n";
				}
			}
			return 0;
		}

		if (hasFlag(config.flags, CommonFlags::Verbose)) {
			std::cerr << " Current work dir: " << stappler::filesystem::currentDir<Interface>()
					  << "\n";
			std::cerr << " Options: " << stappler::data::EncodeFormat::Pretty << config.encode()
					  << "\n";
		}

		Rc<Context> ctx;

		auto makeContextSymbol = SharedModule::acquireTypedSymbol<SymbolMakeContextSignature>(
				buildconfig::MODULE_APPCOMMON_NAME, SymbolMakeContextName);
		if (makeContextSymbol) {
			ctx = makeContextSymbol(move(config));
		} else {
			ctx = Rc<Context>::create(move(config));
		}

		if (!ctx) {
			log::error("Context", "Fail to create Context");
			return -1;
		}

		auto container = Rc<platform::ContextContainer>::create();
		container->context = sp::move(ctx);
		container->controller = container->context->_controller;

		return container->controller->run(container);
	};

	// access context
	auto cfgSymbol = SharedModule::acquireTypedSymbol<SymbolParseConfigSignature>(
			buildconfig::MODULE_APPCOMMON_NAME, SymbolParseConfigName);
	if (cfgSymbol) {
		return runWithConfig(cfgSymbol(argc, argv));
	} else {
		return runWithConfig(ContextConfig(argc, argv));
	}
}

Context::Context() {
	_alloc = memory::allocator::create();
	_pool = memory::pool::create(_alloc);
	_tmpPool = memory::pool::create(_pool);

	// Context pool should be main thread's pool
	thread::ThreadInfo::setThreadPool(_pool);

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

	if (!_controller) {
		log::error("Context", "Fail to create ContextController");
		return false;
	}
	_looper = _controller->getLooper();

#if MODULE_XENOLITH_FONT
	auto setLocale = SharedModule::acquireTypedSymbol<decltype(&locale::setLocale)>(
			buildconfig::MODULE_XENOLITH_FONT_NAME, "locale::setLocale");
	if (setLocale) {
		if (_info->userLanguage.empty()) {
			setLocale(stappler::platform::getOsLocale());
		} else {
			setLocale(_info->userLanguage);
		}
	}
#endif

	return true;
}

void Context::performOnThread(Function<void()> &&func, Ref *target, bool immediate,
		StringView tag) const {
	_looper->performOnThread(sp::move(func), target, immediate, tag);
}

bool Context::isCursorSupported(WindowCursor cursor, bool serverSide) const {
	return _controller->isCursorSupported(cursor, serverSide);
}

WindowCapabilities Context::getWindowCapabilities() const { return _controller->getCapabilities(); }

Status Context::readFromClipboard(Function<void(Status, BytesView, StringView)> &&cb,
		Function<StringView(SpanView<StringView>)> &&tcb, Ref *target) {
	auto request = Rc<platform::ClipboardRequest>::create();
	request->dataCallback = sp::move(cb);
	request->typeCallback = sp::move(tcb);
	request->target = target;

	return _controller->readFromClipboard(sp::move(request));
}

Status Context::writeToClipboard(Function<Bytes(StringView)> &&cb, SpanView<String> types,
		Ref *ref) {
	auto data = Rc<platform::ClipboardData>::create();
	data->encodeCallback = sp::move(cb);
	data->types = types.vec<Interface>();
	data->owner = ref;

	return _controller->writeToClipboard(move(data));
}

void Context::handleConfigurationChanged(Rc<ContextInfo> &&info) { _info = move(info); }

void Context::handleGraphicsLoaded(NotNull<core::Loop> loop) {
	_loop = loop;
	_loop->run();
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

void Context::handleAppThreadCreated(NotNull<AppThread>) {
	log::info("Context", "handleAppThreadCreated");
}
void Context::handleAppThreadDestroyed(NotNull<AppThread>) {
	log::info("Context", "handleAppThreadDestroyed");
}
void Context::handleAppThreadUpdate(NotNull<AppThread>, const UpdateTime &time) {
	// log::info("Context", "handleAppThreadUpdate");
}

core::SwapchainConfig Context::handleAppWindowSurfaceUpdate(NotNull<AppWindow> w,
		const SurfaceInfo &info, bool fastMode) {
	SwapchainConfig ret;
	ret.extent = info.currentExtent;
	ret.imageCount = std::max(uint32_t(3), info.minImageCount);

	ret.presentMode = core::PresentMode::Unsupported;

	auto windowInfo = w->getInfo();

	core::PresentMode preferredPresentMode =
			windowInfo ? windowInfo->preferredPresentMode : core::PresentMode::Mailbox;
	core::ImageFormat imageFormat =
			windowInfo ? windowInfo->imageFormat : core::ImageFormat::R8G8B8A8_UNORM;
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
	if (ret.presentMode != core::PresentMode::Mailbox) {
		if (std::find(info.presentModes.begin(), info.presentModes.end(),
					core::PresentMode::Immediate)
				!= info.presentModes.end()) {
			ret.presentModeFast = core::PresentMode::Immediate;
		}
	}

	auto it = info.formats.begin();
	while (it != info.formats.end()) {
		if (it->first == imageFormat && it->second == colorSpace) {
			break;
		} else {
			++it;
		}
	}

	if (it == info.formats.end()) {
		ret.imageFormat = info.formats.front().first;
		ret.colorSpace = info.formats.front().second;
		log::info("Context",
			"handleAppWindowSurfaceUpdate: fail to find (imageFormat:colorspace) pair "
			"for a window, fallback to (",
			core::getImageFormatName(ret.imageFormat), ":",
			core::getColorSpaceName(ret.colorSpace), ")");
	} else {
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

void Context::handleAppWindowCreated(NotNull<AppThread> thread, NotNull<AppWindow> w,
		NotNull<Director> d) {
	log::info("Context", "handleAppWindowCreated");

	thread->addListener(w, [w](const UpdateTime &, bool wakeup) {
		if (wakeup) {
			w->setReadyForNextFrame();

			// force display link to update views
			w->update(core::PresentationUpdateFlags::DisplayLink);
		}
	});

	auto scene = makeScene(w, d->getFrameConstraints());
	if (scene) {
		d->runScene(move(scene));
	}
}

void Context::handleAppWindowDestroyed(NotNull<AppThread> thread, NotNull<AppWindow> w) {
	log::info("Context", "handleAppWindowDestroyed");
	thread->removeListener(w);
}

void Context::handleNativeWindowCreated(NotNull<NativeWindow> w) {
	log::info("Context", "handleNativeWindowCreated");

	if (auto appWindow = makeAppWindow(w)) {
		w->setAppWindow(move(appWindow));
	} else {
		log::error("Context", "Fail to create AppWindow for NativeWindow");
	}
}

void Context::handleNativeWindowDestroyed(NotNull<NativeWindow> w) {
	log::info("Context", "handleNativeWindowDestroyed");
	auto appWindow = w->getAppWindow();
	if (appWindow) {
		appWindow->close(true);
	}
}

void Context::handleNativeWindowRedrawNeeded(NotNull<NativeWindow>) {
	log::info("Context", "handleNativeWindowRedrawNeeded");
}

void Context::handleNativeWindowConstraintsChanged(NotNull<NativeWindow> w,
		core::UpdateConstraintsFlags flags) {
	log::info("Context", "handleNativeWindowConstraintsChanged");

	auto appWindow = w->getAppWindow();
	if (appWindow) {
		appWindow->updateConstraints(flags);
	}
}

void Context::handleNativeWindowInputEvents(NotNull<NativeWindow> w,
		Vector<core::InputEventData> &&events) {
	auto appWindow = w->getAppWindow();
	if (appWindow) {
		appWindow->handleInputEvents(sp::move(events));
	}
}

void Context::handleNativeWindowTextInput(NotNull<NativeWindow> w,
		const core::TextInputState &state) {
	auto appWindow = w->getAppWindow();
	if (appWindow) {
		appWindow->handleTextInput(state);
	}
}

void Context::handleLowMemory() {
	log::info("Context", "handleLowMemory");
	for (auto &it : _components) { it.second->handleLowMemory(this); }
}

void Context::handleWillDestroy() {
	log::info("Context", "handleWillDestroy");
	for (auto &it : _components) { it.second->handleDestroy(this); }
	_components.clear();

	_controller = nullptr;

	if (_loop) {
		_loop->stop();
		_loop = nullptr;
	}
}
void Context::handleDidDestroy() { log::info("Context", "handleDidDestroy"); }

void Context::handleWillStop() {
	log::info("Context", "handleWillStop");
	if (_running) {
		for (auto &it : _components) { it.second->handleStop(this); }

		_running = false;
	}
}

void Context::handleDidStop() {
	log::info("Context", "handleDidStop");
	if (_application) {
		_application->stop();
		_application->waitStopped();
		_application = nullptr;
	}
}

void Context::handleWillPause() {
	log::info("Context", "handleWillPause");
	for (auto &it : _components) { it.second->handlePause(this); }
}
void Context::handleDidPause() { log::info("Context", "handleDidPause"); }

void Context::handleWillResume() { log::info("Context", "handleWillResume"); }

void Context::handleDidResume() {
	log::info("Context", "handleDidResume");
	for (auto &it : _components) { it.second->handleResume(this); }
}

void Context::handleWillStart() {
	log::info("Context", "handleWillStart");
	_application = Rc<AppThread>::create(this);
}

void Context::handleDidStart() {
	log::info("Context", "handleDidStart");
	if (!_running) {
		for (auto &it : _components) { it.second->handleStart(this); }

		_application->run();

		_running = true;
	}
}

void Context::handleNetworkStateChanged(NetworkFlags flags) {
	for (auto &it : _components) { it.second->handleNetworkStateChanged(flags); }

	if (_application) {
		_application->handleNetworkStateChanged(flags);
	}
}

void Context::handleThemeInfoChanged(const ThemeInfo &info) {
	for (auto &it : _components) { it.second->handleThemeInfoChanged(info); }

	if (_application) {
		_application->handleThemeInfoChanged(info);
	}
}

bool Context::configureWindow(NotNull<WindowInfo> w) {
	auto caps = _controller->getCapabilities();

	for (auto flag : sp::flags(w->flags)) {
		switch (flag) {
		case WindowCreationFlags::UserSpaceDecorations:
			if (!hasFlag(caps, WindowCapabilities::UserSpaceDecorations)) {
				log::warn("Context", "WindowCreationFlags::UserSpaceDecorations is not supported");
				w->flags &= ~WindowCreationFlags::UserSpaceDecorations;
			}
			break;
		case WindowCreationFlags::DirectOutput:
			if (!hasFlag(caps, WindowCapabilities::DirectOutput)) {
				log::warn("Context", "WindowCreationFlags::DirectOutput is not supported");
				w->flags &= ~WindowCreationFlags::DirectOutput;
			}
			break;
		case WindowCreationFlags::PreferServerSideDecoration: break;
		case WindowCreationFlags::PreferNativeDecoration: break;
		case WindowCreationFlags::PreferServerSideCursors: break;
		default: break;
		}
	}

	return true;
}

void Context::updateMessageToken(BytesView tok) {
	if (tok != _messageToken) {
		onMessageToken(this, _messageToken);
	}
}

void Context::receiveRemoteNotification(Value &&val) { onRemoteNotification(this, move(val)); }

Rc<ScreenInfo> Context::getScreenInfo() const { return _controller->getScreenInfo(); }

Rc<AppWindow> Context::makeAppWindow(NotNull<NativeWindow> w) {
	return _controller->makeAppWindow(_application, w);
}

Rc<Scene> Context::makeScene(NotNull<AppWindow> w, const core::FrameConstraints &c) {
	Rc<Scene> scene;
	auto makeSceneSymbol = SharedModule::acquireTypedSymbol<SymbolMakeSceneSignature>(
			buildconfig::MODULE_APPCOMMON_NAME, SymbolMakeSceneName);
	if (makeSceneSymbol) {
		scene = makeSceneSymbol(_application, w, c);
	}
	if (!scene) {
		log::error("Context", "Fail to create scene for the window '", w->getInfo()->id, "'");
	}
	return scene;
}

void Context::initializeComponent(NotNull<ContextComponent> comp) {
	_controller->initializeComponent(comp);
}

} // namespace stappler::xenolith
