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

#include "XLPlatformMacosObjc.h"
#include "XLPlatformApplication.h"
#include "XLCoreInput.h"

namespace stappler::xenolith::platform {

struct MacosApplicationData {
	CFRunLoopRef loop = nullptr;
	CFRunLoopSourceRef updateSource = nullptr;
	CFRunLoopTimerRef updateTimer = nullptr;

	memory::allocator_t *alloc = nullptr;
	memory::pool_t *pool = nullptr;
	NSApplication *application = nullptr;
	XLMacAppDelegate *delegate = nullptr;

	std::thread::id mainThread;
	std::mutex outputMutex;
	std::vector<Rc<thread::Task>> outputQueue;
	std::vector<Pair<std::function<void()>, Rc<Ref>>> outputCallbacks;

	TimeInterval updateInterval = TimeInterval::seconds(1);

	MacosApplicationData();
	~MacosApplicationData();

	void run();
	void stop();

	void updateOnTimer();
	void updateOnSource();

	void perform(Function<void()> &&cb, Ref *ref);
};

static MacosApplicationData s_appData;

void openUrl(StringView url) {
	auto nsurl = [NSURL URLWithString:[NSString stringWithUTF8String:url.terminated()?url.data():url.str<Interface>().data()]];
	[[NSWorkspace sharedWorkspace] openURL:nsurl];
}

bool isOnMainThread() {
	return std::this_thread::get_id() == s_appData.mainThread;
}

void performOnMainThread(Function<void()> &&cb, Ref *ref) {
	s_appData.perform(move(cb), ref);
}

void runApplication() {
	s_appData.run();
}

void stopApplication() {
	s_appData.stop();
}

MacosApplicationData::MacosApplicationData() {
	platform::clock(core::ClockType::Monotonic); // ensure clock is initialized

	delegate = [[XLMacAppDelegate alloc] init];
	application = [NSApplication sharedApplication];

	if (!application.delegate) {
		/*if ((flags & ApplicationFlags::GuiApplication) != ApplicationFlags::None) {
			app.activationPolicy = NSApplicationActivationPolicyRegular;
			app.presentationOptions = NSApplicationPresentationDefault;
		}*/

		application.delegate = delegate;

		[NSThread detachNewThreadSelector:@selector(doNothing:) toTarget:delegate withObject:nil];
	}

	alloc = memory::allocator::create();
	pool = memory::pool::create(alloc);

	loop = CFRunLoopGetCurrent();

	CFRunLoopSourceContext ctx = CFRunLoopSourceContext{
		.version = 0,
		.info = this,
		.retain = nullptr,
		.release = nullptr,
		.copyDescription = nullptr,
		.equal = nullptr,
		.hash = nullptr,
		.schedule = nullptr,
		.cancel = nullptr,
		.perform = [] (void *info) {
			auto data = (MacosApplicationData *)info;

			data->updateOnSource();
		}
	};

	updateSource = CFRunLoopSourceCreate(nullptr, 0, &ctx);

	CFRunLoopAddSource(loop, updateSource, kCFRunLoopDefaultMode);

	mainThread = std::this_thread::get_id();
}

MacosApplicationData::~MacosApplicationData() {
	CFRunLoopRemoveSource(loop, updateSource, kCFRunLoopDefaultMode);
	CFRelease(updateSource);
}

void MacosApplicationData::run() {
	CFRunLoopTimerContext tctx = CFRunLoopTimerContext{
		.version = 0,
		.info = this,
		.retain = nullptr,
		.release = nullptr,
		.copyDescription = nullptr,
	};

	updateTimer = CFRunLoopTimerCreate(nullptr, 0, updateInterval.toFloatSeconds(), 0, 0, [] (CFRunLoopTimerRef timer, void *info) {
		auto data = (MacosApplicationData *)info;
		data->updateOnTimer();
	}, &tctx);

	CFRunLoopAddTimer(loop, updateTimer, kCFRunLoopDefaultMode);

	if (![[NSRunningApplication currentApplication] isFinishedLaunching]) {
		[NSApp run];
	}

	CFRunLoopRemoveTimer(loop, updateTimer, kCFRunLoopDefaultMode);
	CFRelease(updateTimer);

	updateTimer = nullptr;
}

void MacosApplicationData::stop() {
	if (delegate) {
		XL_OBJC_CALL([NSApp stop:delegate]);
		XL_OBJC_CALL([NSApp abortModal]);
	}
}

void MacosApplicationData::updateOnTimer() {

}

void MacosApplicationData::updateOnSource() {
	outputMutex.lock();

	auto stack = sp::move(outputQueue);
	auto callbacks = sp::move(outputCallbacks);

	outputQueue.clear();
	outputCallbacks.clear();

	outputMutex.unlock();

	memory::pool::perform_clear([&] {
		for (Rc<thread::Task> &task : stack) {
			task->onComplete();
		}

		for (Pair<std::function<void()>, Rc<Ref>> &task : callbacks) {
			task.first();
		}
	}, pool);
}

void MacosApplicationData::perform(Function<void()> &&cb, Ref *ref) {
	std::unique_lock lock(outputMutex);

	outputCallbacks.emplace_back(move(cb), ref);

	lock.unlock();

	CFRunLoopSourceSignal(updateSource);
}


}
