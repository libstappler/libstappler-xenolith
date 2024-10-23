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

#include "XLFrameContext.h"
#include "XLFrameInfo.h"
#include "XLDirector.h"
#include "XLScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

FrameContext::~FrameContext() { }

bool FrameContext::init() {
	return true;
}

void FrameContext::onEnter(Scene *scene) {
	_scene = scene;
	_queue = _scene->getQueue();
}

void FrameContext::onExit() {
	_queue = nullptr;
	_scene = nullptr;
}

SP_COVERAGE_TRIVIAL
Rc<FrameContextHandle> FrameContext::makeHandle(FrameInfo &) {
	return nullptr;
}

void FrameContext::submitHandle(FrameInfo &info, FrameContextHandle *handle) {
	submitMaterials(info);

	if (!handle->waitDependencies.empty()) {
		info.director->getApplication()->wakeup();
	}
}

core::MaterialId FrameContext::getMaterial(const MaterialInfo &info) const {
	auto hash = info.hash();
	auto it = _materials.find(hash);
	if (it != _materials.end()) {
		for (auto &m : it->second) {
			if (m.info == info) {
				return m.id;
			}
		}
	}
	return 0;
}

core::MaterialId FrameContext::acquireMaterial(const MaterialInfo &info, Vector<core::MaterialImage> &&images, Ref *data, bool revokable) {
	auto pipeline = getPipelineForMaterial(info);
	if (!pipeline) {
		return 0;
	}

	for (uint32_t idx = 0; idx < uint32_t(images.size()); ++ idx) {
		if (images[idx].image != nullptr) {
			auto &image = images[idx];
			image.info = getImageViewForMaterial(info, idx, images[idx].image);
			image.view = nullptr;
			image.sampler = info.samplers[idx];
		}
	}

	core::MaterialId newId = 0;
	if (revokable) {
		if (!_revokedIds.empty()) {
			newId = _revokedIds.back();
			_revokedIds.pop_back();
		}
	}

	if (newId == 0) {
		newId = _materialAttachment->getNextMaterialId();
	}

	if (auto m = Rc<core::Material>::create(newId, pipeline, move(images), data)) {
		auto id = m->getId();
		addPendingMaterial(move(m));
		addMaterial(info, id, revokable);
		return id;
	}
	return 0;
}

void FrameContext::readMaterials(core::MaterialAttachment *a) {
	_materialAttachment = a;

	auto renderPass = a->getLastRenderPass();
	while (renderPass) {
		auto &layouts = renderPass->pipelineLayouts;
		for (auto &layout : layouts) {
			bool isUsable = false;
			for (auto &set : layout->sets) {
				for (auto &desc : set->descriptors) {
					if (desc->attachment->attachment->attachment == a) {
						isUsable = true;
						break;
					}
				}
			}

			if (!isUsable) {
				break;
			}

			auto &sp = _layouts.emplace_back(PipelineLayoutData({layout}));

			for (auto &pipeline : layout->graphicPipelines) {
				auto hash = pipeline->material.hash();
				auto it = sp.pipelines.find(hash);
				if (it == sp.pipelines.end()) {
					it = sp.pipelines.emplace(hash, Vector<const core::GraphicPipelineData *>()).first;
				}
				it->second.emplace_back(pipeline);
			}
		}

		renderPass = a->getPrevRenderPass(renderPass);
	}
	for (auto &m : a->getPredefinedMaterials()) {
		addMaterial(getMaterialInfo(m), m->getId(), false);
	}
}

MaterialInfo FrameContext::getMaterialInfo(const Rc<core::Material> &material) const {
	MaterialInfo ret;

	size_t idx = 0;
	for (auto &it : material->getImages()) {
		if (idx < config::MaxMaterialImages) {
			ret.images[idx] = it.image->image->getIndex();
			ret.samplers[idx] = it.sampler;
			ret.colorModes[idx] = it.info.getColorMode();
		}
		++ idx;
	}

	ret.pipeline = material->getPipeline()->material;
	return ret;
}

void FrameContext::addPendingMaterial(Rc<core::Material> &&material) {
	_pendingMaterialsToAdd.emplace_back(move(material));
	if (!_materialDependency) {
		_materialDependency = Rc<core::DependencyEvent>::alloc(core::DependencyEvent::QueueSet{
			Rc<core::Queue>(_materialAttachment->getCompiler())
		});
	}
}

void FrameContext::addMaterial(const MaterialInfo &info, core::MaterialId id, bool revokable) {
	auto materialHash = info.hash();
	auto it = _materials.find(info.hash());
	if (it != _materials.end()) {
		it->second.emplace_back(ContextMaterialInfo{info, id, revokable});
	} else {
		Vector<ContextMaterialInfo> ids({ContextMaterialInfo{info, id, revokable}});
		_materials.emplace(materialHash, move(ids));
	}
}

String FrameContext::listMaterials() const {
	StringStream out;
	for (auto &it : _materials) {
		out << it.first << ":\n";
		for (auto &iit : it.second) {
			out << "\t" << iit.info.description() << " -> " << iit.id << "\n";
		}
	}
	return out.str();
}

core::ImageViewInfo FrameContext::getImageViewForMaterial(const MaterialInfo &info, uint32_t idx, const core::ImageData *image) const {
	return core::ImageViewInfo(image->format, info.colorModes[idx]);
}

auto FrameContext::getPipelineForMaterial(const MaterialInfo &info) const -> const core::GraphicPipelineData * {
	auto hash = info.pipeline.hash();
	for (auto &it : _layouts) {
		auto hashIt = it.pipelines.find(hash);
		if (hashIt != it.pipelines.end()) {
			for (auto &pipeline : hashIt->second) {
				if (pipeline->material == info.pipeline && isPipelineMatch(pipeline, info)) {
					return pipeline;
				}
			}
		}
	}
	log::warn("Scene", "No pipeline for attachment '", _materialAttachment->getName(), "': ",
			info.pipeline.description(), " : ", info.pipeline.data());
	return nullptr;
}

bool FrameContext::isPipelineMatch(const core::GraphicPipelineData *data, const MaterialInfo &info) const {
	return true; // TODO: true match
}

void FrameContext::submitMaterials(const FrameInfo &info) {
	// submit material updates
	if (!_pendingMaterialsToAdd.empty() || !_pendingMaterialsToRemove.empty()) {
		Vector<Rc<core::DependencyEvent>> events;
		if (_materialDependency) {
			events.emplace_back(_materialDependency);
		}

		if (!_pendingMaterialsToAdd.empty() || !_pendingMaterialsToRemove.empty()) {
			auto req = Rc<core::MaterialInputData>::alloc();
			req->attachment = _materialAttachment;
			req->materialsToAddOrUpdate = move(_pendingMaterialsToAdd);
			req->materialsToRemove = move(_pendingMaterialsToRemove);
			req->callback = [app = Rc<Application>(info.director->getApplication())] {
				app->wakeup();
			};

			for (auto &it : req->materialsToRemove) {
				emplace_ordered(_revokedIds, it);
			}

			info.director->getGlLoop()->compileMaterials(move(req), events);
		}

		_pendingMaterialsToAdd.clear();
		_pendingMaterialsToRemove.clear();
		_materialDependency = nullptr;
	}
}

void FrameContext::revokeImages(SpanView<uint64_t> vec) {
#ifdef COVERAGE
	listMaterials();
#endif
	auto shouldRevoke = [&, this] (const ContextMaterialInfo &iit) {
		for (auto &id : vec) {
			if (iit.info.hasImage(id)) {
				emplace_ordered(_pendingMaterialsToRemove, iit.id);
				return true;
			}
		}
		return false;
	};

	for (auto &it : _materials) {
		auto iit = it.second.begin();
		while (iit != it.second.end()) {
			if (iit->revokable) {
				if (shouldRevoke(*iit)) {
					iit = it.second.erase(iit);
				} else {
					++ iit;
				}
			} else {
				++ iit;
			}
		}
	}
}

}
