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

#ifndef XENOLITH_APPLICATION_XLCONTEXT_H_
#define XENOLITH_APPLICATION_XLCONTEXT_H_

#include "XLEvent.h"
#include "XLContextInfo.h"
#include "XLCoreTextInput.h"
#include "SPSharedModule.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class ContextController;
class ContextNativeWindow;

} // namespace stappler::xenolith::platform

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Context;
class AppThread;
class AppWindow;
class Director;
class Scene;

// Syntactic sugar for SharedModule definition

#define DEFINE_SCENE_FACTORY(SceneFactoryFunction) \
static_assert(::std::is_same_v<decltype(&SceneFactoryFunction),\
	STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolMakeSceneSignature>, \
	"Scene factory function should match :Context::SymbolMakeSceneSignature"); \
SP_USED static STAPPLER_VERSIONIZED_NAMESPACE::SharedExtension __macro_appCommonSceneFactorySymbol(\
		STAPPLER_VERSIONIZED_NAMESPACE::buildconfig::MODULE_APPCOMMON_NAME, \
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolMakeSceneName, \
		&SceneFactoryFunction);

#define DEFINE_PRIMARY_SCENE_CLASS(SceneClass) \
	static STAPPLER_VERSIONIZED_NAMESPACE::Rc<STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Scene> __macro_makeScene(\
		STAPPLER_VERSIONIZED_NAMESPACE::NotNull<STAPPLER_VERSIONIZED_NAMESPACE::xenolith::AppThread> app, \
		STAPPLER_VERSIONIZED_NAMESPACE::NotNull<STAPPLER_VERSIONIZED_NAMESPACE::xenolith::AppWindow> window, \
		const STAPPLER_VERSIONIZED_NAMESPACE::xenolith::core::FrameConstraints &constraints) { \
	return STAPPLER_VERSIONIZED_NAMESPACE::Rc<SceneClass>::create(app, window, constraints); \
} \
DEFINE_SCENE_FACTORY(__macro_makeScene)

#define DEFINE_CONTEXT_CONSTRUCTOR(ContextConstructorFunction) \
static_assert(::std::is_same_v<decltype(&ContextConstructorFunction),\
	STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolMakeContextSignature>, \
	"Context constructor function should match :Context::SymbolMakeContextSignature"); \
SP_USED static STAPPLER_VERSIONIZED_NAMESPACE::SharedExtension __macro_appCommonSContextConstructorSymbol(\
		STAPPLER_VERSIONIZED_NAMESPACE::buildconfig::MODULE_APPCOMMON_NAME, \
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolMakeContextName, \
		&ContextConstructorFunction);

#define DEFINE_CONTEXT_CLASS(ContextClass) \
	static STAPPLER_VERSIONIZED_NAMESPACE::Rc<STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context> __macro_makeContext(\
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::ContextConfig &&config) { \
	return STAPPLER_VERSIONIZED_NAMESPACE::Rc<ContextClass>::create(STAPPLER_VERSIONIZED_NAMESPACE::move(config)); \
} \
DEFINE_CONTEXT_CONSTRUCTOR(__macro_makeContext)

using NativeWindow = platform::ContextNativeWindow;

class ContextComponent : public Ref {
public:
	virtual ~ContextComponent() { }

	virtual bool init() { return true; }

	virtual void handleStart(Context *a) { }
	virtual void handleResume(Context *a) { }
	virtual void handlePause(Context *a) { }
	virtual void handleStop(Context *a) { }
	virtual void handleDestroy(Context *a) { }
	virtual void handleLowMemory(Context *a) { }
};

/** Context: main application class, that allow workflow customization
	
 By default, it uses appcommon (MODULE_APPCOMMON_NAME) SharedModule to access user overrides:
 
 (see Context::Symbol* constants)
 
 - Context::SymbolHelpStringName - string with application CLI help
 -  --or --
 - Context::SymbolPrintHelpName - function to print application help
 
 - Context::SymbolParseConfigName - function to parse command line data to ContextConfig
 - Context::SymbolMakeContextName - function to create (possibly subclassed) Context
 - Context::SymbolMakeScemeName - function to create application Scene for a view
 
 It's essential to define Context::SymbolMakeScemeName or override `makeScene` for application to work
*/

class Context : public Ref {
public:
	using SwapchainConfig = core::SwapchainConfig;
	using SurfaceInfo = core::SurfaceInfo;

	static EventHeader onMessageToken;
	static EventHeader onRemoteNotification;

	using SymbolHelpStringSignature = const char *;
	static constexpr auto SymbolHelpStringName = "HELP_STRING";

	using SymbolPrintHelpSignature = void (*)(const ContextConfig &, int argc, const char **);
	static constexpr auto SymbolPrintHelpName = "printHelp";

	using SymbolParseConfigSignature = ContextConfig (*)(int argc, const char **);
	static constexpr auto SymbolParseConfigName = "parseConfig";

	using SymbolMakeContextSignature = Rc<Context> (*)(ContextConfig &&);
	static constexpr auto SymbolMakeContextName = "makeContext";

	using SymbolMakeSceneSignature = Rc<Scene> (*)(NotNull<AppThread>, NotNull<AppWindow>,
			const core::FrameConstraints &);
	static constexpr auto SymbolMakeSceneName = "makeScene";

	// Symbol name for the MODULE_XENOLITH_APPLICATION
	static constexpr auto SymbolContextRunName = "Context::run";

	static int run(int argc, const char **argv);

	Context();
	virtual ~Context();

	virtual bool init(ContextConfig &&);

	const ContextInfo *getInfo() const { return _info; }
	event::Looper *getLooper() const { return _looper; }

	core::Loop *getGlLoop() const { return _loop; }

	BytesView getMessageToken() const { return _messageToken; }

	virtual void performOnThread(Function<void()> &&func, Ref *target = nullptr,
			bool immediate = false, StringView tag = STAPPLER_LOCATION) const;

	template <typename T>
	auto addComponent(Rc<T> &&) -> T *;

	template <typename T>
	T *getComponent() const;

	virtual Status readFromClipboard(Function<void(BytesView, StringView)> &&,
			Function<StringView(SpanView<StringView>)> &&, Ref * = nullptr);
	virtual Status writeToClipboard(Function<Bytes(StringView)> &&, SpanView<String>,
			Ref * = nullptr);

	virtual void handleConfigurationChanged(Rc<ContextInfo> &&);

	virtual void handleGraphicsLoaded(NotNull<core::Loop>);

	virtual Value saveState();

	virtual void handleAppThreadCreated(NotNull<AppThread>);
	virtual void handleAppThreadDestroyed(NotNull<AppThread>);
	virtual void handleAppThreadUpdate(NotNull<AppThread>, const UpdateTime &time);

	virtual SwapchainConfig handleAppWindowSurfaceUpdate(NotNull<AppWindow>, const SurfaceInfo &,
			bool fastMode);
	virtual void handleAppWindowCreated(NotNull<AppWindow>, NotNull<Director>);
	virtual void handleAppWindowDestroyed(NotNull<AppWindow>);

	virtual void handleNativeWindowCreated(NotNull<NativeWindow>);
	virtual void handleNativeWindowDestroyed(NotNull<NativeWindow>);
	virtual void handleNativeWindowRedrawNeeded(NotNull<NativeWindow>);
	virtual void handleNativeWindowConstraintsChanged(NotNull<NativeWindow>, bool liveResize);
	virtual void handleNativeWindowInputEvents(NotNull<NativeWindow>,
			Vector<core::InputEventData> &&);
	virtual void handleNativeWindowTextInput(NotNull<NativeWindow>, const core::TextInputState &);

	virtual void handleLowMemory();

	virtual void handleWillDestroy();
	virtual void handleDidDestroy();

	virtual void handleWillStop();
	virtual void handleDidStop();

	virtual void handleWillPause();
	virtual void handleDidPause();

	virtual void handleWillResume();
	virtual void handleDidResume();

	virtual void handleWillStart();
	virtual void handleDidStart();

	virtual void addNetworkCallback(Ref *key, Function<void(NetworkFlags)> &&);
	virtual void removeNetworkCallback(Ref *key);

	virtual void updateMessageToken(BytesView tok);
	virtual void receiveRemoteNotification(Value &&val);

	virtual Rc<ScreenInfo> getScreenInfo() const;

protected:
	virtual Rc<AppWindow> makeAppWindow(NotNull<NativeWindow>);

	virtual Rc<core::PresentationEngine> makePresentationEngine(NotNull<core::PresentationWindow>);

	virtual Rc<Scene> makeScene(NotNull<AppWindow>, const core::FrameConstraints &);

	virtual void initializeComponent(NotNull<ContextComponent>);

	memory::allocator_t *_alloc = nullptr;
	memory::pool_t *_pool = nullptr;
	memory::pool_t *_tmpPool = nullptr;

	event::Looper *_looper = nullptr;

	bool _isEmulator = false;
	bool _valid = false;
	bool _running = false;

	Rc<ContextInfo> _info;

	Bytes _messageToken;

	Rc<platform::ContextController> _controller;

	Rc<core::Loop> _loop;

	Rc<AppThread> _application;
	Rc<NativeWindow> _rootWindow;

	HashMap<std::type_index, Rc<ContextComponent>> _components;
};

template <typename T>
auto Context::addComponent(Rc<T> &&t) -> T * {
	auto it = _components.find(std::type_index(typeid(T)));
	if (it == _components.end()) {
		it = _components.emplace(std::type_index(typeid(*t.get())), move(t)).first;
		initializeComponent(it->second);
	}
	return it->second.get_cast<T>();
}

template <typename T>
auto Context::getComponent() const -> T * {
	auto it = _components.find(std::type_index(typeid(T)));
	if (it != _components.end()) {
		return reinterpret_cast<T *>(it->second.get());
	}
	return nullptr;
}


} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLCONTEXT_H_ */
