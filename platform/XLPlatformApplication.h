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

#ifndef XENOLITH_PLATFORM_XLPLATFORMAPPLICATION_H_
#define XENOLITH_PLATFORM_XLPLATFORMAPPLICATION_H_

#include "XLCoreLoop.h"
#include "XLCoreInstance.h"
#include "SPThread.h"
#include "SPThreadTaskQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class PlatformApplication;

struct SP_PUBLIC UpdateTime {
	// global OS timer in microseconds
	uint64_t global = 0;

	// microseconds since application was started
	uint64_t app = 0;

	// microseconds since last update
	uint64_t delta = 0;

	// seconds since last update
	float dt = 0.0f;
};

struct SP_PUBLIC ApplicationInfo {
	// application reverce-domain name
	String bundleName = "org.stappler.xenolith.test";

	// application human-readable name
	String applicationName = "Xenolith";

	// application human-readable version
	String applicationVersion = "0.0.1";

	// current locale name
	String userLanguage = "ru-ru";

	// networking user agent
	String userAgent = "XenolithTestApp";

	// initial launch URL (deep link)
	String launchUrl;

	// version code in Vulkan format (see XL_MAKE_API_VERSION)
	uint32_t applicationVersionCode = 0;

	// application screen size (windows size of fullscreen size)
	Extent2 screenSize = Extent2(1024, 768);

	// Window decoration padding
	Padding viewDecoration;

	// Application screen pixel density
	float density = 1.0f;

	// Application screen DPI
	uint32_t dpi = 92;

	// Flag: application is launched on phone-like device
	bool isPhone = false;

	// Flag: application window is fixed in its borders
	bool isFixed = false;

	// Flag: debugging with renderdoc requested
	bool renderdoc = false;

	// Flag: graphic API validation debugging requested
	bool validation = true;

	// Flag: verbose output requested
	bool verbose = false;

	// Flag: application help requested
	bool help = false;

	// Native platform application controller pointer (if exists)
	void *platformHandle = nullptr;

	// Worker threads count, for async application-level tasks
	uint32_t threadsCount = 2;

	// Application event update interval (NOT screen update interval)
	TimeInterval updateInterval = TimeInterval::microseconds(100000);

	Function<void(const PlatformApplication &)> initCallback;
	Function<void(const PlatformApplication &, const UpdateTime &)> updateCallback;
	Function<void(const PlatformApplication &)> finalizeCallback;

	core::LoopInfo loopInfo;

	static int parseCmdSwitch(ApplicationInfo &ret, char c, const char *str);
	static int parseCmdString(ApplicationInfo &ret, const StringView &str, int argc, const char * argv[]);

	Value encode() const;
};

class SP_PUBLIC PlatformApplication : public thread::ThreadInterface<Interface> {
public:
	using Task = thread::Task;

	using ExecuteCallback = Function<bool(const Task &)>;
	using CompleteCallback = Function<void(const Task &, bool)>;

	virtual ~PlatformApplication();
	PlatformApplication();

	virtual bool init(ApplicationInfo &&info, Rc<core::Instance> &&instance);

	virtual void run();

	virtual void waitRunning();
	virtual void waitFinalized();

	virtual void threadInit() override;
	virtual void threadDispose() override;
	virtual bool worker() override;

	virtual void end();

	virtual void wakeup();

	bool isOnMainThread() const;

	void performOnGlThread(Function<void()> &&func, Ref *target = nullptr, bool immediate = false) const;

	/* If current thread is main thread: executes function/task
	   If not: adds function/task to main thread queue */
	void performOnMainThread(Function<void()> &&func, Ref *target = nullptr, bool onNextFrame = false);

	/* If current thread is main thread: executes function/task
	   If not: adds function/task to main thread queue */
    void performOnMainThread(Rc<Task> &&task, bool onNextFrame = false);

	/* Performs action in this thread, task will be constructed in place */
	void perform(ExecuteCallback &&, CompleteCallback && = nullptr, Ref * = nullptr);

	/* Performs task in thread, identified by id */
    void perform(Rc<Task> &&task);

	/* Performs task in thread, identified by id */
    void perform(Rc<Task> &&task, bool performFirst);

	thread::TaskQueue *getQueue() const { return _queue; }

	const ApplicationInfo &getInfo() const { return _info; }

	core::Loop *getGlLoop() const { return _glLoop; }

	virtual void openUrl(StringView) const;

	virtual void updateMessageToken(BytesView);
	virtual void receiveRemoteNotification(Value &&);

protected:
	virtual void handleDeviceStarted(const core::Loop &loop, const core::Device &dev);
	virtual void handleDeviceFinalized(const core::Loop &loop, const core::Device &dev);

	virtual void loadExtensions();
	virtual void initializeExtensions();
	virtual void finalizeExtensions();

	virtual void performAppUpdate(const UpdateTime &);
	virtual void performTimersUpdate(bool forced);

	struct WaitCallbackInfo {
		Function<void()> func;
		Rc<Ref> target;
		bool immediate = false;
	};

	Rc<thread::TaskQueue> _queue;
	ApplicationInfo _info;

	mutable std::atomic_flag _shouldQuit;

	std::atomic<bool> _running = false;
	std::mutex _runningMutex;
	std::condition_variable _runningVar;

	std::thread _thread;
	std::thread::id _threadId;

	Rc<core::Loop> _glLoop;
	Rc<core::Instance> _instance;
	const core::Device *_device = nullptr;

	mutable Vector<WaitCallbackInfo> _glWaitCallback;

	bool _extensionsInitialized = false;
	bool _immediateUpdate = false;

	UpdateTime _time;
	uint64_t _clock = 0;
	uint64_t _startTime = 0;
	uint64_t _lastUpdate = 0;

	Bytes _messageToken;
};

}

#endif /* XENOLITH_PLATFORM_XLPLATFORMAPPLICATION_H_ */
