/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "XLApplication.h"
#include "XLEvent.h"
#include "XLEventHandler.h"
#include "XLResourceCache.h"
#include "XLCoreDevice.h"
#include "XLCoreQueue.h"
#include "SPSharedModule.h"

#if MODULE_XENOLITH_FONT

#include "XLFontExtension.h"
#include "XLFontLocale.h"

#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

static thread_local Application *tl_mainLoop = nullptr;

XL_DECLARE_EVENT_CLASS(Application, onMessageToken)
XL_DECLARE_EVENT_CLASS(Application, onRemoteNotification)

Application *Application::getInstance() {
	if (tl_mainLoop) {
		return tl_mainLoop;
	} else {
		// Find application in thread hierarchy
		return const_cast<Application *>(Thread::findSpecificThread<Application>());
	}
}

Application::~Application() { }

void Application::threadInit() {
	tl_mainLoop = this;

	_resourceCache = Rc<ResourceCache>::create(this);

	PlatformApplication::threadInit();
}

void Application::threadDispose() {
	_resourceCache->invalidate();

	PlatformApplication::threadDispose();

	tl_mainLoop = nullptr;
}

void Application::addEventListener(const EventHandlerNode *listener) const {
	auto it = _eventListeners.find(listener->getEventID());
	if (it != _eventListeners.end()) {
		it->second.insert(listener);
	} else {
		_eventListeners.emplace(listener->getEventID(),
				HashSet<const EventHandlerNode *>{listener});
	}
}

void Application::removeEventListner(const EventHandlerNode *listener) const {
	auto it = _eventListeners.find(listener->getEventID());
	if (it != _eventListeners.end()) {
		it->second.erase(listener);
	}
}

void Application::removeAllEventListeners() const { _eventListeners.clear(); }

void Application::dispatchEvent(const Event &ev) const {
	if (_eventListeners.size() > 0) {
		auto it = _eventListeners.find(ev.getHeader().getEventID());
		if (it != _eventListeners.end() && it->second.size() != 0) {
			Vector<const EventHandlerNode *> listenersToExecute;
			auto &listeners = it->second;
			for (auto l : listeners) {
				if (l->shouldRecieveEventWithObject(ev.getEventID(), ev.getObject())) {
					listenersToExecute.push_back(l);
				}
			}

			for (auto l : listenersToExecute) { l->onEventRecieved(ev); }
		}
	}
}

void Application::updateMessageToken(BytesView tok) {
	if (tok != _messageToken) {
		PlatformApplication::updateMessageToken(tok);
		onMessageToken(this, _messageToken);
	}
}

void Application::receiveRemoteNotification(Value &&val) { onRemoteNotification(this, move(val)); }

void Application::performAppUpdate(const UpdateTime &t) {
	PlatformApplication::performAppUpdate(t);

	for (auto &it : _extensions) { it.second->update(this, t); }
}

void Application::loadExtensions() {
	PlatformApplication::loadExtensions();

#if MODULE_XENOLITH_FONT
	auto setLocale = SharedModule::acquireTypedSymbol<decltype(&locale::setLocale)>(
			buildconfig::MODULE_XENOLITH_FONT_NAME, "locale::setLocale");
	if (setLocale) {
		setLocale(_info.userLanguage);
	}

	auto createQueue =
			SharedModule::acquireTypedSymbol<decltype(&font::FontExtension::createFontQueue)>(
					buildconfig::MODULE_XENOLITH_FONT_NAME, "FontExtension::createFontQueue");
	auto createFontExtension =
			SharedModule::acquireTypedSymbol<decltype(&font::FontExtension::createFontExtension)>(
					buildconfig::MODULE_XENOLITH_FONT_NAME, "FontExtension::createFontExtension");
	auto createFontController = SharedModule::acquireTypedSymbol<
			decltype(&font::FontExtension::createDefaultController)>(
			buildconfig::MODULE_XENOLITH_FONT_NAME, "FontExtension::createDefaultController");

	if (createFontExtension && createQueue && createFontController) {
		auto lib = createFontExtension(this, createQueue(_instance, "ApplicationFontQueue"));

		addExtension(createFontController((font::FontExtension *)lib.get(),
				"ApplicationFontController"));
		addExtension(move(lib));
	}
#endif
}

void Application::initializeExtensions() {
	PlatformApplication::initializeExtensions();

	for (auto &it : _extensions) { it.second->initialize(this); }

	_resourceCache->initialize(*_glLoop);
}

void Application::finalizeExtensions() {
	for (auto &it : _extensions) { it.second->invalidate(this); }

	_resourceCache->invalidate();
	_resourceCache = nullptr;

	PlatformApplication::finalizeExtensions();
}

} // namespace stappler::xenolith
