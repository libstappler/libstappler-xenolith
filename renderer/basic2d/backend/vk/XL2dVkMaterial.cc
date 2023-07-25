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

#include "XL2dVkMaterial.h"

namespace stappler::xenolith::basic2d::vk {

MaterialAttachment::~MaterialAttachment() { }

bool MaterialAttachment::init(AttachmentBuilder &builder, const BufferInfo &info) {
	return core::MaterialAttachment::init(builder, info, [] (uint8_t *target, const core::Material *material) {
		auto &images = material->getImages();
		if (!images.empty()) {
			MaterialData material;
			auto &image = images.front();
			material.samplerImageIdx = image.descriptor | (image.sampler << 16);
			material.setIdx = image.set;
			material.flags = 0;
			material.atlasIdx = 0;
			if (image.image->atlas) {
				if (auto &index = image.image->atlas->getIndexBuffer()) {
					material.flags |= 1;
					material.atlasIdx |= index->getDescriptor();

					auto indexSize = image.image->atlas->getIndexData().size() / sizeof(DataAtlasIndex);
					auto pow2index = std::countr_zero(indexSize);

					material.flags |= (pow2index << 24);
				}
				if (auto &data = image.image->atlas->getDataBuffer()) {
					material.flags |= 2;
					material.atlasIdx |= (data->getDescriptor() << 16);
				}
			}
			memcpy(target, &material, sizeof(MaterialData));
			return true;
		}
		return false;
	}, sizeof(MaterialData));
}

auto MaterialAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<MaterialAttachmentHandle>::create(this, handle);
}

MaterialAttachmentHandle::~MaterialAttachmentHandle() { }

bool MaterialAttachmentHandle::init(const Rc<Attachment> &a, const FrameQueue &handle) {
	if (BufferAttachmentHandle::init(a, handle)) {
		return true;
	}
	return false;
}

bool MaterialAttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &desc,
		uint32_t, bool isExternal) const {
	return _materials && _materials->getGeneration() != desc.boundGeneration;
}

bool MaterialAttachmentHandle::writeDescriptor(const core::QueuePassHandle &handle, DescriptorBufferInfo &info) {
	if (!_materials) {
		return false;
	}

	auto b = _materials->getBuffer();
	if (!b) {
		return false;
	}
	info.buffer = static_cast<Buffer *>(b.get());
	info.offset = 0;
	info.range = info.buffer->getSize();
	info.descriptor->boundGeneration = _materials->getGeneration();
	return true;
}

const MaterialAttachment *MaterialAttachmentHandle::getMaterialAttachment() const {
	return static_cast<MaterialAttachment *>(_attachment.get());
}

const Rc<core::MaterialSet> MaterialAttachmentHandle::getSet() const {
	if (!_materials) {
		_materials = getMaterialAttachment()->getMaterials();
	}
	return _materials;
}

}
