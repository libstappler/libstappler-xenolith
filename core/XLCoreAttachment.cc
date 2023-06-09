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

#include "XLCoreAttachment.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameHandle.h"

namespace stappler::xenolith::core {

uint32_t DependencyEvent::GetNextId() {
	static std::atomic<uint32_t> s_eventId = 1;
	return s_eventId.fetch_add(1);
}

bool Attachment::init(AttachmentBuilder &builder) {
	_data = builder.getAttachmentData();
	return true;
}

void Attachment::clear() {

}

StringView Attachment::getName() const {
	return _data->key;
}

AttachmentUsage Attachment::getUsage() const {
	return _data->usage;
}

bool Attachment::isTransient() const {
	return _data->transient;
}

void Attachment::setInputCallback(Function<void(FrameQueue &, const Rc<AttachmentHandle> &, Function<void(bool)> &&)> &&input) {
	_inputCallback = move(input);
}

void Attachment::acquireInput(FrameQueue &frame, const Rc<AttachmentHandle> &a, Function<void(bool)> &&cb) {
	if (_inputCallback) {
		_inputCallback(frame, a, move(cb));
	} else {
		// wait for input on handle
		frame.getFrame()->waitForInput(frame, a, move(cb));
	}
}

bool Attachment::validateInput(const Rc<AttachmentInputData> &) const {
	return true;
}

void Attachment::sortDescriptors(Queue &queue, Device &dev) { }

Rc<AttachmentHandle> Attachment::makeFrameHandle(const FrameQueue &) {
	return nullptr;
}

Vector<const QueuePassData *> Attachment::getRenderPasses() const {
	Vector<const PassData *> ret;
	for (auto &it : _data->passes) {
		ret.emplace_back(it->pass);
	}
	return ret;
}

const QueuePassData *Attachment::getFirstRenderPass() const {
	if (_data->passes.empty()) {
		return nullptr;
	}

	return _data->passes.front()->pass;
}

const QueuePassData *Attachment::getLastRenderPass() const {
	if (_data->passes.empty()) {
		return nullptr;
	}

	return _data->passes.back()->pass;
}

const QueuePassData *Attachment::getNextRenderPass(const PassData *pass) const {
	size_t idx = 0;
	for (auto &it : _data->passes) {
		if (it->pass == pass) {
			break;
		}
		++idx;
	}
	if (idx + 1 >= _data->passes.size()) {
		return nullptr;
	}
	return _data->passes.at(idx + 1)->pass;
}

const QueuePassData *Attachment::getPrevRenderPass(const PassData *pass) const {
	size_t idx = 0;
	for (auto &it : _data->passes) {
		if (it->pass == pass) {
			break;
		}
		++ idx;
	}
	if (idx == 0) {
		return nullptr;
	}
	return _data->passes.at(idx - 1)->pass;
}

bool BufferAttachment::init(AttachmentBuilder &builder, const BufferInfo &info) {
	_info = info;
	builder.setType(AttachmentType::Buffer);
	if (Attachment::init(builder)) {
		_info.key = _data->key;
		return true;
	}
	return false;
}

void BufferAttachment::clear() {
	Attachment::clear();
}

bool ImageAttachment::init(AttachmentBuilder &builder, const ImageInfo &info, AttachmentInfo &&a) {
	builder.setType(AttachmentType::Image);
	if (Attachment::init(builder)) {
		_imageInfo = info;
		_attachmentInfo = move(a);
		_imageInfo.key = _data->key;
		return true;
	}
	return false;
}

void ImageAttachment::addImageUsage(ImageUsage usage) {
	_imageInfo.usage |= usage;
}

bool ImageAttachment::isCompatible(const ImageInfo &image) const {
	return _imageInfo.isCompatible(image);
}

Extent3 ImageAttachment::getSizeForFrame(const FrameQueue &frame) const {
	Extent3 ret = _imageInfo.extent;
	auto spec = frame.getFrame()->getImageSpecialization(this);
	if (_attachmentInfo.frameSizeCallback) {
		ret = _attachmentInfo.frameSizeCallback(frame, spec);
	} else if (spec) {
		ret = spec->extent;
	} else {
		ret = Extent3(frame.getExtent());
	}
	return ret;
}

ImageViewInfo ImageAttachment::getImageViewInfo(const ImageInfoData &info, const AttachmentPassData &passAttachment) const {
	bool allowSwizzle = true;
	AttachmentUsage usage = AttachmentUsage::None;
	for (auto &subpassAttachment : passAttachment.subpasses) {
		usage |= subpassAttachment->usage;
	}

	if ((usage & AttachmentUsage::Input) != AttachmentUsage::None
			|| (usage & AttachmentUsage::Resolve) != AttachmentUsage::None
			|| (usage & AttachmentUsage::DepthStencil) != AttachmentUsage::None) {
		allowSwizzle = false;
	}

	ImageViewInfo passInfo(info);
	passInfo.setup(passAttachment.colorMode, allowSwizzle);
	return passInfo;
}

Vector<ImageViewInfo> ImageAttachment::getImageViews(const ImageInfoData &info) const {
	Vector<ImageViewInfo> ret;

	auto addView = [&] (const ImageViewInfo &info) {
		auto it = std::find(ret.begin(), ret.end(), info);
		if (it == ret.end()) {
			ret.emplace_back(info);
		}
	};

	for (auto &passAttachment : _data->passes) {
		addView(getImageViewInfo(info, *passAttachment));

		for (auto &desc : passAttachment->descriptors) {
			bool allowSwizzle = (desc->type == DescriptorType::SampledImage);

			ImageViewInfo passInfo(info);
			passInfo.setup(passAttachment->colorMode, allowSwizzle);
			addView(passInfo);
		}
	}

	return ret;
}

bool GenericAttachment::init(AttachmentBuilder &builder) {
	builder.setType(AttachmentType::Generic);
	return Attachment::init(builder);
}

Rc<AttachmentHandle> GenericAttachment::makeFrameHandle(const FrameQueue &h) {
	return Rc<AttachmentHandle>::create(this, h);
}

bool AttachmentHandle::init(const Rc<Attachment> &attachment, const FrameQueue &frame) {
	_attachment = attachment;
	return true;
}

void AttachmentHandle::setQueueData(FrameAttachmentData &data) {
	_queueData = &data;
}

// returns true for immediate setup, false if seyup job was scheduled
bool AttachmentHandle::setup(FrameQueue &, Function<void(bool)> &&) {
	return true;
}

void AttachmentHandle::finalize(FrameQueue &, bool successful) {

}

void AttachmentHandle::submitInput(FrameQueue &q, Rc<AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	if (data->waitDependencies.empty()) {
		cb(true);
	} else {
		q.getFrame()->waitForDependencies(data->waitDependencies, [cb = move(cb)] (FrameHandle &, bool success) {
			cb(success);
		});
	}
}

bool AttachmentHandle::isInput() const {
	return (_attachment->getUsage() & AttachmentUsage::Input) != AttachmentUsage::None;
}

bool AttachmentHandle::isOutput() const {
	return (_attachment->getUsage() & AttachmentUsage::Output) != AttachmentUsage::None;
}

uint32_t AttachmentHandle::getDescriptorArraySize(const PassHandle &, const PipelineDescriptor &d, bool isExternal) const {
	return d.count;
}

bool AttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &d, uint32_t, bool isExternal) const {
	return false;
}

}
