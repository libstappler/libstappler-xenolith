/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_APPLICATION_XLAPPLICATION_H_
#define XENOLITH_APPLICATION_XLAPPLICATION_H_

#include "XLPlatformApplication.h"
#include "XLEvent.h"
#include "XLResourceCache.h"
#include "XLApplicationExtension.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class SP_PUBLIC Application : public PlatformApplication {
public:
	static EventHeader onMessageToken;
	static EventHeader onRemoteNotification;

	// thread-local
	static Application *getInstance();

	virtual ~Application();

	virtual void threadInit() override;
	virtual void threadDispose() override;

	const Rc<ResourceCache> &getResourceCache() const { return _resourceCache; }

	template <typename T>
	bool addExtension(Rc<T> &&);

	template <typename T>
	T *getExtension() const;

	virtual void updateMessageToken(BytesView) override;
	virtual void receiveRemoteNotification(Value &&) override;

protected:
	virtual void performAppUpdate(const UpdateTime &) override;

	virtual void loadExtensions() override;
	virtual void initializeExtensions() override;
	virtual void finalizeExtensions() override;

	bool _hasViews = false;

	Rc<ResourceCache> _resourceCache;

	HashMap<std::type_index, Rc<ApplicationExtension>> _extensions;
};

template <typename T>
bool Application::addExtension(Rc<T> &&t) {
	auto it = _extensions.find(std::type_index(typeid(T)));
	if (it == _extensions.end()) {
		auto ref = t.get();
		_extensions.emplace(std::type_index(typeid(*t.get())), move(t));
		if (_extensionsInitialized) {
			ref->initialize(this);
		}
		return true;
	} else {
		return false;
	}
}

template <typename T>
auto Application::getExtension() const -> T * {
	auto it = _extensions.find(std::type_index(typeid(T)));
	if (it != _extensions.end()) {
		return reinterpret_cast<T *>(it->second.get());
	}
	return nullptr;
}


} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLAPPLICATION_H_ */
