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

#include "XL2dVkVertexPass.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameCache.h"
#include "XLDirector.h"
#include "XLVkRenderPass.h"
#include "XLVkTextureSet.h"
#include "XLVkPipeline.h"
#include "XL2dLinearGradient.h"
#include "XL2dFrameContext.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

VertexAttachment::~VertexAttachment() { }

bool VertexAttachment::init(AttachmentBuilder &builder, const BufferInfo &info, const AttachmentData *m) {
	if (BufferAttachment::init(builder, info)) {
		_materials = m;
		return true;
	}
	return false;
}

auto VertexAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<VertexAttachmentHandle>::create(this, handle);
}

VertexAttachmentHandle::~VertexAttachmentHandle() { }

bool VertexAttachmentHandle::setup(FrameQueue &handle, Function<void(bool)> &&cb) {
	if (auto materials = handle.getAttachment((static_cast<VertexAttachment *>(_attachment.get()))->getMaterials())) {
		_materials = static_cast<const MaterialAttachmentHandle *>(materials->handle.get());
	}
	return true;
}

void VertexAttachmentHandle::submitInput(FrameQueue &q, Rc<core::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	auto d = data.cast<FrameContextHandle2d>();
	if (!d || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies, [this, d = move(d), cb = move(cb)] (FrameHandle &handle, bool success) {
		if (!success || !handle.isValidFlag()) {
			cb(false);
			return;
		}

		auto &cache = handle.getLoop()->getFrameCache();

		_materialSet = _materials->getSet();
		_drawStat.cachedFramebuffers = cache->getFramebuffersCount();
		_drawStat.cachedImages = cache->getImagesCount();
		_drawStat.cachedImageViews = cache->getImageViewsCount();

		handle.performInQueue([this, d = move(d)] (FrameHandle &handle) {
			return loadVertexes(handle, d);
		}, [cb = move(cb)] (FrameHandle &handle, bool success) {
			cb(success);
		}, this, "VertexMaterialAttachmentHandle::submitInput");
	});
}

bool VertexAttachmentHandle::empty() const {
	return !_indexes || !_vertexes || !_transforms;
}

const Rc<FrameContextHandle2d> &VertexAttachmentHandle::getCommands() const {
	return _commands;
}

struct VertexMaterialDrawPlan {
	struct PlanCommandInfo {
		const CmdGeneral *cmd = nullptr;
		SpanView<TransformVertexData> vertexes;
		Mat4 transform;
		const LinearGradientData *gradient = nullptr;
		uint32_t vertexOffset = 0;
		uint32_t vertexCount = 0;
		uint32_t gradientStart = 0;
		uint32_t gradientCount = 0;
	};

	struct PlanShadowInfo {
		const CmdShadow *cmd = nullptr;
		SpanView<TransformVertexData> vertexes;
		Mat4 transform;
	};

	template <typename T>
	struct WritePlanData {
		const core::Material *material = nullptr;
		Rc<core::DataAtlas> atlas;
		uint32_t vertexes = 0;
		uint32_t indexes = 0;
		uint32_t transforms = 0;
		Map<StateId, std::forward_list<T>> states;
	};

	using MaterialWritePlan = WritePlanData<PlanCommandInfo>;
	using ShadowWritePlan = WritePlanData<PlanShadowInfo>;

	struct WriteTarget {
		uint8_t *transform;
		uint8_t *vertexes;
		uint8_t *indexes;
	};

	Extent3 surfaceExtent;
	core::SurfaceTransformFlags transform = core::SurfaceTransformFlags::Identity;

	uint32_t excludeVertexes = 0;
	uint32_t excludeIndexes = 0;

	Map<SpanView<ZOrder>, float, ZOrderLess> paths;

	// fill write plan
	MaterialWritePlan globalWritePlan;

	// write plan for objects, that do depth-write and can be drawn out of order
	HashMap<core::MaterialId, MaterialWritePlan> solidWritePlan;

	// write plan for objects without depth-write, that can be drawn out of order
	HashMap<core::MaterialId, MaterialWritePlan> surfaceWritePlan;

	// write plan for transparent objects, that should be drawn in order
	Map<SpanView<ZOrder>, HashMap<core::MaterialId, MaterialWritePlan>, ZOrderLess> transparentWritePlan;

	std::forward_list<Vector<TransformVertexData>> deferredTmp;

	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
	uint32_t transtormOffset = 0;

	uint32_t transformIdx = 0;

	uint32_t solidCmds = 0;
	uint32_t surfaceCmds = 0;
	uint32_t transparentCmds = 0;
	uint32_t shadowsCmds = 0;

	Vec2 shadowSize = Vec2(1.0f, 1.0f);

	bool hasGpuSideAtlases = false;

	float maxShadowValue = 0.0f;

	FrameContextHandle2d *handle = nullptr;

	Vector<VertexSpan> materialSpans;
	Vector<VertexSpan> shadowSolidSpans;
	Vector<VertexSpan> shadowSdfSpans;

	VertexMaterialDrawPlan(const core::FrameContraints &constraints)
	: surfaceExtent{constraints.extent}, transform(constraints.transform) { }

	template <typename PlanData, typename Cmd>
	auto emplaceWritePlanData(WritePlanData<PlanData> &writePlan, const Command *c, const Cmd *cmd, SpanView<TransformVertexData> vertexes) -> PlanData & {
		for (auto &iit : vertexes) {
			globalWritePlan.vertexes += iit.data->data.size();
			globalWritePlan.indexes += iit.data->indexes.size();

			if (iit.sdfIndexes > 0) {
				globalWritePlan.indexes += (iit.sdfIndexes + iit.fillIndexes);
			}

			++ globalWritePlan.transforms;

			writePlan.vertexes += iit.data->data.size();
			writePlan.indexes += iit.data->indexes.size();
			++ writePlan.transforms;

			if ((c->flags & CommandFlags::DoNotCount) != CommandFlags::None) {
				excludeVertexes = iit.data->data.size();
				excludeIndexes = iit.data->indexes.size();
			}
		}

		auto iit = writePlan.states.find(cmd->state);
		if (iit == writePlan.states.end()) {
			iit = writePlan.states.emplace(cmd->state, std::forward_list<PlanData>()).first;
		}

		return iit->second.emplace_front(PlanData{cmd, vertexes});
	}

	void emplaceWritePlan(const core::Material *material, HashMap<core::MaterialId, MaterialWritePlan> &writePlan,
				const Command *c, const CmdGeneral *cmd, SpanView<TransformVertexData> vertexes) {
		auto it = writePlan.find(cmd->material);
		if (it == writePlan.end()) {
			if (material) {
				it = writePlan.emplace(cmd->material, MaterialWritePlan()).first;
				it->second.material = material;
				if (auto atlas = it->second.material->getAtlas()) {
					it->second.atlas = atlas;
				}
			}
		}

		if (it != writePlan.end() && it->second.material) {
			auto &plan = emplaceWritePlanData(it->second, c, cmd, vertexes);

			if (cmd->state != StateIdNone) {
				auto state = handle->getState(cmd->state);
				if (state) {
					auto stateData = dynamic_cast<StateData *>(state->data ? state->data.get() : nullptr);

					if (stateData && stateData->gradient) {
						plan.transform = stateData->transform;
						plan.gradient = stateData->gradient.get();
						globalWritePlan.vertexes += stateData->gradient->steps.size() + 2;
					}
				}
			}
		}

		auto pathsIt = paths.find(cmd->zPath);
		if (pathsIt == paths.end()) {
			paths.emplace(cmd->zPath, 0.0f);
		}
	}

	void pushVertexData(core::MaterialSet *materialSet, const Command *c, const CmdVertexArray *cmd) {
		auto material = materialSet->getMaterialById(cmd->material);
		if (!material) {
			return;
		}
		if (material->getPipeline()->isSolid()) {
			emplaceWritePlan(material, solidWritePlan, c, cmd, cmd->vertexes);
		} else if (cmd->renderingLevel == RenderingLevel::Surface) {
			emplaceWritePlan(material, surfaceWritePlan, c, cmd, cmd->vertexes);
		} else {
			auto v = transparentWritePlan.find(cmd->zPath);
			if (v == transparentWritePlan.end()) {
				v = transparentWritePlan.emplace(cmd->zPath, HashMap<core::MaterialId, MaterialWritePlan>()).first;
			}
			emplaceWritePlan(material, v->second, c, cmd, cmd->vertexes);
		}
	};

	template <typename Cmd>
	void applyNormalized(Vector<TransformVertexData> &vertexes, const Cmd *cmd) {
		// apply transforms;
		if (cmd->normalized) {
			for (auto &it : vertexes) {
				auto modelTransform = cmd->modelTransform * it.transform;

				Mat4 newMV;
				newMV.m[12] = std::floor(modelTransform.m[12]);
				newMV.m[13] = std::floor(modelTransform.m[13]);
				newMV.m[14] = std::floor(modelTransform.m[14]);

				const_cast<TransformVertexData &>(it).transform = cmd->viewTransform * newMV;
			}
		} else {
			for (auto &it : vertexes) {
				const_cast<TransformVertexData &>(it).transform = cmd->viewTransform * cmd->modelTransform * it.transform;
			}
		}
	}

	void pushDeferred(core::MaterialSet *materialSet, const Command *c, const CmdDeferred *cmd) {
		auto material = materialSet->getMaterialById(cmd->material);
		if (!material) {
			return;
		}

		if (!cmd->deferred->isWaitOnReady()) {
			if (!cmd->deferred->isReady()) {
				return;
			}
		}

		auto &vertexes = deferredTmp.emplace_front(cmd->deferred->getData().vec<Interface>());

		applyNormalized(vertexes, cmd);

		if (cmd->renderingLevel == RenderingLevel::Solid) {
			emplaceWritePlan(material, solidWritePlan, c, cmd, vertexes);
		} else if (cmd->renderingLevel == RenderingLevel::Surface) {
			emplaceWritePlan(material, surfaceWritePlan, c, cmd, vertexes);
		} else {
			auto v = transparentWritePlan.find(cmd->zPath);
			if (v == transparentWritePlan.end()) {
				v = transparentWritePlan.emplace(cmd->zPath, HashMap<core::MaterialId, MaterialWritePlan>()).first;
			}
			emplaceWritePlan(material, v->second, c, cmd, vertexes);
		}
	}

	void updatePathsDepth() {
		float depthScale = 1.0f / float(paths.size() + 1);
		float depthOffset = 1.0f - depthScale;
		for (auto &it : paths) {
			it.second = depthOffset;
			depthOffset -= depthScale;
		}
	}

	void pushInitial(WriteTarget &writeTarget) {
		TransformData val;
		memcpy(writeTarget.transform, &val, sizeof(TransformData));
		transtormOffset += sizeof(TransformData); ++ transformIdx;

		Vector<uint32_t> indexes{ 0, 2, 1, 0, 3, 2, 4, 6, 5, 4, 7, 6 };

		Vector<Vertex> vertexes {
			// full screen quad data
			Vertex{
				Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
				Vec4::ONE, Vec2::ZERO, 0, 0
			},
			Vertex{
				Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
				Vec4::ONE, Vec2::UNIT_Y, 0, 0
			},
			Vertex{
				Vec4(1.0f, 1.0f, 0.0f, 1.0f),
				Vec4::ONE, Vec2::ONE, 0, 0
			},
			Vertex{
				Vec4(1.0f, -1.0f, 0.0f, 1.0f),
				Vec4::ONE, Vec2::UNIT_X, 0, 0
			},

			// shadow quad data
			Vertex{
				Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
				Vec4::ONE, Vec2(0.0f, 1.0f - shadowSize.y), 0, 0
			},
			Vertex{
				Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
				Vec4::ONE, Vec2(0.0f, 1.0f), 0, 0
			},
			Vertex{
				Vec4(1.0f, 1.0f, 0.0f, 1.0f),
				Vec4::ONE, Vec2(shadowSize.x, 1.0f), 0, 0
			},
			Vertex{
				Vec4(1.0f, -1.0f, 0.0f, 1.0f),
				Vec4::ONE, Vec2(shadowSize.x, 1.0f - shadowSize.y), 0, 0
			}
		};

		switch (core::getPureTransform(transform)) {
		case core::SurfaceTransformFlags::Rotate90:
			vertexes[0].tex = Vec2::UNIT_Y;
			vertexes[1].tex = Vec2::ONE;
			vertexes[2].tex = Vec2::UNIT_X;
			vertexes[3].tex = Vec2::ZERO;
			vertexes[4].tex = Vec2(0.0f, shadowSize.y);
			vertexes[5].tex = shadowSize;
			vertexes[6].tex = Vec2(shadowSize.x, 0.0f);
			vertexes[7].tex = Vec2::ZERO;
			break;
		case core::SurfaceTransformFlags::Rotate180:
			vertexes[0].tex = Vec2::ONE;
			vertexes[1].tex = Vec2::UNIT_X;
			vertexes[2].tex = Vec2::ZERO;
			vertexes[3].tex = Vec2::UNIT_Y;
			vertexes[4].tex = shadowSize;
			vertexes[5].tex = Vec2(shadowSize.x, 0.0f);
			vertexes[6].tex = Vec2::ZERO;
			vertexes[7].tex = Vec2(0.0f, shadowSize.y);
			break;
		case core::SurfaceTransformFlags::Rotate270:
			vertexes[0].tex = Vec2::UNIT_X;
			vertexes[1].tex = Vec2::ZERO;
			vertexes[2].tex = Vec2::UNIT_Y;
			vertexes[3].tex = Vec2::ONE;
			vertexes[4].tex = Vec2(shadowSize.x, 0.0f);
			vertexes[5].tex = Vec2::ZERO;
			vertexes[6].tex = Vec2(0.0f, shadowSize.y);
			vertexes[7].tex = shadowSize;
			break;
		default:
			break;
		}

		auto target = reinterpret_cast<Vertex *>(writeTarget.vertexes) + vertexOffset;
		memcpy(target, vertexes.data(), vertexes.size() * sizeof(Vertex));
		memcpy(writeTarget.indexes, indexes.data(), indexes.size() * sizeof(uint32_t));

		vertexOffset += vertexes.size();
		indexOffset += indexes.size();
	}

	uint32_t rotateObject(uint32_t obj, uint32_t idx) {
		auto anchor = (obj >> 16) & 0x3;
		return (obj & ~0x30000) | (((anchor + idx) % 4) << 16);
	}

	Vec2 rotateVec(const Vec2 &vec) {
		switch (core::getPureTransform(transform)) {
			case core::SurfaceTransformFlags::Rotate90:
				return Vec2(-vec.y, vec.x);
				break;
			case core::SurfaceTransformFlags::Rotate180:
				return Vec2(-vec.x, -vec.y);
				break;
			case core::SurfaceTransformFlags::Rotate270:
				return Vec2(vec.y, -vec.x);
				break;
			default:
				break;
		}
		return vec;
	}

	void pushPlanVertexes(WriteTarget &writeTarget, HashMap<core::MaterialId, MaterialWritePlan> &writePlan) {
		auto pushVertexes = [&] (core::MaterialId materialId, const MaterialWritePlan &plan, const TransformData &transform, const TransformVertexData &vertexes) {
			auto target = reinterpret_cast<Vertex *>(writeTarget.vertexes) + vertexOffset;
			memcpy(target, vertexes.data->data.data(), vertexes.data->data.size() * sizeof(Vertex));
			memcpy(writeTarget.transform + transtormOffset, &transform, sizeof(TransformData));

			maxShadowValue = std::max(transform.shadow.x, maxShadowValue);

			size_t idx = 0;
			if (plan.atlas) {
				auto ext = plan.atlas->getImageExtent();
				float atlasScaleX = 1.0f / ext.width;
				float atlasScaleY =  1.0f / ext.height;

				for (; idx < vertexes.data->data.size(); ++ idx) {
					auto &t = target[idx];
					t.material = materialId | transformIdx << 16;

					if (!hasGpuSideAtlases) {
						if (auto d = reinterpret_cast<const DataAtlasValue *>(plan.atlas->getObjectByName(t.object))) {
							t.pos += Vec4(d->pos.x, d->pos.y, 0, 0);
							t.tex = d->tex;
							t.object = 0;
						} else {
#if DEBUG
							log::warn("VertexMaterialDrawPlan", "Object not found: ", t.object, " ", string::toUtf8<Interface>(char16_t(t.object)));
#endif
							auto anchor = font::CharId::getAnchorForChar(t.object);
							switch (anchor) {
							case font::CharAnchor::BottomLeft:
								t.tex = Vec2(1.0f - atlasScaleX, 0.0f);
								break;
							case font::CharAnchor::TopLeft:
								t.tex = Vec2(1.0f - atlasScaleX, 0.0f + atlasScaleY);
								break;
							case font::CharAnchor::TopRight:
								t.tex = Vec2(1.0f, 0.0f + atlasScaleY);
								break;
							case font::CharAnchor::BottomRight:
								t.tex = Vec2(1.0f, 0.0f);
								break;
							}
						}
					}
				}
			} else {
				for (; idx < vertexes.data->data.size(); ++ idx) {
					target[idx].material = materialId | transformIdx << 16;
				}
			}

			vertexOffset += vertexes.data->data.size();
			transtormOffset += sizeof(TransformData);
			++ transformIdx;
		};

		for (auto &plan : writePlan) {
			for (auto &state : plan.second.states) {
				for (auto &cmd : state.second) {
					cmd.vertexOffset = vertexOffset;
					for (auto &iit : cmd.vertexes) {
						TransformData transformData(iit.transform);

						auto pathIt = paths.find(cmd.cmd->zPath);
						if (pathIt != paths.end()) {
							transformData.offset.z = pathIt->second;
						}

						if (cmd.cmd->depthValue > 0.0f) {
							auto f16 = halffloat::encode(cmd.cmd->depthValue);
							auto value = halffloat::decode(f16);
							transformData.shadow = Vec4(value, value, value, 1.0);
						}

						pushVertexes(plan.first, plan.second, transformData, iit);
					}

					if (cmd.gradient) {
						auto target = reinterpret_cast<Vertex *>(writeTarget.vertexes) + vertexOffset;

						Vec2 start = cmd.transform * cmd.gradient->start;
						Vec2 end = cmd.transform * cmd.gradient->end;

						start.y = surfaceExtent.height - start.y;
						end.y = surfaceExtent.height - end.y;

						Vec2 norm = end - start;

						float d = norm.y * norm.y / (norm.x * norm.x + norm.y * norm.y);

						Vec2 axisAngle;
						if (std::abs(norm.y) > std::abs(norm.x)) {
							axisAngle.x = std::copysign(norm.length(), norm.y);
							//axisAngle.y = (norm.x * surfaceExtent.width) / (norm.y * surfaceExtent.height);
							axisAngle.y = d;
						} else {
							axisAngle.x = std::copysign(norm.length(), norm.x);
							//axisAngle.y = (norm.y * surfaceExtent.height) / (norm.x * surfaceExtent.width);
							axisAngle.y = d;
						}

						target->pos = Vec4(start, 0.0f, 0.0f);
						target->tex = axisAngle;
						++ target;

						target->pos = Vec4(end, 1.0f, 0.0f);
						target->tex = axisAngle;
						++ target;

						for (auto &it : cmd.gradient->steps) {
							target->pos = Vec4(math::lerp(start, end, it.value), it.value, it.factor);
							target->tex = axisAngle;
							target->color = Vec4(it.color.r, it.color.g, it.color.b, it.color.a);
							++ target;
						}

						cmd.gradientStart = vertexOffset;
						cmd.gradientCount = cmd.gradient->steps.size();

						vertexOffset += cmd.gradient->steps.size() + 2;
					}

					cmd.vertexCount = vertexOffset - cmd.vertexOffset;
				}
			}
		}
	}

	void drawWritePlan(WriteTarget &writeTarget, HashMap<core::MaterialId, MaterialWritePlan> &writePlan) {
		// optimize draw order, minimize switching pipeline, textureSet and descriptors
		Vector<const Pair<const core::MaterialId, MaterialWritePlan> *> drawOrder;

		for (auto &it : writePlan) {
			if (drawOrder.empty()) {
				drawOrder.emplace_back(&it);
			} else {
				auto lb = std::lower_bound(drawOrder.begin(), drawOrder.end(), &it,
						[] (const Pair<const core::MaterialId, MaterialWritePlan> *l, const Pair<const core::MaterialId, MaterialWritePlan> *r) {
					if (l->second.material->getPipeline() != l->second.material->getPipeline()) {
						return GraphicPipeline::comparePipelineOrdering(*l->second.material->getPipeline(), *r->second.material->getPipeline());
					} else if (l->second.material->getLayoutIndex() != r->second.material->getLayoutIndex()) {
						return l->second.material->getLayoutIndex() < r->second.material->getLayoutIndex();
					} else {
						return l->first < r->first;
					}
				});
				if (lb == drawOrder.end()) {
					drawOrder.emplace_back(&it);
				} else {
					drawOrder.emplace(lb, &it);
				}
			}
		}

		for (auto &it : drawOrder) {
			for (auto &state : it->second.states) {
				auto materialIndexes = indexOffset;
				uint32_t gradientStart = 0;
				uint32_t gradientCount = 0;

				for (auto &cmd : state.second) {
					for (auto &vertexes : cmd.vertexes) {
						auto indexTarget = reinterpret_cast<uint32_t *>(writeTarget.indexes) + indexOffset;

						if (vertexes.sdfIndexes > 0) {
							auto indexSource = vertexes.data->indexes.data();
							auto indexCount = vertexes.data->indexes.size() - vertexes.sdfIndexes;
							for (size_t i = 0; i < indexCount; ++ i) {
								*(indexTarget++) = indexSource[i] + cmd.vertexOffset;
							}

							indexOffset += indexCount;
						} else {
							for (auto &it : vertexes.data->indexes) {
								*(indexTarget++) = it + cmd.vertexOffset;
							}

							indexOffset += vertexes.data->indexes.size();
						}
					}

					if (cmd.gradient) {
						if (gradientCount != 0) {
							materialSpans.emplace_back(VertexSpan({ 0, indexOffset - materialIndexes, 1, materialIndexes, state.first,
								gradientStart, gradientCount}));
							materialIndexes = indexOffset;
						} else {
							gradientStart = cmd.gradientStart;
							gradientStart = cmd.gradientCount;
						}
					}
				}

				if (indexOffset > materialIndexes) {
					materialSpans.emplace_back(VertexSpan({ it->first, indexOffset - materialIndexes, 1, materialIndexes, state.first,
						gradientStart, gradientCount}));
				}
			}
		}

		for (auto &it : drawOrder) {
			for (auto &state : it->second.states) {
				auto materialIndexes = indexOffset;

				for (auto &cmd : state.second) {
					for (auto &vertexes : cmd.vertexes) {
						auto indexTarget = reinterpret_cast<uint32_t *>(writeTarget.indexes) + indexOffset;

						if (vertexes.sdfIndexes > 0 && vertexes.fillIndexes > 0) {
							auto indexSource = vertexes.data->indexes.data();
							auto indexCount = vertexes.fillIndexes;
							for (size_t i = 0; i < indexCount; ++ i) {
								*(indexTarget++) = indexSource[i] + cmd.vertexOffset;
							}

							indexOffset += indexCount;
						}
					}
				}

				if (indexOffset > materialIndexes) {
					shadowSolidSpans.emplace_back(VertexSpan({ 0, indexOffset - materialIndexes, 1, materialIndexes, state.first,
						0, 0}));
				}
			}
		}

		for (auto &it : drawOrder) {
			for (auto &state : it->second.states) {
				auto materialIndexes = indexOffset;

				for (auto &cmd : state.second) {
					for (auto &vertexes : cmd.vertexes) {
						auto indexTarget = reinterpret_cast<uint32_t *>(writeTarget.indexes) + indexOffset;

						if (vertexes.sdfIndexes > 0) {
							auto indexSource = vertexes.data->indexes.data() + vertexes.fillIndexes + vertexes.strokeIndexes;
							auto indexCount = vertexes.sdfIndexes;
							for (size_t i = 0; i < indexCount; ++ i) {
								*(indexTarget++) = indexSource[i] + cmd.vertexOffset;
							}

							indexOffset += indexCount;
						}
					}
				}

				if (indexOffset > materialIndexes) {
					shadowSdfSpans.emplace_back(VertexSpan({ 0, indexOffset - materialIndexes, 1, materialIndexes, state.first,
						0, 0}));
				}
			}
		}
	}

	void pushAll(WriteTarget &writeTarget) {
		pushInitial(writeTarget);
		pushPlanVertexes(writeTarget, solidWritePlan);
		pushPlanVertexes(writeTarget, surfaceWritePlan);
		for (auto &it : transparentWritePlan) {
			pushPlanVertexes(writeTarget, it.second);
		}

		uint32_t counter = 0;
		drawWritePlan(writeTarget, solidWritePlan);

		solidCmds = materialSpans.size() - counter;
		counter = materialSpans.size();

		drawWritePlan(writeTarget, surfaceWritePlan);

		surfaceCmds = materialSpans.size() - counter;
		counter = materialSpans.size();

		for (auto &it : transparentWritePlan) {
			drawWritePlan(writeTarget, it.second);
		}

		transparentCmds = materialSpans.size() - counter;
	}
};

bool VertexAttachmentHandle::loadVertexes(FrameHandle &fhandle, const Rc<FrameContextHandle2d> &commands) {
	auto handle = dynamic_cast<DeviceFrameHandle *>(&fhandle);
	if (!handle) {
		return false;
	}

	auto t = platform::clock();

	VertexMaterialDrawPlan plan(fhandle.getFrameConstraints());
	plan.hasGpuSideAtlases = handle->getAllocator()->getDevice()->hasDynamicIndexedBuffers();
	plan.handle = commands;

	auto shadowExtent = commands->lights.getShadowExtent( fhandle.getFrameConstraints().getScreenSize());
	auto shadowSize = commands->lights.getShadowSize( fhandle.getFrameConstraints().getScreenSize());

	plan.shadowSize = Vec2(shadowSize.width / float(shadowExtent.width), shadowSize.height / float(shadowExtent.height));

	auto cmd = commands->commands->getFirst();
	while (cmd) {
		switch (cmd->type) {
		case CommandType::CommandGroup:
			break;
		case CommandType::VertexArray:
			plan.pushVertexData(_materialSet.get(), cmd, reinterpret_cast<const CmdVertexArray *>(cmd->data));
			break;
		case CommandType::Deferred:
			plan.pushDeferred(_materialSet.get(), cmd, reinterpret_cast<const CmdDeferred *>(cmd->data));
			break;
		case CommandType::ShadowArray:
		case CommandType::ShadowDeferred:
		case CommandType::SdfGroup2D:
			break;
		}
		cmd = cmd->next;
	}

	if (plan.globalWritePlan.vertexes == 0 || plan.globalWritePlan.indexes == 0) {
		return true;
	}

	plan.updatePathsDepth();

	auto devFrame = static_cast<DeviceFrameHandle *>(handle);

	auto &pool = devFrame->getMemPool(this);

	// create buffers
	_indexes = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(core::BufferUsage::IndexBuffer, (plan.globalWritePlan.indexes + 12) * sizeof(uint32_t)));

	_vertexes = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(core::BufferUsage::StorageBuffer, (plan.globalWritePlan.vertexes + 8) * sizeof(Vertex)));

	_transforms = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(core::BufferUsage::StorageBuffer, (plan.globalWritePlan.transforms + 1) * sizeof(TransformData)));

	if (!_vertexes || !_indexes || !_transforms) {
		return false;
	}

	Bytes vertexData, indexData, transformData;

	VertexMaterialDrawPlan::WriteTarget writeTarget;

	if (fhandle.isPersistentMapping()) {
		writeTarget.vertexes = _vertexes->getPersistentMappedRegion();
		writeTarget.indexes = _indexes->getPersistentMappedRegion();
		writeTarget.transform = _transforms->getPersistentMappedRegion();
	} else {
		vertexData.resize(_vertexes->getSize());
		indexData.resize(_indexes->getSize());
		transformData.resize(_transforms->getSize());

		writeTarget.vertexes = vertexData.data();
		writeTarget.indexes = indexData.data();
		writeTarget.transform = transformData.data();
	}

	// write initial full screen quad
	plan.pushAll(writeTarget);

	_spans = move(plan.materialSpans);
	_shadowSolidSpans = move(plan.shadowSolidSpans);
	_shadowSdfSpans = move(plan.shadowSdfSpans);

	if (fhandle.isPersistentMapping()) {
		_vertexes->flushMappedRegion();
		_indexes->flushMappedRegion();
		_transforms->flushMappedRegion();
	} else {
		_vertexes->setData(vertexData);
		_indexes->setData(indexData);
		_transforms->setData(transformData);
	}

	_drawStat.vertexes = plan.globalWritePlan.vertexes - plan.excludeVertexes;
	_drawStat.triangles = (plan.globalWritePlan.indexes - plan.excludeIndexes) / 3;
	_drawStat.zPaths = plan.paths.size();
	_drawStat.drawCalls = _spans.size();
	_drawStat.materials = _materialSet->getMaterials().size();
	_drawStat.solidCmds = plan.solidCmds;
	_drawStat.surfaceCmds = plan.surfaceCmds;
	_drawStat.transparentCmds = plan.transparentCmds;
	_drawStat.shadowsCmds = plan.shadowsCmds;
	_drawStat.vertexInputTime = platform::clock() - t;

	commands->director->pushDrawStat(_drawStat);

	addBufferView(_vertexes);
	addBufferView(_transforms);

	_commands = commands;
	_maxShadowValue = plan.maxShadowValue;
	return true;
}

core::ImageFormat VertexPass::selectDepthFormat(SpanView<core::ImageFormat> formats) {
	core::ImageFormat ret = core::ImageFormat::Undefined;

	uint32_t score = 0;

	auto selectWithScore = [&] (core::ImageFormat fmt, uint32_t sc) {
		if (score < sc) {
			ret = fmt;
			score = sc;
		}
	};

	for (auto &it : formats) {
		switch (it) {
		case core::ImageFormat::D16_UNORM: selectWithScore(it, 12); break;
		case core::ImageFormat::X8_D24_UNORM_PACK32: selectWithScore(it, 7); break;
		case core::ImageFormat::D32_SFLOAT: selectWithScore(it, 9); break;
		case core::ImageFormat::S8_UINT: break;
		case core::ImageFormat::D16_UNORM_S8_UINT: selectWithScore(it, 11); break;
		case core::ImageFormat::D24_UNORM_S8_UINT: selectWithScore(it, 10); break;
		case core::ImageFormat::D32_SFLOAT_S8_UINT: selectWithScore(it, 8); break;
		default: break;
		}
	}

	return ret;
}

auto VertexPass::makeFrameHandle(const FrameQueue &handle) -> Rc<QueuePassHandle> {
	return Rc<VertexPassHandle>::create(*this, handle);
}

bool VertexPassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = static_cast<VertexPass *>(_queuePass.get());

	if (auto materialBuffer = q.getAttachment(pass->getMaterials())) {
		_materialBuffer = static_cast<const MaterialAttachmentHandle *>(materialBuffer->handle.get());
	}

	if (auto vertexBuffer = q.getAttachment(pass->getVertexes())) {
		_vertexBuffer = static_cast<const VertexAttachmentHandle *>(vertexBuffer->handle.get());
	}

	return QueuePassHandle::prepare(q, move(cb));
}

Vector<const CommandBuffer *> VertexPassHandle::doPrepareCommands(FrameHandle &handle) {
	auto buf = _pool->recordBuffer(*_device, [&, this] (CommandBuffer &buf) {
		auto materials = _materialBuffer->getSet().get();

		Vector<ImageMemoryBarrier> outputImageBarriers;
		Vector<BufferMemoryBarrier> outputBufferBarriers;

		doFinalizeTransfer(materials, outputImageBarriers, outputBufferBarriers);

		if (!outputBufferBarriers.empty() && !outputImageBarriers.empty()) {
			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
				outputBufferBarriers, outputImageBarriers);
		}

		prepareRenderPass(buf);

		_data->impl.cast<RenderPass>()->perform(*this, buf, [&, this] {
			prepareMaterialCommands(materials, buf);
		});

		finalizeRenderPass(buf);
		return true;
	});

	return Vector<const CommandBuffer *>{buf};
}

void VertexPassHandle::prepareRenderPass(CommandBuffer &) { }

void VertexPassHandle::prepareMaterialCommands(core::MaterialSet * materials, CommandBuffer &buf) {
	auto commands = _vertexBuffer->getCommands();
	auto pass = static_cast<RenderPass *>(_data->impl.get());

	if (_vertexBuffer->empty() || !_vertexBuffer->getIndexes() || !_vertexBuffer->getVertexes()) {
		return;
	}

	// bind global indexes
	buf.cmdBindIndexBuffer(_vertexBuffer->getIndexes(), 0, VK_INDEX_TYPE_UINT32);

	clearDynamicState(buf);

	// Use commented code to debug drawing command-by-command

	/*static size_t ctrl = 0;

	size_t i = 0;
	size_t min = 0;
	size_t max = (ctrl ++) % 12;*/

	uint32_t boundTextureSetIndex = maxOf<uint32_t>();

	struct MaterialData {
		uint32_t materialIdx = 0;
		uint32_t imageIdx = 0;
		uint32_t samplerIdx = 0;
		uint32_t gradientOffset = 0;
		uint32_t gradientCount = 0;
	};

	MaterialData data;

	for (auto &materialVertexSpan : _vertexBuffer->getVertexData()) {
		/*if (i < min) {
			++ i;
			continue;
		}
		if (i >= max) {
			break;
		}*/

		//++ i;
		data.materialIdx = materials->getMaterialOrder(materialVertexSpan.material);
		const core::Material * material = materials->getMaterialById(materialVertexSpan.material);
		if (!material) {
			continue;
		}
		data.imageIdx = material->getImages().front().descriptor;
		data.samplerIdx = material->getImages().front().sampler;
		data.gradientOffset = materialVertexSpan.gradientOffset;
		data.gradientCount = materialVertexSpan.gradientCount;

		auto pipeline = material->getPipeline();
		auto textureSetIndex =  material->getLayoutIndex();

		buf.cmdBindPipelineWithDescriptors(pipeline);

		if (textureSetIndex != boundTextureSetIndex) {
			auto l = materials->getLayout(textureSetIndex);
			if (l && l->set) {
				auto s = static_cast<TextureSet *>(l->set.get());
				auto set = s->getSet();

				// rebind texture set at last index
				buf.cmdBindDescriptorSets(static_cast<RenderPass *>(_data->impl.get()), makeSpanView(&set, 1), 1);
				boundTextureSetIndex = textureSetIndex;
			} else {
				stappler::log::error("MaterialRenderPassHandle", "Invalid textureSetlayout: ", textureSetIndex);
				return;
			}
		}

		applyDynamicState(commands, buf, materialVertexSpan.state);

		buf.cmdPushConstants(pass->getPipelineLayout(0),
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				BytesView(reinterpret_cast<const uint8_t *>(&data), sizeof(MaterialData)));

		buf.cmdDrawIndexed(
			materialVertexSpan.indexCount, // indexCount
			materialVertexSpan.instanceCount, // instanceCount
			materialVertexSpan.firstIndex, // firstIndex
			0, // int32_t   vertexOffset
			0  // uint32_t  firstInstance
		);
	}
}

void VertexPassHandle::finalizeRenderPass(CommandBuffer &) { }

void VertexPassHandle::clearDynamicState(CommandBuffer &buf) {
	auto currentExtent = getFramebuffer()->getExtent();

	VkViewport viewport{ 0.0f, 0.0f, float(currentExtent.width), float(currentExtent.height), 0.0f, 1.0f };
	buf.cmdSetViewport(0, makeSpanView(&viewport, 1));

	VkRect2D scissorRect{ { 0, 0}, { currentExtent.width, currentExtent.height } };
	buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));

	_dynamicStateId = maxOf<StateId>();
	_dynamicState = DrawStateValues();
}

void VertexPassHandle::applyDynamicState(const FrameContextHandle2d *commands, CommandBuffer &buf, uint32_t stateId) {
	if (stateId == _dynamicStateId) {
		return;
	}

	auto currentExtent = getFramebuffer()->getExtent();
	auto state = commands->getState(stateId);
	//log::verbose("VertexPassHandle", (void *)this, " enable state: ", stateId, " ", (void *)state);
	if (!state) {
		if (_dynamicState.isScissorEnabled()) {
			_dynamicState.enabled &= ~(core::DynamicState::Scissor);
			VkRect2D scissorRect{ { 0, 0}, { currentExtent.width, currentExtent.height } };
			buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));
		}
	} else {
		if (state->isScissorEnabled()) {
			if (_dynamicState.isScissorEnabled()) {
				if (_dynamicState.scissor != state->scissor) {
					auto scissorRect = rotateScissor(_constraints, state->scissor);
					buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));
					_dynamicState.scissor = state->scissor;
				}
			} else {
				_dynamicState.enabled |= core::DynamicState::Scissor;
				auto scissorRect = rotateScissor(_constraints, state->scissor);
				buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));
				_dynamicState.scissor = state->scissor;
			}
		} else {
			if (_dynamicState.isScissorEnabled()) {
				_dynamicState.enabled &= ~(core::DynamicState::Scissor);
				VkRect2D scissorRect{ { 0, 0}, { currentExtent.width, currentExtent.height } };
				buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));
			}
		}
	}

	_dynamicStateId = stateId;
}

}
