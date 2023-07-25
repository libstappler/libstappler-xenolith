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

#ifndef XENOLITH_SCENE_DIRECTOR_XLFRAMECONTEXT_H_
#define XENOLITH_SCENE_DIRECTOR_XLFRAMECONTEXT_H_

#include "XLCoreQueue.h"
#include "XLCoreMaterial.h"
#include "XLNodeInfo.h"
#include "XLResourceOwner.h"

namespace stappler::xenolith {

struct FrameInfo;
struct FrameContextHandle;

class Scene;
class Director;

class FrameContext : public ResourceOwner {
public:
	virtual ~FrameContext();

	virtual bool init();

	virtual void onEnter(Scene *);
	virtual void onExit();

	virtual Rc<FrameContextHandle> makeHandle(FrameInfo &);
	virtual void submitHandle(FrameInfo &, FrameContextHandle *);

	virtual uint64_t getMaterial(const MaterialInfo &) const;
	virtual uint64_t acquireMaterial(const MaterialInfo &, Vector<core::MaterialImage> &&images, bool revokable);

	virtual void revokeImages(SpanView<uint64_t>);

protected:
	struct PipelineLayoutData {
		const core::PipelineLayoutData *layout;
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

	virtual core::ImageViewInfo getImageViewForMaterial(const MaterialInfo &, uint32_t idx, const core::ImageData *) const;
	virtual Bytes getDataForMaterial(const MaterialInfo &) const;

	virtual const core::GraphicPipelineData *getPipelineForMaterial(const MaterialInfo &) const;
	virtual bool isPipelineMatch(const core::GraphicPipelineData *, const MaterialInfo &) const;

	virtual void submitMaterials(const FrameInfo &);

	Scene *_scene = nullptr;
	Rc<core::Queue> _queue;

	const core::MaterialAttachment *_materialAttachment = nullptr;
	Vector<PipelineLayoutData> _layouts;

	HashMap<uint64_t, Vector<ContextMaterialInfo>> _materials;

	Vector<Rc<core::Material>> _pendingMaterialsToAdd;
	Vector<uint32_t> _pendingMaterialsToRemove;

	Rc<core::DependencyEvent> _materialDependency;

	// отозванные ид могут быть выданы новым отзываемым материалам, чтобы не засорять биндинги
	Vector<core::MaterialId> _revokedIds;
};

struct FrameContextHandle : public core::AttachmentInputData {
	Rc<Director> director; // allow to access director from rendering pipeline (to send stats)
	FrameContext *context = nullptr;

	memory::vector<StateId> stateStack;
	memory::vector<DrawStateValues> states;

	StateId addState(DrawStateValues values) {
		auto it = std::find(states.begin(), states.end(), values);
		if (it != states.end()) {
			return it - states.begin();
		} else {
			states.emplace_back(values);
			return states.size() - 1;
		}
	}

	const DrawStateValues *getState(StateId state) const {
		if (state < states.size()) {
			return &states.at(state);
		} else {
			return nullptr;
		}
	}
};

}

#endif /* XENOLITH_SCENE_DIRECTOR_XLFRAMECONTEXT_H_ */
