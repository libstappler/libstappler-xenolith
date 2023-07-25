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

#ifndef XENOLITH_CORE_XLCOREATTACHMENT_H_
#define XENOLITH_CORE_XLCOREATTACHMENT_H_

#include "XLCoreQueueData.h"
#include "XLCoreInfo.h"
#include "XLCorePlatform.h"

namespace stappler::xenolith::core {

struct DependencyEvent : public Ref {
	static uint32_t GetNextId();

	virtual ~DependencyEvent() { }

	uint32_t id = GetNextId();
	uint64_t clock = platform::clock(core::ClockType::Monotonic);
	std::atomic<uint32_t> signaled = 1;
	std::atomic<bool> submitted = false;
	bool success = true;
};

// dummy class for attachment input
struct AttachmentInputData : public Ref {
	virtual ~AttachmentInputData() { }

	Vector<Rc<DependencyEvent>> waitDependencies;
};

class Attachment : public NamedRef {
public:
	using FrameQueue = core::FrameQueue;
	using RenderQueue = core::Queue;
	using PassData = core::QueuePassData;
	using AttachmentData = core::AttachmentData;
	using AttachmentHandle = core::AttachmentHandle;
	using AttachmentBuilder = core::AttachmentBuilder;

	using InputCallback = Function<void(FrameQueue &, const Rc<AttachmentHandle> &, Function<void(bool)> &&)>;

	virtual ~Attachment() { }

	virtual bool init(AttachmentBuilder &builder);
	virtual void clear();

	virtual StringView getName() const override;

	uint64_t getId() const;
	AttachmentUsage getUsage() const;
	bool isTransient() const;

	// Set callback for frame to acquire input for this
	void setInputCallback(InputCallback &&);

	// Run input callback for frame and handle
	void acquireInput(FrameQueue &, const Rc<AttachmentHandle> &, Function<void(bool)> &&);

	virtual bool validateInput(const Rc<AttachmentInputData> &) const;

	virtual bool isCompatible(const ImageInfo &) const { return false; }

	virtual void sortDescriptors(Queue &queue, Device &dev);

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &);

	virtual Vector<const PassData *> getRenderPasses() const;
	virtual const PassData *getFirstRenderPass() const;
	virtual const PassData *getLastRenderPass() const;
	virtual const PassData *getNextRenderPass(const PassData *) const;
	virtual const PassData *getPrevRenderPass(const PassData *) const;

	const AttachmentData *getData() const { return _data; }

protected:
	const AttachmentData *_data = nullptr;

	InputCallback _inputCallback;
};

class BufferAttachment : public Attachment {
public:
	virtual ~BufferAttachment() { }

	virtual bool init(AttachmentBuilder &builder, const BufferInfo &);
	virtual void clear() override;

	const BufferInfo &getInfo() const { return _info; }

protected:
	using Attachment::init;

	BufferInfo _info;
};

class ImageAttachment : public Attachment {
public:
	virtual ~ImageAttachment() { }

	struct AttachmentInfo {
		AttachmentLayout initialLayout = AttachmentLayout::Ignored;
		AttachmentLayout finalLayout = AttachmentLayout::Ignored;
		bool clearOnLoad = false;
		Color4F clearColor = Color4F::BLACK;
		ColorMode colorMode;
	};

	virtual bool init(AttachmentBuilder &builder, const ImageInfo &, AttachmentInfo &&);

	virtual const ImageInfo &getImageInfo() const { return _imageInfo; }
	virtual bool shouldClearOnLoad() const { return _attachmentInfo.clearOnLoad; }
	virtual Color4F getClearColor() const { return _attachmentInfo.clearColor; }
	virtual ColorMode getColorMode() const { return _attachmentInfo.colorMode; }

	virtual AttachmentLayout getInitialLayout() const { return _attachmentInfo.initialLayout; }
	virtual AttachmentLayout getFinalLayout() const { return _attachmentInfo.finalLayout; }

	virtual void addImageUsage(ImageUsage);

	virtual bool isCompatible(const ImageInfo &) const override;

	virtual ImageViewInfo getImageViewInfo(const ImageInfoData &info, const AttachmentPassData &) const;
	virtual Vector<ImageViewInfo> getImageViews(const ImageInfoData &info) const;

protected:
	using Attachment::init;

	ImageInfo _imageInfo;
	AttachmentInfo _attachmentInfo;
};

class GenericAttachment : public Attachment {
public:
	virtual ~GenericAttachment() { }

	virtual bool init(AttachmentBuilder &builder);

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using Attachment::init;
};

class AttachmentHandle : public Ref {
public:
	using PassHandle = core::QueuePassHandle;
	using FrameQueue = core::FrameQueue;
	using FrameHandle = core::FrameHandle;
	using Attachment = core::Attachment;

	virtual ~AttachmentHandle() { }

	virtual bool init(const Rc<Attachment> &, const FrameQueue &);
	virtual void setQueueData(FrameAttachmentData &);
	virtual FrameAttachmentData *getQueueData() const { return _queueData; }

	virtual bool isAvailable(const FrameQueue &) const { return true; }

	// returns true for immediate setup, false if setup job was scheduled
	virtual bool setup(FrameQueue &, Function<void(bool)> &&);

	virtual void finalize(FrameQueue &, bool successful);

	virtual bool isInput() const;
	virtual bool isOutput() const;
	virtual const Rc<Attachment> &getAttachment() const { return _attachment; }
	StringView getName() const { return _attachment->getName(); }

	virtual void submitInput(FrameQueue &, Rc<AttachmentInputData> &&, Function<void(bool)> &&cb);

	virtual uint32_t getDescriptorArraySize(const PassHandle &, const PipelineDescriptor &, bool isExternal) const;
	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &, uint32_t, bool isExternal) const;

protected:
	Rc<Attachment> _attachment;
	FrameAttachmentData *_queueData = nullptr;
};

}

#endif /* XENOLITH_CORE_XLCOREATTACHMENT_H_ */
