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

#ifndef XENOLITH_APPLICATION_XLAPPLICATION_H_
#define XENOLITH_APPLICATION_XLAPPLICATION_H_

#include "XLEventHeader.h"
#include "XLResourceCache.h"
#include "SPThreadTaskQueue.h"
#include "XLApplicationExtension.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Event;
class EventHandlerNode;

class SP_PUBLIC Application : public thread::ThreadInterface<Interface> {
public:
	static EventHeader onMessageToken;
	static EventHeader onRemoteNotification;

	struct CommonInfo {
		String bundleName;
		String applicationName;
		String applicationVersion;
		String userAgent;
		String locale;

		uint32_t applicationVersionCode = 0;
		void *nativeHandle = nullptr;

		int dpi = 92;
	};

	struct RunInfo {
		Function<void(const Application &)> initCallback;
		Function<void(const Application &, const UpdateTime &)> updateCallback;
		Function<void(const Application &)> finalizeCallback;

		core::LoopInfo loopInfo;
		uint32_t threadsCount = 2;
		TimeInterval updateInterval = TimeInterval::microseconds(100000);
	};

	using Task = thread::Task;

	using ExecuteCallback = Function<bool(const Task &)>;
	using CompleteCallback = Function<void(const Task &, bool)>;

	static Application *getInstance();

	virtual ~Application();

	Application();

	virtual bool init(CommonInfo &&info, Rc<core::Instance> &&instance);

	virtual void run(RunInfo &&);

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

	void addEventListener(const EventHandlerNode *listener);
	void removeEventListner(const EventHandlerNode *listener);

	void removeAllEventListeners();
	void dispatchEvent(const Event &ev);

	uint64_t getClock() const;

	const Rc<ResourceCache> &getResourceCache() const { return _resourceCache; }
	const Rc<core::Loop> &getGlLoop() const { return _glLoop; }

	thread::TaskQueue *getQueue();

	template <typename T>
	bool addExtension(Rc<T> &&);

	template <typename T>
	T *getExtension();

	StringView getMessageToken() const { return _messageToken; }
	const CommonInfo &getInfo() const { return _info; }

	void openUrl(StringView) const;

protected:
	virtual void handleDeviceStarted(const core::Loop &loop, const core::Device &dev);
	virtual void handleDeviceFinalized(const core::Loop &loop, const core::Device &dev);

	virtual void handleMessageToken(String &&);
	virtual void handleRemoteNotification(Value &&);

	virtual void performAppUpdate(const RunInfo &, const UpdateTime &);
	virtual void performTimersUpdate(const RunInfo &cb, bool forced);

	virtual void runExtensions();

	struct WaitCallbackInfo {
		Function<void()> func;
		Rc<Ref> target;
		bool immediate = false;
	};

	Rc<thread::TaskQueue> _queue;
	UpdateTime _time;
	uint64_t _clock = 0;
	uint64_t _startTime = 0;
	uint64_t _lastUpdate = 0;

	bool _started = false;
	bool _immediateUpdate = false;
	bool _hasViews = false;
	mutable std::atomic_flag _shouldQuit;
	HashMap<EventHeader::EventID, HashSet<const EventHandlerNode *>> _eventListeners;
	Rc<ResourceCache> _resourceCache;
	Rc<core::Loop> _glLoop;
	Rc<core::Instance> _instance;
	const core::Device *_device = nullptr;

	HashMap<std::type_index, Rc<ApplicationExtension>> _extensions;

	String _messageToken;
	CommonInfo _info;

	mutable Vector<WaitCallbackInfo> _glWaitCallback;

	std::atomic<bool> _running = false;
	std::mutex _runningMutex;
	std::condition_variable _runningVar;

	std::thread _thread;
	std::thread::id _threadId;

	RunInfo _runInfo;
};

template <typename T>
bool Application::addExtension(Rc<T> &&t) {
	auto it = _extensions.find(std::type_index(typeid(T)));
	if (it == _extensions.end()) {
		auto ref = t.get();
		_extensions.emplace(std::type_index(typeid(*t.get())), move(t));
		if (_started) {
			ref->initialize(this);
		}
		return true;
	} else {
		return false;
	}
}

template <typename T>
auto Application::getExtension() -> T * {
	auto it = _extensions.find(std::type_index(typeid(T)));
	if (it != _extensions.end()) {
		return reinterpret_cast<T *>(it->second.get());
	}
	return nullptr;
}


}

#endif /* XENOLITH_APPLICATION_XLAPPLICATION_H_ */
