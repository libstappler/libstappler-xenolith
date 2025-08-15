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

#ifndef XENOLITH_APPLICATION_DIRECTOR_XLFRAMECONTEXT_H_
#define XENOLITH_APPLICATION_DIRECTOR_XLFRAMECONTEXT_H_

#include "XLCoreQueueData.h"
#include "XLResourceOwner.h"
#include "XLCoreQueue.h"
#include "XLCoreMaterial.h"
#include "XLCoreFrameRequest.h"
#include "XLNodeInfo.h"
#include "XLComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

struct FrameInfo;
struct FrameContextHandle;
class InputListenerStorage;

class Scene;
class Director;
class FrameStateOwnerInterface;

class SP_PUBLIC FrameContext : public ResourceOwner {
public:
	virtual ~FrameContext() = default;

	virtual bool init();

	virtual void onEnter(Scene *);
	virtual void onExit();

	virtual Rc<FrameContextHandle> makeHandle(FrameInfo &);
	virtual void submitHandle(FrameInfo &, FrameContextHandle *);

	virtual core::MaterialId getMaterial(const MaterialInfo &) const;
	virtual core::MaterialId acquireMaterial(const core::PipelineFamilyInfo *family,
			const MaterialInfo &, Vector<core::MaterialImage> &&images, Ref *data, bool revokable);

	virtual void revokeImages(SpanView<uint64_t>);

protected:
	struct PipelineLayoutData {
		const core::PipelineFamilyInfo *family = nullptr;
		const core::PipelineLayoutData *layout = nullptr;
		HashMap<size_t, Vector<const core::GraphicPipelineData *>> pipelines;
	};

	struct ContextMaterialInfo {
		MaterialInfo info;
		core::MaterialId id;
		bool revokable;
	};

	void readMaterials(core::MaterialAttachment *a);
	MaterialInfo getMaterialInfo(const Rc<core::Material> &material) const;

	void addPendingMaterial(Rc<core::Material> &&);
	void addMaterial(const MaterialInfo &, core::MaterialId, bool revokable);
	String listMaterials() const;

	virtual core::ImageViewInfo getImageViewForMaterial(const core::GraphicPipelineData *pipeline,
			const MaterialInfo &, uint32_t idx, const core::ImageData *) const;

	virtual const core::GraphicPipelineData *getPipelineForMaterial(
			const core::PipelineFamilyInfo *, const MaterialInfo &,
			SpanView<core::MaterialImage> images) const;
	virtual bool isPipelineMatch(const core::GraphicPipelineData *,
			const core::PipelineFamilyInfo *, const MaterialInfo &,
			SpanView<core::MaterialImage> images) const;

	virtual void submitMaterials(const FrameInfo &);

	Scene *_scene = nullptr;
	Rc<core::Queue> _queue;

	const core::MaterialAttachment *_materialAttachment = nullptr;
	Set<const core::PipelineLayoutData *> _layouts;
	Map<const core::PipelineFamilyInfo *, PipelineLayoutData> _families;

	HashMap<uint64_t, Vector<ContextMaterialInfo>> _materials;

	Vector<Rc<core::Material>> _pendingMaterialsToAdd;
	Vector<uint32_t> _pendingMaterialsToRemove;

	Rc<core::DependencyEvent> _materialDependency;

	// отозванные ид могут быть выданы новым отзываемым материалам, чтобы не засорять биндинги
	Vector<core::MaterialId> _revokedIds;
};

struct SP_PUBLIC FrameContextHandle : public core::AttachmentInputData {
	virtual ~FrameContextHandle() = default;

	uint64_t clock;
	Rc<Director> director; // allow to access director from rendering pipeline (to send stats)
	FrameContext *context = nullptr;

	memory::vector<Pair<StateId, FrameStateOwnerInterface *>> stateStack;
	memory::vector<DrawStateValues> states;

	StateId addState(DrawStateValues values) {
		auto it = std::find(states.begin(), states.end(), values);
		if (it != states.end()) {
			//log::verbose("FrameContextHandle", "addStateRet ", it - states.begin(), " ", values.scissor);
			return StateId(it - states.begin());
		} else {
			states.emplace_back(values);
			//log::verbose("FrameContextHandle", "addState ", states.size() - 1, " ", values.scissor);
			return StateId(states.size() - 1);
		}
	}

	const DrawStateValues *getState(StateId state) const {
		if (state < states.size()) {
			return &states.at(state);
		} else {
			return nullptr;
		}
	}

	StateId getCurrentState() const {
		return stateStack.empty() ? maxOf<StateId>() : stateStack.back().first;
	}
};

class SP_PUBLIC FrameStateOwnerInterface {
public:
	virtual ~FrameStateOwnerInterface() = default;

	// Object should use context to fully rebuild previously pushed state
	virtual StateId rebuildState(FrameContextHandle &) = 0;
};

struct SP_PUBLIC FrameInfo {
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

	uint32_t focusValue = 0;

	FrameContextHandle *currentContext = nullptr;

	memory::vector<Rc<Component>> *pushComponent(const Rc<Component> &comp) {
		auto it = componentsStack.find(comp->getFrameTag());
		if (it == componentsStack.end()) {
			it = componentsStack.emplace(comp->getFrameTag()).first;
		}
		it->second.emplace_back(comp);
		return &it->second;
	}

	void popComponent(memory::vector<Rc<Component>> *vec) { vec->pop_back(); }

	template <typename T = Component>
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
} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_DIRECTOR_XLFRAMECONTEXT_H_ */
