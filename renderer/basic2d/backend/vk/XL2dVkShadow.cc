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

#include "XL2dVkShadow.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameCache.h"
#include "XLCoreFrameRequest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

ShadowLightDataAttachmentHandle::~ShadowLightDataAttachmentHandle() { }

void ShadowLightDataAttachmentHandle::submitInput(FrameQueue &q, Rc<core::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	auto d = data.cast<FrameContextHandle2d>();
	if (!d || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies, [this, d = move(d), cb = move(cb)] (FrameHandle &handle, bool success) mutable {
		if (!success || !handle.isValidFlag()) {
			cb(false);
			return;
		}

		auto devFrame = static_cast<DeviceFrameHandle *>(&handle);

		_input = move(d);
		_data = devFrame->getMemPool(devFrame)->spawn(AllocationUsage::DeviceLocalHostVisible,
					BufferInfo(static_cast<BufferAttachment *>(_attachment.get())->getInfo(), sizeof(ShadowData)));
		cb(true);
	});
}

bool ShadowLightDataAttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
		uint32_t, bool isExternal) const {
	if (_data) {
		return true;
	}
	return false;
}

bool ShadowLightDataAttachmentHandle::writeDescriptor(const core::QueuePassHandle &, DescriptorBufferInfo &info) {
	switch (info.index) {
	case 0:
		info.buffer = _data;
		info.offset = 0;
		info.range = _data->getSize();
		return true;
		break;
	default:
		break;
	}
	return false;
}

void ShadowLightDataAttachmentHandle::allocateBuffer(DeviceFrameHandle *devFrame, const ShadowVertexAttachmentHandle *vertexes, uint32_t gridSize) {
	_data->map([&] (uint8_t *buf, VkDeviceSize size) {
		ShadowData *data = reinterpret_cast<ShadowData *>(buf);

		if (isnan(_input->lights.luminosity)) {
			float l = _input->lights.globalColor.a;
			for (uint32_t i = 0; i < _input->lights.ambientLightCount; ++ i) {
				l += _input->lights.ambientLights[i].color.a;
			}
			for (uint32_t i = 0; i < _input->lights.directLightCount; ++ i) {
				l += _input->lights.directLights[i].color.a;
			}
			_shadowData.luminosity = data->luminosity = 1.0f / l;
		} else {
			_shadowData.luminosity = data->luminosity = 1.0f / _input->lights.luminosity;
		}

		float fullDensity = _input->lights.sceneDensity;
		auto screenSize = devFrame->getFrameConstraints().getScreenSize();

		Extent2 scaledExtent(ceilf(screenSize.width / fullDensity), ceilf(screenSize.height / fullDensity));

		//float shadowDensity = _input->lights.sceneDensity / _input->lights.shadowDensity;
		//Extent2 shadowExtent(ceilf(screenSize.width / shadowDensity), ceilf(screenSize.height / shadowDensity));
		//Vec2 shadowOffset( screenSize.width / shadowDensity - shadowExtent.width, screenSize.height / shadowDensity - shadowExtent.height);
		//shadowOffset *= shadowDensity;
		_shadowData.globalColor = data->globalColor = _input->lights.globalColor * data->luminosity;

		// pre-calculated color with no shadows
		Color4F discardColor = _shadowData.globalColor;
		for (uint32_t i = 0; i < _input->lights.ambientLightCount; ++ i) {
			auto a = _input->lights.ambientLights[i].color.a * data->luminosity;
			auto ncolor = (_input->lights.ambientLights[i].color * _input->lights.ambientLights[i].color.a) * data->luminosity;
			ncolor.a = a;
			discardColor = discardColor + ncolor;
		}
		discardColor.a = 1.0f;
		_shadowData.discardColor = data->discardColor = discardColor;

		_shadowData.gridSize = data->gridSize = ceilf(gridSize / fullDensity);
		_shadowData.gridWidth = data->gridWidth = (scaledExtent.width - 1) / data->gridSize + 1;
		_shadowData.gridHeight = data->gridHeight = (scaledExtent.height - 1) / data->gridSize + 1;
		_shadowData.objectsCount = data->objectsCount = vertexes->getTrianglesCount() + vertexes->getCirclesCount()
				+ vertexes->getRectsCount() + vertexes->getRoundedRectsCount() + vertexes->getPolygonsCount();

		_shadowData.trianglesFirst = data->trianglesFirst = 0;
		_shadowData.trianglesCount = data->trianglesCount = vertexes->getTrianglesCount();

		_shadowData.circlesFirst = data->circlesFirst = _shadowData.trianglesFirst + _shadowData.trianglesCount;
		_shadowData.circlesCount = data->circlesCount = vertexes->getCirclesCount();

		_shadowData.rectsFirst = data->rectsFirst = _shadowData.circlesFirst + _shadowData.circlesCount;
		_shadowData.rectsCount = data->rectsCount = vertexes->getRectsCount();

		_shadowData.roundedRectsFirst = data->roundedRectsFirst = _shadowData.rectsFirst + _shadowData.rectsCount;
		_shadowData.roundedRectsCount = data->roundedRectsCount = vertexes->getRoundedRectsCount();

		_shadowData.polygonsFirst = data->polygonsFirst = _shadowData.roundedRectsFirst + _shadowData.roundedRectsCount;
		_shadowData.polygonsCount = data->polygonsCount = vertexes->getPolygonsCount();

		_shadowData.ambientLightCount = data->ambientLightCount = _input->lights.ambientLightCount;
		_shadowData.directLightCount = data->directLightCount = _input->lights.directLightCount;
		_shadowData.maxValue = data->maxValue = vertexes->getMaxValue();
		_shadowData.bbOffset = data->bbOffset = getBoxOffset(_shadowData.maxValue);
		_shadowData.density = data->density = _input->lights.sceneDensity;
		_shadowData.shadowSdfDensity = data->shadowSdfDensity = 1.0f / _input->lights.shadowDensity;
		_shadowData.shadowDensity = data->shadowDensity = 1.0f / _input->lights.sceneDensity;
		_shadowData.pix = data->pix = Vec2(1.0f / float(screenSize.width), 1.0f / float(screenSize.height));

		memcpy(data->ambientLights, _input->lights.ambientLights, sizeof(AmbientLightData) * config::MaxAmbientLights);
		memcpy(data->directLights, _input->lights.directLights, sizeof(DirectLightData) * config::MaxDirectLights);
		memcpy(_shadowData.ambientLights, _input->lights.ambientLights, sizeof(AmbientLightData) * config::MaxAmbientLights);
		memcpy(_shadowData.directLights, _input->lights.directLights, sizeof(DirectLightData) * config::MaxDirectLights);
	});
}

float ShadowLightDataAttachmentHandle::getBoxOffset(float value) const {
	value = std::max(value, 2.0f);
	float bbox = 0.0f;
	for (size_t i = 0; i < _input->lights.ambientLightCount; ++ i) {
		auto &l = _input->lights.ambientLights[i];
		float n_2 = l.normal.x * l.normal.x + l.normal.y * l.normal.y;
		float m = std::sqrt(n_2) / std::sqrt(1 - n_2);

		bbox = std::max((m * value * 2.0f) + (std::ceil(l.normal.w * value)), bbox);
	}
	return bbox;
}

uint32_t ShadowLightDataAttachmentHandle::getLightsCount() const {
	return _input->lights.ambientLightCount + _input->lights.directLightCount;
}

uint32_t ShadowLightDataAttachmentHandle::getObjectsCount() const {
	return _shadowData.objectsCount;
}

ShadowVertexAttachmentHandle::~ShadowVertexAttachmentHandle() { }

void ShadowVertexAttachmentHandle::submitInput(FrameQueue &q, Rc<core::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
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

		handle.performInQueue([this, d = move(d)] (FrameHandle &handle) {
			return loadVertexes(handle, d);
		}, [cb = move(cb)] (FrameHandle &handle, bool success) {
			cb(success);
		}, this, "VertexMaterialAttachmentHandle::submitInput");
	});
}

bool ShadowVertexAttachmentHandle::empty() const {
	return !_indexes || !_vertexes || !_transforms || !_circles || !_rects || !_roundedRects || !_polygons;
}

struct ShadowDrawPlan {
	struct PlanCommandInfo {
		const CmdShadow *cmd;
		SpanView<TransformVertexData> vertexes;
	};

	struct PladSdfCommand {
		const CmdSdfGroup2D *cmd;
		uint32_t triangles = 0;
		uint32_t objects = 0;
	};

	ShadowDrawPlan(core::FrameHandle &fhandle) : pool(fhandle.getPool()->getPool()) {

	}

	void emplaceWritePlan(const Command *c, const CmdShadow *cmd, SpanView<TransformVertexData> v) {
		for (auto &iit : v) {
			vertexes += iit.data->data.size();
			indexes += iit.data->indexes.size();
			++ transforms;
		}

		commands.emplace_front(PlanCommandInfo{cmd, v});
	}

	void pushDeferred(const Command *c, const CmdShadowDeferred *cmd) {
		if (!cmd->deferred->isWaitOnReady()) {
			if (!cmd->deferred->isReady()) {
				return;
			}
		}

		auto vertexes = cmd->deferred->getData().pdup(pool);

		// apply transforms;
		if (cmd->normalized) {
			for (auto &it : vertexes) {
				auto modelTransform = cmd->modelTransform * it.transform;

				Mat4 newMV;
				newMV.m[12] = floorf(modelTransform.m[12]);
				newMV.m[13] = floorf(modelTransform.m[13]);
				newMV.m[14] = floorf(modelTransform.m[14]);

				const_cast<TransformVertexData &>(it).transform = newMV;
			}
		} else {
			for (auto &it : vertexes) {
				const_cast<TransformVertexData &>(it).transform = cmd->modelTransform * it.transform;
			}
		}

		emplaceWritePlan(c, cmd, vertexes);
	}

	void pushSdf(const Command *c, const CmdSdfGroup2D *cmd) {
		uint32_t objects = 0;
		uint32_t triangles = 0;
		for (auto &it : cmd->data) {
			switch (it.type) {
			case SdfShape::Circle2D: ++ circles; ++ objects; ++ vertexes; break;
			case SdfShape::Rect2D: ++ rects; ++ objects; ++ vertexes; break;
			case SdfShape::RoundedRect2D: ++ roundedRects; ++ objects; vertexes += 2; break;
			case SdfShape::Triangle2D: vertexes += 3; indexes += 3; ++ triangles; break;
			case SdfShape::Polygon2D: {
				auto data = reinterpret_cast<const SdfPolygon2D *>(it.bytes.data());
				vertexes += data->points.size(); ++ triangles;
				++ polygons;
				break;
			}
			default: break;
			}
		}
		if (objects > 0 || triangles > 0) {
			if (objects > 0) {
				++ transforms;
			}
			if (triangles > 0) {
				++ transforms;
			}
			sdfCommands.emplace_front(PladSdfCommand{cmd, triangles, objects});
		}
	}

	memory::pool_t *pool = nullptr;
	uint32_t vertexes = 0;
	uint32_t indexes = 0;
	uint32_t transforms = 0;
	uint32_t circles = 0;
	uint32_t rects = 0;
	uint32_t roundedRects = 0;
	uint32_t polygons = 0;
	std::forward_list<PlanCommandInfo> commands;
	std::forward_list<PladSdfCommand> sdfCommands;
};

struct ShadowBufferMap {
	uint8_t *region;
	Bytes external;
	Buffer *buffer;
	bool isPersistent = false;

	~ShadowBufferMap() {
		if (isPersistent) {
			buffer->flushMappedRegion();
		} else {
			buffer->setData(external);
		}
	}

	ShadowBufferMap(Buffer *b, bool persistent) : buffer(b), isPersistent(persistent) {
		if (isPersistent) {
			region = buffer->getPersistentMappedRegion();
		} else {
			external.resize(buffer->getSize());
			region = external.data();
		}
	}
};

bool ShadowVertexAttachmentHandle::loadVertexes(FrameHandle &fhandle, const Rc<FrameContextHandle2d> &commands) {
	auto handle = dynamic_cast<DeviceFrameHandle *>(&fhandle);
	if (!handle) {
		return false;
	}

	// fill write plan

	ShadowDrawPlan plan(fhandle);

	auto cmd = commands->shadows->getFirst();
	while (cmd) {
		switch (cmd->type) {
		case CommandType::CommandGroup:
		case CommandType::VertexArray:
		case CommandType::Deferred:
			break;
		case CommandType::ShadowArray:
			plan.emplaceWritePlan(cmd, reinterpret_cast<const CmdShadowArray *>(cmd->data),
					reinterpret_cast<const CmdShadowArray *>(cmd->data)->vertexes);
			break;
		case CommandType::ShadowDeferred:
			plan.pushDeferred(cmd, reinterpret_cast<const CmdShadowDeferred *>(cmd->data));
			break;
		case CommandType::SdfGroup2D:
			plan.pushSdf(cmd, reinterpret_cast<const CmdSdfGroup2D *>(cmd->data));
			break;
		}
		cmd = cmd->next;
	}

	if (plan.vertexes == 0 && plan.indexes == 0 && plan.circles == 0) {
		return true;
	}

	auto &info = reinterpret_cast<BufferAttachment *>(_attachment.get())->getInfo();

	// create buffers
	auto &pool = handle->getMemPool(handle);
	_indexes = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(info, std::max((plan.indexes / 3), uint32_t(1)) * sizeof(Triangle2DIndex)));

	_vertexes = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(info, std::max(plan.vertexes, uint32_t(1)) * sizeof(Vec4)));

	_transforms = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(info, std::max((plan.transforms + 1), uint32_t(1)) * sizeof(TransformData)));

	_circles = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(info, std::max(plan.circles, uint32_t(1)) * sizeof(Circle2DIndex)));

	_rects = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(info, std::max(plan.rects, uint32_t(1)) * sizeof(Rect2DIndex)));

	_roundedRects = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(info, std::max(plan.roundedRects, uint32_t(1)) * sizeof(RoundedRect2DIndex)));

	_polygons = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(info, std::max(plan.polygons, uint32_t(1)) * sizeof(Polygon2DIndex)));

	if (!_vertexes || !_indexes || !_transforms || !_circles || !_rects || !_roundedRects || !_polygons) {
		return false;
	}

	ShadowBufferMap vertexesMap(_vertexes, fhandle.isPersistentMapping());
	ShadowBufferMap indexesMap(_indexes, fhandle.isPersistentMapping());
	ShadowBufferMap transformMap(_transforms, fhandle.isPersistentMapping());
	ShadowBufferMap circlesMap(_circles, fhandle.isPersistentMapping());
	ShadowBufferMap rectsMap(_rects, fhandle.isPersistentMapping());
	ShadowBufferMap roundedRectsMap(_roundedRects, fhandle.isPersistentMapping());
	ShadowBufferMap polygonsMap(_polygons, fhandle.isPersistentMapping());

	TransformData val;
	memcpy(transformMap.region, &val, sizeof(TransformData));

	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
	uint32_t circleOffset = 0;
	uint32_t rectOffset = 0;
	uint32_t roundedRectOffset = 0;
	uint32_t polygonsOffset = 0;

	uint32_t materialVertexes = 0;
	uint32_t materialIndexes = 0;
	uint32_t transformIdx = 1;

	auto pushVertexes = [&] (const CmdShadow *cmd, const TransformData &transform, VertexData *vertexes) {
		auto target = reinterpret_cast<Vec4 *>(vertexesMap.region) + vertexOffset;

		memcpy(transformMap.region + sizeof(TransformData) * transformIdx, &transform, sizeof(TransformData));

		for (size_t idx = 0; idx < vertexes->data.size(); ++ idx) {
			memcpy(target++, &vertexes->data[idx].pos, sizeof(Vec4));
		}

		auto indexTarget = reinterpret_cast<Triangle2DIndex *>(indexesMap.region) + indexOffset;

		for (size_t idx = 0; idx < vertexes->indexes.size() / 3; ++ idx) {
			Triangle2DIndex data({
				vertexes->indexes[idx * 3 + 0] + vertexOffset,
				vertexes->indexes[idx * 3 + 1] + vertexOffset,
				vertexes->indexes[idx * 3 + 2] + vertexOffset,
				transformIdx,
				cmd->value,
				1.0f
			});
			memcpy(indexTarget++, &data, sizeof(Triangle2DIndex));

			_maxValue = std::max(_maxValue, cmd->value);
		}

		vertexOffset += vertexes->data.size();
		indexOffset += vertexes->indexes.size() / 3;
		++ transformIdx;

		materialVertexes += vertexes->data.size();
		materialIndexes += vertexes->indexes.size();
	};

	for (auto &cmd : plan.commands) {
		for (auto &iit : cmd.vertexes) {
			pushVertexes(cmd.cmd, iit.transform, iit.data);
		}
	}

	auto pushSdf = [&] (const CmdSdfGroup2D *cmd, uint32_t triangles, uint32_t objects) {
		auto target = reinterpret_cast<Vec4 *>(vertexesMap.region) + vertexOffset;

		uint32_t transformTriangles = 0;
		uint32_t transformObjects = 0;

		if (triangles > 0) {
			TransformData transform(cmd->modelTransform);
			memcpy(transformMap.region + sizeof(TransformData) * transformIdx, &transform, sizeof(TransformData));
			transformTriangles = transformIdx;
			++ transformIdx;
		}

		if (objects > 0) {
			TransformData transform(cmd->modelTransform.getInversed());
			cmd->modelTransform.getScale(&transform.padding.x);
			memcpy(transformMap.region + sizeof(TransformData) * transformIdx, &transform, sizeof(TransformData));
			transformObjects = transformIdx;
			++ transformIdx;
		}

		for (auto &it : cmd->data) {
			switch (it.type) {
			case SdfShape::Circle2D: {
				auto data = reinterpret_cast<const SdfCircle2D *>(it.bytes.data());
				auto pos = Vec4(data->origin, 0.0f, data->radius);

				memcpy(target++, &pos, sizeof(Vec4));

				Circle2DIndex index;
				index.origin = vertexOffset;
				index.transform = transformObjects;
				index.value = cmd->value;
				index.opacity = cmd->opacity;

				memcpy(reinterpret_cast<Circle2DIndex *>(circlesMap.region) + circleOffset, &index, sizeof(Circle2DIndex));

				++ circleOffset;
				++ vertexOffset;
				break;
			}
			case SdfShape::Rect2D: {
				auto data = reinterpret_cast<const SdfRect2D *>(it.bytes.data());
				auto pos = Vec4(data->origin, data->size);

				memcpy(target++, &pos, sizeof(Vec4));

				Rect2DIndex index;
				index.origin = vertexOffset;
				index.transform = transformObjects;
				index.value = cmd->value;
				index.opacity = cmd->opacity;

				memcpy(reinterpret_cast<Rect2DIndex *>(rectsMap.region) + rectOffset, &index, sizeof(Rect2DIndex));

				++ rectOffset;
				++ vertexOffset;
				break;
			}
			case SdfShape::RoundedRect2D: {
				auto data = reinterpret_cast<const SdfRoundedRect2D *>(it.bytes.data());
				auto pos = Vec4(data->origin, data->size);

				memcpy(target++, &pos, sizeof(Vec4));
				memcpy(target++, &data->radius, sizeof(Vec4));

				RoundedRect2DIndex index;
				index.origin = vertexOffset;
				index.transform = transformObjects;
				index.value = cmd->value;
				index.opacity = cmd->opacity;

				memcpy(reinterpret_cast<RoundedRect2DIndex *>(roundedRectsMap.region) + roundedRectOffset, &index, sizeof(RoundedRect2DIndex));

				++ roundedRectOffset;
				vertexOffset += 2;
				break;
			}
			case SdfShape::Triangle2D: {
				auto data = reinterpret_cast<const SdfTriangle2D *>(it.bytes.data());
				Vec4 vertexes[3] = {
					Vec4(data->origin + data->a, 0.0f, 1.0f),
					Vec4(data->origin + data->b, 0.0f, 1.0f),
					Vec4(data->origin + data->c, 0.0f, 1.0f),
				};
				memcpy(target, vertexes, sizeof(Vec4) * 3);
				target += 3;

				Triangle2DIndex triangle({
					vertexOffset,
					vertexOffset + 1,
					vertexOffset + 2,
					transformTriangles,
					cmd->value,
					cmd->opacity
				});

				memcpy(reinterpret_cast<Triangle2DIndex *>(indexesMap.region) + indexOffset, &triangle, sizeof(Triangle2DIndex));

				++ indexOffset;
				vertexOffset += 3;
				break;
			}
			case SdfShape::Polygon2D: {
				auto data = reinterpret_cast<const SdfPolygon2D *>(it.bytes.data());
				for (auto &it : data->points) {
					Vec4 pt(it, 0, 1);
					memcpy(target++, &pt, sizeof(Vec4));
				}

				Polygon2DIndex polygon({
					vertexOffset,
					uint32_t(data->points.size()),
					transformTriangles,
					uint32_t(0),
					cmd->value,
					cmd->opacity
				});

				memcpy(reinterpret_cast<Polygon2DIndex *>(polygonsMap.region) + polygonsOffset, &polygon, sizeof(Polygon2DIndex));

				vertexOffset += data->points.size();
				++ polygonsOffset;
				break;
			}
			default: break;
			}
		}

		_maxValue = std::max(_maxValue, cmd->value);
	};

	for (auto &cmd : plan.sdfCommands) {
		pushSdf(cmd.cmd, cmd.triangles, cmd.objects);
	}

	_trianglesCount = plan.indexes / 3;
	_circlesCount = plan.circles;
	_rectsCount = plan.rects;
	_roundedRectsCount = plan.roundedRects;
	_polygonsCount = plan.polygons;

	addBufferView(_indexes);
	addBufferView(_vertexes);
	addBufferView(_transforms);
	addBufferView(_circles);
	addBufferView(_rects);
	addBufferView(_roundedRects);
	addBufferView(_polygons);

	return true;
}

ShadowPrimitivesAttachmentHandle::~ShadowPrimitivesAttachmentHandle() { }

void ShadowPrimitivesAttachmentHandle::allocateBuffer(DeviceFrameHandle *devFrame, const ShadowData &data) {
	auto &pool = devFrame->getMemPool(devFrame);

	_objects = pool->spawn(AllocationUsage::DeviceLocal, BufferInfo(core::BufferUsage::StorageBuffer,
			std::max(uint32_t(1), data.objectsCount) * sizeof(Sdf2DObjectData)));
	_gridSize = pool->spawn(AllocationUsage::DeviceLocal, BufferInfo(core::BufferUsage::StorageBuffer,
			data.gridWidth * data.gridHeight * sizeof(uint32_t)));
	_gridIndex = pool->spawn(AllocationUsage::DeviceLocal, BufferInfo(core::BufferUsage::StorageBuffer,
			std::max(uint32_t(1), data.objectsCount) * data.gridWidth * data.gridHeight * sizeof(uint32_t)));

	addBufferView(_objects);
	addBufferView(_gridSize);
	addBufferView(_gridIndex);
}


ShadowLightDataAttachment::~ShadowLightDataAttachment() { }

bool ShadowLightDataAttachment::init(AttachmentBuilder &builder) {
	if (BufferAttachment::init(builder, BufferInfo(core::BufferUsage::UniformBuffer, size_t(sizeof(ShadowData))))) {
		return true;
	}
	return false;
}

bool ShadowLightDataAttachment::validateInput(const Rc<core::AttachmentInputData> &data) const {
	if (dynamic_cast<FrameContextHandle2d *>(data.get())) {
		return true;
	}
	return false;
}

auto ShadowLightDataAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<ShadowLightDataAttachmentHandle>::create(this, handle);
}

ShadowVertexAttachment::~ShadowVertexAttachment() { }

bool ShadowVertexAttachment::init(AttachmentBuilder &builder) {
	if (BufferAttachment::init(builder, BufferInfo(core::BufferUsage::StorageBuffer))) {
		return true;
	}
	return false;
}

bool ShadowVertexAttachment::validateInput(const Rc<core::AttachmentInputData> &data) const {
	if (dynamic_cast<FrameContextHandle2d *>(data.get())) {
		return true;
	}
	return false;
}

auto ShadowVertexAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<ShadowVertexAttachmentHandle>::create(this, handle);
}

ShadowPrimitivesAttachment::~ShadowPrimitivesAttachment() { }

bool ShadowPrimitivesAttachment::init(AttachmentBuilder &builder) {
	if (BufferAttachment::init(builder, BufferInfo(core::BufferUsage::StorageBuffer))) {
		return true;
	}
	return false;
}

auto ShadowPrimitivesAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<ShadowPrimitivesAttachmentHandle>::create(this, handle);
}

ShadowSdfImageAttachment::~ShadowSdfImageAttachment() { }

bool ShadowSdfImageAttachment::init(AttachmentBuilder &builder, Extent2 extent) {
	return ImageAttachment::init(builder, ImageInfo(
		extent,
		core::ForceImageUsage(core::ImageUsage::Storage | core::ImageUsage::Sampled | core::ImageUsage::TransferDst | core::ImageUsage::TransferSrc),
		core::PassType::Compute,
		core::ImageFormat::R16G16B16A16_SFLOAT),
	ImageAttachment::AttachmentInfo{
		.initialLayout = core::AttachmentLayout::Undefined,
		.finalLayout = core::AttachmentLayout::ShaderReadOnlyOptimal,
		.clearOnLoad = false,
		.clearColor = Color4F(1.0f, 0.0f, 0.0f, 0.0f)
	});
}

Rc<ShadowSdfImageAttachment::AttachmentHandle> ShadowSdfImageAttachment::makeFrameHandle(const FrameQueue &handle) {
	return Rc<ShadowSdfImageAttachmentHandle>::create(this, handle);
}

void ShadowSdfImageAttachmentHandle::submitInput(FrameQueue &q, Rc<core::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	auto d = data.cast<FrameContextHandle2d>();
	if (!d || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies, [this, d = move(d), cb = move(cb)] (FrameHandle &handle, bool success) mutable {
		if (!success || !handle.isValidFlag()) {
			cb(false);
			return;
		}

		_shadowDensity = d->lights.shadowDensity;
		_sceneDensity = d->lights.sceneDensity;

		_currentImageInfo = static_cast<ImageAttachment *>(_attachment.get())->getImageInfo();
		_currentImageInfo.extent = d->lights.getShadowExtent( handle.getFrameConstraints().getScreenSize());
		handle.getRequest()->addImageSpecialization(static_cast<const ImageAttachment *>(_attachment.get()), ImageInfoData(_currentImageInfo));
		cb(true);
	});
}

}
