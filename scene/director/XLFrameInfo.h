/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_SCENE_DIRECTOR_XLFRAMEINFO_H_
#define XENOLITH_SCENE_DIRECTOR_XLFRAMEINFO_H_

#include "XLFrameContext.h"
#include "XLComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class InputListenerStorage;
class Director;

struct FrameInfo;
struct FrameContextHandle;

struct FrameInfo {
	Rc<PoolRef> pool;

	Rc<core::FrameRequest> request;
	Rc<Director> director;
	Rc<Scene> scene;
	Rc<InputListenerStorage> input;

	memory::vector<ZOrder> zPath;
	memory::vector<Mat4> viewProjectionStack;
	memory::vector<Mat4> modelTransformStack;
	memory::vector<float> depthStack;
	memory::vector<Rc<FrameContextHandle>> contextStack;
	memory::map<uint64_t, memory::vector<Rc<Component>>> componentsStack;
	memory::set<const core::AttachmentData *> resolvedInputs;

	FrameContextHandle *currentContext = nullptr;

	memory::vector<Rc<Component>> * pushComponent(const Rc<Component> &comp) {
		auto it = componentsStack.find(comp->getFrameTag());
		if (it == componentsStack.end()) {
			it = componentsStack.emplace(comp->getFrameTag()).first;
		}
		it->second.emplace_back(comp);
		return &it->second;
	}

	void popComponent(memory::vector<Rc<Component>> *vec) {
		vec->pop_back();
	}

	template<typename T = Component>
	auto getComponent(uint64_t tag) const -> Rc<T> {
		auto it = componentsStack.find(tag);
		if (it != componentsStack.end() && !it->second.empty()) {
			return it->second.back().cast<T>();
		}
		return nullptr;
	}

	void pushContext(FrameContext *ctx) {
		auto h = ctx->makeHandle(*this);

		currentContext = contextStack.emplace_back(move(h));
	}

	void popContext() {
		currentContext->context->submitHandle(*this, currentContext);
		contextStack.pop_back();
		if (contextStack.empty()) {
			currentContext = nullptr;
		} else {
			currentContext = contextStack.back();
		}
	}
};

}

#endif /* XENOLITH_SCENE_DIRECTOR_XLFRAMEINFO_H_ */
