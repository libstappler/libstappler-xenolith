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

#include "XL2dVkParticlePass.h"
#include "SPMemory.h"
#include "SPPlatform.h"
#include "XL2dCommandList.h"
#include "XL2dParticleSystem.h"
#include "XL2dFrameContext.h"
#include "XLCoreEnum.h"
#include "XLCoreInfo.h"
#include "XLCoreQueueData.h"
#include "XLVk.h"
#include "XLVkAllocator.h"
#include "XLVkAttachment.h"
#include "XLVkDevice.h"
#include "XLVkDeviceQueue.h"
#include "glsl/XL2dShaders.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

Vector<uint64_t> ParticlePersistentData::updateEmitters(DeviceMemoryPool *pool,
		const memory::map<uint64_t, Rc<ParticleSystemData>> &data) {
	Vector<uint64_t> ret;
	auto it = _emitters.begin();
	while (it != _emitters.end()) {
		auto v = data.find(it->first);
		if (v == data.end()) {
			it = _emitters.erase(it);
		} else {
			if (it->second.systemData != v->second) {
				emplace_ordered(ret, v->first);
				updateEmitter(pool, it->second, v->second);
			}
			++it;
		}
	}

	for (auto &it : data) {
		auto v = _emitters.find(it.first);
		if (v == _emitters.end()) {
			emplace_ordered(ret, it.first);
			addEmitter(pool, it.first, it.second);
		}
	}

	return ret;
}

static constexpr uint32_t ParticlePersistentData_getFirstParticleIndex() {
	VkDeviceSize emitterBufferSize = sizeof(ParticleEmitterData) + sizeof(ParticleEmissionPoints);
	return (emitterBufferSize - 1) / sizeof(ParticleData) + 1;
}

static VkDeviceSize ParticlePersistentData_getBufferSize(ParticleSystemData *s) {
	uint32_t firstParticle = ParticlePersistentData_getFirstParticleIndex();

	auto bufferSize = (firstParticle + s->data.count) * sizeof(ParticleData);

	ParticleEmissionPoints emissionPoints;

	if (s->data.emissionType == 0) {
		if (s->emissionPoints.size() > 1) {
			bufferSize += s->emissionPoints.size() * sizeof(float) * 2;
		}
	}
	return bufferSize;
}

static void ParticlePersistentData_fillBufferPrefix(uint8_t *ptr, ParticleSystemData *s) {
	uint32_t firstParticle = ParticlePersistentData_getFirstParticleIndex();
	auto particleBufferSize = (firstParticle + s->data.count) * sizeof(ParticleData);

	memcpy(ptr, &s->data, sizeof(ParticleEmitterData));

	ParticleEmissionPoints emissionPoints;
	if (s->data.emissionType == 0) {
		if (s->emissionPoints.size() == 1) {
			emissionPoints.singlePoint = s->emissionPoints.front();
			emissionPoints.offset = 0;
		} else {
			emissionPoints.offset = particleBufferSize / sizeof(float);
		}
	}
	memcpy(ptr + sizeof(ParticleEmitterData), &emissionPoints, sizeof(ParticleEmissionPoints));
}

static void ParticlePersistentData_fillBufferSuffix(uint8_t *ptr, ParticleSystemData *s,
		uint32_t particleCount) {

	// initialize randomizer
	Vector<glsl::pcg16_state_t> randomdata;
	randomdata.resize(particleCount);

	// use hard-random from OS
	// to debug randomness - use static values
	stappler::platform::makeRandomBytes(reinterpret_cast<uint8_t *>(randomdata.data()),
			randomdata.size());

	auto particle = reinterpret_cast<ParticleData *>(ptr);

	for (size_t i = 0; i < s->data.count; ++i) {
		auto &init = randomdata[i];

		// Note - do not set values directly, use GLSL-compatible function
		glsl::pcg16_srandom_r(particle->rng, init.state, init.inc);
		++particle;
	}

	if (s->emissionPoints.size() > 1) {
		memcpy(particle, s->emissionPoints.data(), s->emissionPoints.size() * sizeof(float) * 2);
	}
}

static void ParticlePersistentData_fillBuffer(uint8_t *ptr, ParticleSystemData *s,
		uint32_t particleOffset) {
	uint32_t firstParticle = ParticlePersistentData_getFirstParticleIndex();
	ParticlePersistentData_fillBufferPrefix(ptr, s);
	ParticlePersistentData_fillBufferSuffix(ptr + firstParticle * sizeof(ParticleData), s,
			s->data.count);
}

void ParticlePersistentData::updateEmitter(DeviceMemoryPool *pool, EmitterData &e,
		ParticleSystemData *s) {
	if (e.systemData->data.count == s->data.count
			&& e.systemData->data.emissionType == s->data.emissionType
			&& e.systemData->emissionPoints == s->emissionPoints) {
		// update only emitter data
		auto stagingBuffer = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
				BufferInfo(core::ForceBufferUsage(core::BufferUsage::TransferSrc),
						sizeof(ParticleEmitterData)));

		e.systemData = s;

		stagingBuffer->map([&](uint8_t *ptr, VkDeviceSize) {
			memcpy(ptr, &s->data, sizeof(ParticleEmitterData));
		}, DeviceMemoryAccess::Flush);

		_staging.emplace_back(
				StagingData{pool, stagingBuffer, 0, e.buffer, 0, sizeof(ParticleEmitterData)});
	} else {
		auto bufferSize = ParticlePersistentData_getBufferSize(s);

		auto targetBuffer = pool->spawnPersistent(AllocationUsage::DeviceLocal,
				BufferInfo(core::BufferUsage::StorageBuffer, bufferSize));

		VkDeviceSize particlesOffet =
				ParticlePersistentData_getFirstParticleIndex() * sizeof(ParticleData);
		VkDeviceSize prefixSize = sizeof(ParticleEmitterData) + sizeof(ParticleEmissionPoints);
		VkDeviceSize suffixOffset = prefixSize;
		VkDeviceSize particlesSize = 0;

		uint32_t newParticles = 0;
		if (s->data.count > e.systemData->data.count) {
			newParticles = s->data.count - e.systemData->data.count;
			suffixOffset += e.systemData->data.count * sizeof(ParticleData);
			particlesSize = e.systemData->data.count * sizeof(ParticleData);
		} else {
			suffixOffset += s->data.count * sizeof(ParticleData);
			particlesSize = s->data.count * sizeof(ParticleData);
		}

		VkDeviceSize suffixSize = newParticles * sizeof(ParticleData);

		if (s->emissionPoints.size() > 1) {
			suffixSize += s->emissionPoints.size() * sizeof(float) * 2;
		}

		auto stagingPrefix = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
				BufferInfo(core::ForceBufferUsage(core::BufferUsage::TransferSrc), prefixSize));

		auto stagingSuffix = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
				BufferInfo(core::ForceBufferUsage(core::BufferUsage::TransferSrc), suffixSize));

		stagingPrefix->map([&](uint8_t *ptr, VkDeviceSize) {
			ParticlePersistentData_fillBufferPrefix(ptr, s);
		}, DeviceMemoryAccess::Flush);

		stagingSuffix->map([&](uint8_t *ptr, VkDeviceSize) {
			ParticlePersistentData_fillBufferSuffix(ptr, s, newParticles);
		}, DeviceMemoryAccess::Flush);

		_staging.emplace_back(StagingData{pool, stagingPrefix, 0, targetBuffer, 0, prefixSize});
		_staging.emplace_back(
				StagingData{pool, stagingSuffix, 0, targetBuffer, suffixOffset, suffixSize});
		_staging.emplace_back(StagingData{nullptr, e.buffer, particlesOffet, targetBuffer,
			particlesOffet, particlesSize});

		e.buffer = targetBuffer;
		e.systemData = s;
	}
}

void ParticlePersistentData::addEmitter(DeviceMemoryPool *pool, uint64_t id,
		ParticleSystemData *s) {
	auto bufferSize = ParticlePersistentData_getBufferSize(s);

	auto targetBuffer = pool->spawnPersistent(AllocationUsage::DeviceLocal,
			BufferInfo(core::BufferUsage::StorageBuffer, bufferSize));

	auto stagingBuffer = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(core::ForceBufferUsage(core::BufferUsage::TransferSrc), bufferSize));

	stagingBuffer->map([&](uint8_t *ptr, VkDeviceSize) {
		ParticlePersistentData_fillBuffer(ptr, s, 0);
	}, DeviceMemoryAccess::Flush);

	_emitters.emplace(id, EmitterData{id, targetBuffer, s});
	_staging.emplace_back(StagingData{pool, stagingBuffer, 0, targetBuffer, 0, bufferSize});
}

bool ParticleEmitterAttachment::init(AttachmentBuilder &builder) {
	if (!vk::BufferAttachment::init(builder,
				BufferInfo(core::BufferUsage::StorageBuffer | core::BufferUsage::TransferDst,
						core::PassType::Compute))) {
		return false;
	}

	builder.setInputValidationCallback([](const core::AttachmentInputData *) { return true; });

	builder.setInputSubmissionCallback(
			[this](FrameQueue &q, AttachmentHandle &a, core::AttachmentInputData *d,
					Function<void(bool)> &&cb) { handleInput(q, a, d, sp::move(cb)); });

	return true;
}

void ParticleEmitterAttachment::handleInput(FrameQueue &q, AttachmentHandle &handle,
		core::AttachmentInputData *d, Function<void(bool)> &&complete) {
	auto ctx = static_cast<FrameContextHandle2d *>(d);
	auto dFrame = q.getFrame().get_cast<DeviceFrameHandle>();
	auto &bufHandle = static_cast<BufferAttachmentHandle &>(handle);

	if (!d) {
		complete(false);
	}

	Vector<uint64_t> ids;

	if (!ctx->particleEmitters.empty()) {
		ids = _data->updateEmitters(dFrame->getMemPool(nullptr), ctx->particleEmitters);
	}

	for (auto &it : _data->getEmitters()) {
		bufHandle.addBufferView(it.second.buffer, 0, it.second.buffer->getSize(),
				exists_ordered(ids, it.first));
	}

	complete(true);
}

bool ParticleVertexAttachment::init(AttachmentBuilder &builder) {
	if (!vk::BufferAttachment::init(builder,
				BufferInfo(core::BufferUsage::StorageBuffer | core::BufferUsage::TransferDst,
						core::PassType::Compute))) {
		return false;
	}

	builder.setInputValidationCallback([](const core::AttachmentInputData *) { return true; });

	builder.setInputSubmissionCallback(
			[this](FrameQueue &q, AttachmentHandle &a, core::AttachmentInputData *d,
					Function<void(bool)> &&cb) { handleInput(q, a, d, sp::move(cb)); });

	return true;
}

void ParticleVertexAttachment::handleInput(FrameQueue &q, AttachmentHandle &handle,
		core::AttachmentInputData *d, Function<void(bool)> &&complete) {
	auto ctx = static_cast<FrameContextHandle2d *>(d);
	auto dFrame = q.getFrame().get_cast<DeviceFrameHandle>();
	auto &bufHandle = static_cast<BufferAttachmentHandle &>(handle);

	if (!ctx) {
		complete(false);
	}

	if (!ctx->particleEmitters.empty()) {
		auto alloc = dFrame->getMemPool(nullptr);

		size_t nVertexes = 0;

		for (auto &it : ctx->particleEmitters) { nVertexes += it.second->data.count; }

		auto vertexBuffer = alloc->spawn(AllocationUsage::DeviceLocal,
				BufferInfo(core::BufferUsage::StorageBuffer, core::BufferUsage::TransferDst,
						sizeof(ParticleVertex) * nVertexes * 4));
		auto indexBuffer = alloc->spawn(AllocationUsage::DeviceLocal,
				BufferInfo(core::BufferUsage::StorageBuffer, core::BufferUsage::IndexBuffer,
						core::BufferUsage::TransferDst, sizeof(uint32_t) * nVertexes * 6));
		auto indirectBuffer = alloc->spawn(AllocationUsage::DeviceLocal,
				BufferInfo(core::BufferUsage::StorageBuffer, core::BufferUsage::IndirectBuffer,
						core::BufferUsage::TransferDst,
						sizeof(ParticleIndirectCommand) * ctx->particleEmitters.size()));

		bufHandle.addBufferView(vertexBuffer);
		bufHandle.addBufferView(indexBuffer);
		bufHandle.addBufferView(indirectBuffer);
	}

	complete(true);
}

bool ParticlePass::init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder,
		const AttachmentData *outVertexes) {
	using namespace core;

	_vertexes = outVertexes;

	_emitters = queueBuilder.addAttachemnt(FrameContext2d::ParticleEmittersAttachment,
			[&](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();

		return Rc<ParticleEmitterAttachment>::create(builder);
	});

	passBuilder.addAttachment(_emitters);
	passBuilder.addAttachment(_vertexes,
			AttachmentDependencyInfo::make(PipelineStage::ComputeShader, AccessType::ShaderWrite));

	auto layout = passBuilder.addDescriptorLayout("ParticleLayout",
			[&, this](PipelineLayoutBuilder &layoutBuilder) {
		// Vertex input attachment - per-frame vertex list
		layoutBuilder.addSet([&, this](DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptor(passBuilder.addAttachment(_emitters));
			setBuilder.addDescriptor(passBuilder.addAttachment(_vertexes));
		});
	});

	// clang-format off
	passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
		auto particleUpdateComp =
				queueBuilder.addProgramByRef("ParticleUpdateComp", shaders::ParticleUpdateComp);

		subpassBuilder.addComputePipeline(UpdatePipelineName, layout->defaultFamily,
			SpecializationInfo(
				particleUpdateComp,
				Vector<SpecializationConstant>{
					SpecializationConstant([](const core::Device &dev, const PipelineLayoutData &) -> SpecializationConstant {
						return SpecializationConstant(getMaxEmitterCount(dev));
					})
				}
			)
		);

		subpassBuilder.setCommandsCallback([this] (FrameQueue &frame, const SubpassData &subpass, core::CommandBuffer &buf) {
			recordCommandBuffer(subpass, frame, buf);
		});
	});
	// clang-format on

	passBuilder.setAvailabilityChecker(
			[](const FrameQueue &, const QueuePassData &) { return false; });

	if (!QueuePass::init(passBuilder)) {
		return false;
	}

	return true;
}

uint32_t ParticlePass::getMaxEmitterCount(const core::Device &coreDevice) {
	auto &dev = static_cast<const vk::Device &>(coreDevice);
	auto maxBuffers =
			dev.getInfo().properties.device10.properties.limits.maxPerStageDescriptorStorageBuffers;
	return (maxBuffers - 3) / 2;
}

void ParticlePass::recordCommandBuffer(const core::SubpassData &subpass, core::FrameQueue &queue,
		core::CommandBuffer &cbuf) {
	auto dFrame = queue.getFrame().get_cast<DeviceFrameHandle>();
	auto &buf = static_cast<vk::CommandBuffer &>(cbuf);
	auto it = subpass.computePipelines.find(UpdatePipelineName);
	if (it == subpass.computePipelines.end()) {
		return;
	}

	auto memPool = dFrame->getMemPool(nullptr);
	auto transferIndirectBuffer = memPool->spawn(AllocationUsage::DeviceLocalHostVisible,
			BufferInfo(core::ForceBufferUsage(core::BufferUsage::TransferSrc),
					sizeof(ParticleIndirectCommand) * _data->getEmitters().size()));

	transferIndirectBuffer->map([&](uint8_t *buf, VkDeviceSize bufSize) {
		auto target = reinterpret_cast<ParticleIndirectCommand *>(buf);

		uint32_t firstIndex = 0;
		uint32_t vertexOffset = 0;

		for (auto &e : _data->getEmitters()) {
			target->indexCount = 0;
			target->instanceCount = 1;
			target->firstIndex = firstIndex;
			target->vertexOffset = vertexOffset;
			target->firstInstance = 0;
			target->padding20 = 0;
			target->padding24 = 0;
			target->padding28 = 0;

			vertexOffset += e.second.systemData->data.count * 4;
			firstIndex += e.second.systemData->data.count * 6;

			++target;
		}
	}, DeviceMemoryAccess::Flush);

	ParticlePushConstantBlock pcb;
	pcb.emitterIndex = 0;
	pcb.vertexOffset = 0;
	pcb.indexOffset = 0;
	pcb.nframes = 1;

	auto vertexHandle = queue.getAttachment(_vertexes)->handle.get_cast<BufferAttachmentHandle>();

	Vector<BufferMemoryBarrier> barriers;

	auto indirectBuffer = vertexHandle->getBuffers().at(2).buffer.get();

	buf.cmdCopyBuffer(transferIndirectBuffer, indirectBuffer);

	barriers.emplace_back(BufferMemoryBarrier(indirectBuffer, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT));

	for (auto it : _data->getStaging()) {
		buf.cmdCopyBuffer(it.source, it.target, it.sourceOffset, it.targetOffset, it.size);
		barriers.emplace_back(BufferMemoryBarrier(it.target, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT, it.targetOffset, it.size));
	}

	buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
			barriers);

	buf.cmdBindPipelineWithDescriptors((*it), 0);

	for (auto &e : _data->getEmitters()) {
		pcb.vertexOffset += e.second.systemData->data.count * 4;
		pcb.indexOffset += e.second.systemData->data.count * 6;
		buf.cmdPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0,
				BytesView((const uint8_t *)&pcb, sizeof(ParticlePushConstantBlock)));
		buf.cmdDispatchPipeline(*it, e.second.systemData->data.count);
		++pcb.emitterIndex;
	}
}

} // namespace stappler::xenolith::basic2d::vk
