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
class NativeWindow;

} // namespace stappler::xenolith::platform

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Context;
class AppThread;
class AppWindow;
class Director;
class Scene;

// Syntactic sugar for SharedModule definition

// Usage: DEFINE_CONFIG_FUNCTION((ContextConfig &config){ })

#define DEFINE_CONFIG_FUNCTION(Function)\
SP_USED static STAPPLER_VERSIONIZED_NAMESPACE::SharedExtension __macro_appMakeConfigSymbol(\
		STAPPLER_VERSIONIZED_NAMESPACE::buildconfig::MODULE_APPCOMMON_NAME,\
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolMakeConfigName,\
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolMakeConfigSignature(\
				[]Function));

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

using NativeWindow = platform::NativeWindow;

class ContextComponent : public Ref {
public:
	virtual ~ContextComponent() { }

	virtual bool init() { return true; }

	virtual void handleStart(Context *a) { }
	virtual void handleResume(Context *a) { }
	virtual void handlePause(Context *a) { }
	virtual void handleStop(Context *a) { }
	virtual void handleDestroy(Context *a) { }
	virtual void handleSystemNotification(Context *a, SystemNotification) { }

	virtual void handleNetworkStateChanged(NetworkFlags) { }
	virtual void handleThemeInfoChanged(const ThemeInfo &) { }
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

	static EventHeader onNetworkStateChanged;
	static EventHeader onThemeChanged;
	static EventHeader onSystemNotification;

	static EventHeader onMessageToken;
	static EventHeader onRemoteNotification;

	using SymbolHelpStringSignature = const char *;
	static constexpr auto SymbolHelpStringName = "HELP_STRING";

	using SymbolPrintHelpSignature = void (*)(const ContextConfig &, int argc, const char **);
	static constexpr auto SymbolPrintHelpName = "printHelp";

	using SymbolParseConfigCmdSignature = ContextConfig (*)(int argc, const char **);
	static constexpr auto SymbolParseConfigCmdName = "parseConfigCmd";

	using SymbolParseConfigNativeSignature = ContextConfig (*)(NativeContextHandle *);
	static constexpr auto SymbolParseConfigNativeName = "parseConfigNative";

	using SymbolMakeContextSignature = Rc<Context> (*)(ContextConfig &&);
	static constexpr auto SymbolMakeContextName = "makeContext";

	using SymbolMakeSceneSignature = Rc<Scene> (*)(NotNull<AppThread>, NotNull<AppWindow>,
			const core::FrameConstraints &);
	static constexpr auto SymbolMakeSceneName = "makeScene";

	using SymbolMakeConfigSignature = void (*)(ContextConfig &);
	static constexpr auto SymbolMakeConfigName = "makeConfig";

	// Symbol name for the MODULE_XENOLITH_APPLICATION
	using SymbolRunCmdSignature = int (*)(int argc, const char **argv);
	using SymbolRunNativeSignature = int (*)(NativeContextHandle *);
	static constexpr auto SymbolContextRunName = "Context::run";

	static int run(int argc, const char **argv);
	static int run(NativeContextHandle *);

	Context();
	virtual ~Context();

	virtual bool init(ContextConfig &&);

	const ContextInfo *getInfo() const { return _info; }
	event::Looper *getLooper() const { return _looper; }

	core::Loop *getGlLoop() const { return _loop; }

	platform::ContextController *getController() const { return _controller; }

	BytesView getMessageToken() const { return _messageToken; }

	virtual void performOnThread(Function<void()> &&func, Ref *target = nullptr,
			bool immediate = false, StringView tag = SP_FUNC) const;

	template <typename T>
	auto addComponent(Rc<T> &&) -> T *;

	template <typename T>
	T *getComponent() const;

	bool isCursorSupported(WindowCursor, bool serverSide) const;
	WindowCapabilities getWindowCapabilities() const;

	virtual Status readFromClipboard(Function<void(Status, BytesView, StringView)> &&,
			Function<StringView(SpanView<StringView>)> &&, Ref * = nullptr);
	virtual Status probeClipboard(Function<void(Status, SpanView<StringView>)> &&, Ref * = nullptr);
	virtual Status writeToClipboard(Function<Bytes(StringView)> &&, SpanView<String>,
			Ref * = nullptr, StringView label = StringView());

	virtual void handleConfigurationChanged(Rc<ContextInfo> &&);

	virtual void handleGraphicsLoaded(NotNull<core::Loop>);

	virtual Value saveState();

	virtual void handleAppThreadCreated(NotNull<AppThread>);
	virtual void handleAppThreadDestroyed(NotNull<AppThread>);
	virtual void handleAppThreadUpdate(NotNull<AppThread>, const UpdateTime &time);

	virtual SwapchainConfig handleAppWindowSurfaceUpdate(NotNull<AppWindow>, const SurfaceInfo &,
			bool fastMode);

	virtual void handleNativeWindowCreated(NotNull<NativeWindow>);
	virtual void handleNativeWindowDestroyed(NotNull<NativeWindow>);
	virtual void handleNativeWindowConstraintsChanged(NotNull<NativeWindow>,
			core::UpdateConstraintsFlags);
	virtual void handleNativeWindowInputEvents(NotNull<NativeWindow>,
			Vector<core::InputEventData> &&);
	virtual void handleNativeWindowTextInput(NotNull<NativeWindow>, const core::TextInputState &);

	virtual void handleSystemNotification(SystemNotification);

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

	virtual void handleNetworkStateChanged(NetworkFlags);
	virtual void handleThemeInfoChanged(const ThemeInfo &);

	virtual bool configureWindow(NotNull<WindowInfo>);

	virtual void updateMessageToken(BytesView tok);
	virtual void receiveRemoteNotification(Value &&val);

	virtual Rc<ScreenInfo> getScreenInfo() const;

	template <typename Callback>
	auto performTemporary(const Callback &cb) {
		return memory::pool::perform_clear(cb, _tmpPool);
	}

protected:
	virtual Rc<AppWindow> makeAppWindow(NotNull<NativeWindow>);

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
