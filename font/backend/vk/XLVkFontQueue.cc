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

#include "XLVkFontQueue.h"
#include "XLFontLibrary.h"
#include "XLCoreFrameQueue.h"
#include "XLFontDeferredRequest.h"

#if MODULE_XENOLITH_BACKEND_VK

#include "XLVkAllocator.h"

namespace stappler::xenolith::vk {

struct RenderFontCharTextureData {
	int16_t x = 0;
	int16_t y = 0;
	uint16_t width = 0;
	uint16_t height = 0;
};

struct RenderFontCharPersistentData {
	RenderFontCharTextureData texture;
	uint32_t objectId;
	uint32_t bufferIdx;
	uint32_t offset;
};

struct RenderFontPersistentBufferUserdata : public Ref {
	Rc<DeviceMemoryPool> mempool;
	Vector<Rc<Buffer>> buffers;
	HashMap<uint32_t, RenderFontCharPersistentData> chars;
};

class FontAttachment : public core::GenericAttachment {
public:
	virtual ~FontAttachment();

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class FontAttachmentHandle : public core::AttachmentHandle {
public:
	virtual ~FontAttachmentHandle();

	virtual bool setup(FrameQueue &, Function<void(bool)> &&) override;
	virtual void submitInput(FrameQueue &, Rc<core::AttachmentInputData> &&, Function<void(bool)> &&) override;

	Extent2 getImageExtent() const { return _imageExtent; }
	const Rc<font::RenderFontInput> &getInput() const { return _input; }
	const Rc<Buffer> &getTmpBuffer() const { return _frontBuffer; }
	const Rc<Buffer> &getPersistentTargetBuffer() const { return _persistentTargetBuffer; }
	const Rc<core::DataAtlas> &getAtlas() const { return _atlas; }
	const Rc<RenderFontPersistentBufferUserdata> &getUserdata() const { return _userdata; }
	const Vector<VkBufferImageCopy> &getCopyFromTmpBufferData() const { return _copyFromTmpBufferData; }
	const Map<Buffer *, Vector<VkBufferImageCopy>> &getCopyFromPersistentBufferData() const { return _copyFromPersistentBufferData; }
	const Vector<VkBufferCopy> &getCopyToPersistentBufferData() const { return _copyToPersistentBufferData; }

protected:
	void doSubmitInput(FrameHandle &, Function<void(bool)> &&cb, Rc<font::RenderFontInput> &&d);
	void writeAtlasData(FrameHandle &, bool underlinePersistent);

	uint32_t nextBufferOffset(size_t blockSize);
	uint32_t nextPersistentTransferOffset(size_t blockSize);

	bool addPersistentCopy(uint16_t fontId, char16_t c);
	void pushCopyTexture(uint32_t reqIdx, const font::CharTexture &texData);
	void pushAtlasTexture(core::DataAtlas *, VkBufferImageCopy &);

	Rc<font::RenderFontInput> _input;
	Rc<RenderFontPersistentBufferUserdata> _userdata;
	uint32_t _counter = 0;
	VkDeviceSize _bufferSize = 0;
	VkDeviceSize _optimalRowAlignment = 1;
	VkDeviceSize _optimalTextureAlignment = 1;
	std::atomic<uint32_t> _bufferOffset = 0;
	std::atomic<uint32_t> _persistentOffset = 0;
	std::atomic<uint32_t> _copyFromTmpOffset = 0;
	std::atomic<uint32_t> _copyToPersistentOffset = 0;
	std::atomic<uint32_t> _textureTargetOffset = 0;
	Rc<Buffer> _frontBuffer;
	Rc<Buffer> _persistentTargetBuffer;
	Rc<core::DataAtlas> _atlas;
	Vector<VkBufferImageCopy> _copyFromTmpBufferData;
	Map<Buffer *, Vector<VkBufferImageCopy>> _copyFromPersistentBufferData;
	Vector<VkBufferCopy> _copyToPersistentBufferData;
	Vector<RenderFontCharPersistentData> _copyPersistentCharData;
	Vector<RenderFontCharTextureData> _textureTarget;
	Extent2 _imageExtent;
	Mutex _mutex;
	Function<void(bool)> _onInput;
};

class FontRenderPass : public QueuePass {
public:
	virtual ~FontRenderPass();

	virtual bool init(QueuePassBuilder &passBuilder, const AttachmentData *attachment);

	virtual Rc<QueuePassHandle> makeFrameHandle(const FrameQueue &) override;

	const AttachmentData *getRenderFontAttachment() const { return _fontAttachment; }

protected:
	using QueuePass::init;

	const AttachmentData *_fontAttachment;
};

class FontRenderPassHandle : public QueuePassHandle {
public:
	virtual ~FontRenderPassHandle();

	virtual bool init(QueuePass &, const FrameQueue &) override;

	virtual QueueOperations getQueueOps() const override;

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;
	virtual void finalize(FrameQueue &, bool successful)  override;

protected:
	virtual Vector<const CommandBuffer *> doPrepareCommands(FrameHandle &) override;

	virtual void doSubmitted(FrameHandle &, Function<void(bool)> &&, bool, Rc<Fence> &&) override;
	virtual void doComplete(FrameQueue &, Function<void(bool)> &&, bool) override;

	FontAttachmentHandle *_fontAttachment = nullptr;
	QueueOperations _queueOps = QueueOperations::None;
	Rc<Image> _targetImage;
	Rc<Buffer> _targetAtlasIndex;
	Rc<Buffer> _targetAtlasData;
	Rc<Buffer> _outBuffer;
};

FontQueue::~FontQueue() { }

bool FontQueue::init(StringView name) {
	using namespace core;
	Queue::Builder builder(name);

	auto attachment = builder.addAttachemnt("RenderFontQueueAttachment", [] (AttachmentBuilder &attachmentBuilder) -> Rc<Attachment> {
		attachmentBuilder.defineAsInput();
		attachmentBuilder.defineAsOutput();
		return Rc<FontAttachment>::create(attachmentBuilder);
	});

	builder.addPass("RenderFontQueuePass", PassType::Transfer, RenderOrdering(0), [&] (QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<FontRenderPass>::create(passBuilder, attachment);
	});

	if (Queue::init(move(builder))) {
		_attachment = attachment;
		return true;
	}
	return false;
}

FontAttachment::~FontAttachment() { }

auto FontAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<FontAttachmentHandle>::create(this, handle);
}

FontAttachmentHandle::~FontAttachmentHandle() { }

bool FontAttachmentHandle::setup(FrameQueue &handle, Function<void(bool)> &&) {
	auto dev = static_cast<Device *>(handle.getFrame()->getDevice());
	_optimalTextureAlignment = std::max(dev->getInfo().properties.device10.properties.limits.optimalBufferCopyOffsetAlignment, VkDeviceSize(4));
	_optimalRowAlignment = std::max(dev->getInfo().properties.device10.properties.limits.optimalBufferCopyRowPitchAlignment, VkDeviceSize(4));
	return true;
}

static Extent2 FontAttachmentHandle_buildTextureData(Vector<SpanView<VkBufferImageCopy>> requests) {
	memory::vector<VkBufferImageCopy *> layoutData; layoutData.reserve(requests.size());

	float totalSquare = 0.0f;

	for (auto &v : requests) {
		for (auto &d : v) {
			auto it = std::lower_bound(layoutData.begin(), layoutData.end(), &d,
					[] (const VkBufferImageCopy * l, const VkBufferImageCopy * r) -> bool {
				if (l->imageExtent.height == r->imageExtent.height && l->imageExtent.width == r->imageExtent.width) {
					return l->bufferImageHeight < r->bufferImageHeight;
				} else if (l->imageExtent.height == r->imageExtent.height) {
					return l->imageExtent.width > r->imageExtent.width;
				} else {
					return l->imageExtent.height > r->imageExtent.height;
				}
			});
			layoutData.emplace(it, const_cast<VkBufferImageCopy *>(&d));
			totalSquare += d.imageExtent.width * d.imageExtent.height;
		}
	}

	geom::EmplaceCharInterface iface({
		[] (void *ptr) -> uint16_t { return (reinterpret_cast<VkBufferImageCopy *>(ptr))->imageOffset.x; }, // x
		[] (void *ptr) -> uint16_t { return (reinterpret_cast<VkBufferImageCopy *>(ptr))->imageOffset.y; }, // y
		[] (void *ptr) -> uint16_t { return (reinterpret_cast<VkBufferImageCopy *>(ptr))->imageExtent.width; }, // width
		[] (void *ptr) -> uint16_t { return (reinterpret_cast<VkBufferImageCopy *>(ptr))->imageExtent.height; }, // height
		[] (void *ptr, uint16_t value) { (reinterpret_cast<VkBufferImageCopy *>(ptr))->imageOffset.x = value; }, // x
		[] (void *ptr, uint16_t value) { (reinterpret_cast<VkBufferImageCopy *>(ptr))->imageOffset.y = value; }, // y
		[] (void *ptr, uint16_t value) { }, // tex
	});

	auto span = makeSpanView(reinterpret_cast<void **>(layoutData.data()), layoutData.size());

	return geom::emplaceChars(iface, span, totalSquare);
}

void FontAttachmentHandle::submitInput(FrameQueue &q, Rc<core::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	auto d = data.cast<font::RenderFontInput>();
	if (!d || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies, [this, cb = move(cb), d = move(d)] (FrameHandle &handle, bool success) mutable {
		handle.performInQueue([this, cb = move(cb), d = move(d)] (FrameHandle &handle) mutable -> bool {
			doSubmitInput(handle, move(cb), move(d));
			return true;
		}, nullptr, "RenderFontAttachmentHandle::submitInput");
	});
}

void FontAttachmentHandle::doSubmitInput(FrameHandle &handle, Function<void(bool)> &&cb, Rc<font::RenderFontInput> &&d) {
	_counter = d->requests.size();
	_input = d;
	if (auto instance = d->image->getInstance()) {
		if (auto ud = instance->userdata.cast<RenderFontPersistentBufferUserdata>()) {
			_userdata = ud;
		}
	}

	// process persistent chars
	bool underlinePersistent = false;
	uint32_t totalCount = 0;
	for (auto &it : _input->requests) {
		totalCount += it.chars.size();
	}

	_textureTarget.resize(totalCount + 1); // used in addPersistentCopy

	uint32_t extraPersistent = 0;
	uint32_t processedPersistent = 0;
	if (_userdata) {
		for (auto &it : _input->requests) {
			if (it.persistent) {
				for (auto &c : it.chars) {
					if (addPersistentCopy(it.object->getId(), c)) {
						++ processedPersistent;
						c = 0;
					} else {
						++ extraPersistent;
					}
				}
			}
		}

		if (addPersistentCopy(font::CharLayout::SourceMax, 0)) {
			underlinePersistent = true;
		}
	} else {
		for (auto &it : _input->requests) {
			if (it.persistent) {
				extraPersistent += it.chars.size();
			}
		}

		underlinePersistent = false;
	}

	_onInput = move(cb); // see RenderFontAttachmentHandle::writeAtlasData

	if (processedPersistent == totalCount && underlinePersistent) {
		// no need to transfer extra chars
		writeAtlasData(handle, underlinePersistent);
		return;
	}

	auto frame = static_cast<DeviceFrameHandle *>(&handle);
	auto &memPool = frame->getMemPool(&handle);

	_frontBuffer = memPool->spawn(AllocationUsage::HostTransitionSource, core::BufferInfo(
		core::ForceBufferUsage(core::BufferUsage::TransferSrc),
		size_t(Allocator::PageSize * 2)
	));

	_copyFromTmpBufferData.resize(totalCount - processedPersistent + (underlinePersistent ? 0 : 1));

	if (extraPersistent > 0 || !underlinePersistent) {
		_copyToPersistentBufferData.resize(extraPersistent + (underlinePersistent ? 0 : 1));
		_copyPersistentCharData.resize(extraPersistent + (underlinePersistent ? 0 : 1));

		if (!_userdata) {
			_userdata = Rc<RenderFontPersistentBufferUserdata>::alloc();
			_userdata->mempool = Rc<DeviceMemoryPool>::create(memPool->getAllocator(), false);
			_userdata->buffers.emplace_back(_userdata->mempool->spawn(AllocationUsage::DeviceLocal, core::BufferInfo(
					core::ForceBufferUsage(core::BufferUsage::TransferSrc | core::BufferUsage::TransferDst),
					size_t(Allocator::PageSize * 2)
				)));
			_persistentTargetBuffer = _userdata->buffers.back();
		} else {
			auto tmp = move(_userdata);
			_userdata = Rc<RenderFontPersistentBufferUserdata>::alloc();
			_userdata->mempool = tmp->mempool;
			_userdata->chars = tmp->chars;
			_userdata->buffers = tmp->buffers;

			if (!_userdata->buffers.empty()) {
				_persistentTargetBuffer = _userdata->buffers.back();
			}
		}
	}

	font::DeferredRequest::runFontRenderer(*_input->queue, _input->library, _input->requests, [this] (uint32_t reqIdx, const font::CharTexture &texData) {
		pushCopyTexture(reqIdx, texData);
	}, [this, handle = Rc<FrameHandle>(&handle), underlinePersistent] {
		writeAtlasData(*handle, underlinePersistent);
		// std::cout << "RenderFontAttachmentHandle::submitInput: " << platform::device::_clock(platform::device::ClockType::Monotonic) - handle->getTimeStart() << "\n";
	});
}

void FontAttachmentHandle::writeAtlasData(FrameHandle &handle, bool underlinePersistent) {
	Vector<SpanView<VkBufferImageCopy>> commands;
	if (!underlinePersistent) {
		// write single white pixel for underlines
		uint32_t offset = _frontBuffer->reserveBlock(1, _optimalTextureAlignment);
		if (offset + 1 <= Allocator::PageSize * 2) {
			uint8_t whiteColor = 255;
			_frontBuffer->setData(BytesView(&whiteColor, 1), offset);
			auto objectId = font::CharLayout::getObjectId(font::CharLayout::SourceMax, char16_t(0), geom::SpriteAnchor::BottomLeft);
			auto texOffset = _textureTargetOffset.fetch_add(1);
			_copyFromTmpBufferData[_copyFromTmpBufferData.size() - 1] = VkBufferImageCopy({
				VkDeviceSize(offset),
				uint32_t(texOffset),
				uint32_t(objectId),
				VkImageSubresourceLayers({VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}),
				VkOffset3D({0, 0, 0}),
				VkExtent3D({1, 1, 1})
			});

			auto targetOffset = _persistentTargetBuffer->reserveBlock(1, _optimalTextureAlignment);
			_textureTarget[texOffset] = RenderFontCharTextureData{0, 0, 1, 1};
			_copyToPersistentBufferData[_copyToPersistentBufferData.size() - 1] = VkBufferCopy({
				offset, targetOffset, 1
			});
			_copyPersistentCharData[_copyPersistentCharData.size() - 1] = RenderFontCharPersistentData{
				RenderFontCharTextureData{0, 0, 1, 1},
				objectId, 0, uint32_t(targetOffset)
			};
		}
	}

	// fill new persistent chars
	for (auto &it : _copyPersistentCharData) {
		it.bufferIdx = _userdata->buffers.size() - 1;
		auto cIt = _userdata->chars.find(it.objectId);
		if (cIt == _userdata->chars.end()) {
			_userdata->chars.emplace(it.objectId, it);
		} else {
			cIt->second = it;
		}
	}

	auto pool = memory::pool::create(memory::pool::acquire());
	memory::pool::push(pool);

	// TODO - use GPU rectangle placement
	commands.emplace_back(_copyFromTmpBufferData);
	for (auto &it : _copyFromPersistentBufferData) {
		commands.emplace_back(it.second);
	}

	_imageExtent = FontAttachmentHandle_buildTextureData(commands);

	auto atlas = Rc<core::DataAtlas>::create(core::DataAtlas::ImageAtlas,
			_copyFromTmpBufferData.size() * 4, sizeof(font::FontAtlasValue), _imageExtent);

	for (auto &c : commands) {
		for (auto &it : c) {
			pushAtlasTexture(atlas, const_cast<VkBufferImageCopy &>(it));
		}
	}

	atlas->compile();
	_atlas = move(atlas);

	memory::pool::pop();
	memory::pool::destroy(pool);

	handle.performOnGlThread([this] (FrameHandle &handle) {
		_onInput(true);
		_onInput = nullptr;
	}, this, false, "RenderFontAttachmentHandle::writeAtlasData");
}

uint32_t FontAttachmentHandle::nextBufferOffset(size_t blockSize) {
	auto alignedSize = math::align(uint64_t(blockSize), _optimalTextureAlignment);
	return _bufferOffset.fetch_add(alignedSize);
}

uint32_t FontAttachmentHandle::nextPersistentTransferOffset(size_t blockSize) {
	auto alignedSize = math::align(uint64_t(blockSize), _optimalTextureAlignment);
	return _persistentOffset.fetch_add(alignedSize);
}

bool FontAttachmentHandle::addPersistentCopy(uint16_t fontId, char16_t c) {
	auto objId = font::CharLayout::getObjectId(fontId, c, geom::SpriteAnchor::BottomLeft);
	auto it = _userdata->chars.find(objId);
	if (it != _userdata->chars.end()) {
		auto &buf = _userdata->buffers[it->second.bufferIdx];
		auto bufIt = _copyFromPersistentBufferData.find(buf.get());
		if (bufIt == _copyFromPersistentBufferData.end()) {
			bufIt = _copyFromPersistentBufferData.emplace(buf, Vector<VkBufferImageCopy>()).first;
		}

		auto texTarget = _textureTargetOffset.fetch_add(1);
		bufIt->second.emplace_back(VkBufferImageCopy{
			VkDeviceSize(it->second.offset),
			uint32_t(texTarget),
			uint32_t(objId),
			VkImageSubresourceLayers({VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}),
			VkOffset3D({0, 0, 0}),
			VkExtent3D({it->second.texture.width, it->second.texture.height, 1})
		});

		_textureTarget[texTarget] = it->second.texture;
		return true;
	}
	return false;
}

void FontAttachmentHandle::pushCopyTexture(uint32_t reqIdx, const font::CharTexture &texData) {
	if (texData.width != texData.bitmapWidth || texData.height != texData.bitmapRows) {
		log::error("FontAttachmentHandle", "Invalid size: ", texData.width, ";", texData.height,
				" vs. ", texData.bitmapWidth, ";", texData.bitmapRows, "\n");
	}

	auto size = texData.bitmapRows * std::abs(texData.pitch);
	uint32_t offset = _frontBuffer->reserveBlock(size, _optimalTextureAlignment);
	if (offset + size > Allocator::PageSize * 2) {
		return;
	}

	auto ptr = texData.bitmap;
	if (texData.pitch >= 0) {
		_frontBuffer->setData(BytesView(ptr, texData.pitch * texData.bitmapRows), offset);
	} else {
		for (size_t i = 0; i < texData.bitmapRows; ++ i) {
			_frontBuffer->setData(BytesView(ptr, -texData.pitch), offset + i * (-texData.pitch));
			ptr += texData.pitch;
		}
	}

	auto objectId = font::CharLayout::getObjectId(texData.fontID, texData.charID, geom::SpriteAnchor::BottomLeft);
	auto texOffset = _textureTargetOffset.fetch_add(1);
	_copyFromTmpBufferData[_copyFromTmpOffset.fetch_add(1)] = VkBufferImageCopy({
		VkDeviceSize(offset),
		uint32_t(texOffset),
		uint32_t(objectId),
		VkImageSubresourceLayers({VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}),
		VkOffset3D({0, 0, 0}),
		VkExtent3D({texData.bitmapWidth, texData.bitmapRows, 1})
	});
	_textureTarget[texOffset] = RenderFontCharTextureData{texData.x, texData.y, texData.width, texData.height};

	if (_input->requests[reqIdx].persistent) {
		auto targetIdx = _copyToPersistentOffset.fetch_add(1);
		auto targetOffset = _persistentTargetBuffer->reserveBlock(size, _optimalTextureAlignment);
		_copyToPersistentBufferData[targetIdx] = VkBufferCopy({
			offset, targetOffset, size
		});
		_copyPersistentCharData[targetIdx] = RenderFontCharPersistentData{
			RenderFontCharTextureData{texData.x, texData.y, texData.width, texData.height},
			objectId, 0, uint32_t(targetOffset)
		};
	}
}

void FontAttachmentHandle::pushAtlasTexture(core::DataAtlas *atlas, VkBufferImageCopy &d) {
	font::FontAtlasValue data[4];

	auto texOffset = d.bufferRowLength;
	auto id = d.bufferImageHeight;
	d.bufferImageHeight = 0;
	d.bufferRowLength = 0;

	auto &tex = _textureTarget[texOffset];

	const float x = float(d.imageOffset.x);
	const float y = float(d.imageOffset.y);
	const float w = float(d.imageExtent.width);
	const float h = float(d.imageExtent.height);

	data[0].pos = Vec2(tex.x, -tex.y);
	data[0].tex = Vec2(x / _imageExtent.width, y / _imageExtent.height);

	data[1].pos = Vec2(tex.x, -tex.y - tex.height);
	data[1].tex = Vec2(x / _imageExtent.width, (y + h) / _imageExtent.height);

	data[2].pos = Vec2(tex.x + tex.width, -tex.y - tex.height);
	data[2].tex = Vec2((x + w) / _imageExtent.width, (y + h) / _imageExtent.height);

	data[3].pos = Vec2(tex.x + tex.width, -tex.y);
	data[3].tex = Vec2((x + w) / _imageExtent.width, y / _imageExtent.height);

	atlas->addObject(font::CharLayout::getObjectId(id, geom::SpriteAnchor::BottomLeft), &data[0]);
	atlas->addObject(font::CharLayout::getObjectId(id, geom::SpriteAnchor::TopLeft), &data[1]);
	atlas->addObject(font::CharLayout::getObjectId(id, geom::SpriteAnchor::TopRight), &data[2]);
	atlas->addObject(font::CharLayout::getObjectId(id, geom::SpriteAnchor::BottomRight), &data[3]);
}

FontRenderPass::~FontRenderPass() { }

bool FontRenderPass::init(QueuePassBuilder &passBuilder, const AttachmentData *attachment) {
	passBuilder.addAttachment(attachment);

	if (!QueuePass::init(passBuilder)) {
		return false;
	}
	_fontAttachment = attachment;
	return true;
}

auto FontRenderPass::makeFrameHandle(const FrameQueue &handle) -> Rc<QueuePassHandle> {
	return Rc<FontRenderPassHandle>::create(*this, handle);
}

FontRenderPassHandle::~FontRenderPassHandle() { }

bool FontRenderPassHandle::init(QueuePass &pass, const FrameQueue &handle) {
	if (!QueuePassHandle::init(pass, handle)) {
		return false;
	}

	_queueOps = static_cast<vk::QueuePass *>(_queuePass.get())->getQueueOps();

	auto dev = static_cast<Device *>(handle.getFrame()->getDevice());
	auto q = dev->getQueueFamily(_queueOps);
	if (q->transferGranularity.width > 1 || q->transferGranularity.height > 1) {
		_queueOps = QueueOperations::Graphics;
		for (auto &it : dev->getQueueFamilies()) {
			if (it.index != q->index) {
				switch (it.preferred) {
				case QueueOperations::Compute:
				case QueueOperations::Transfer:
				case QueueOperations::Graphics:
					if ((it.transferGranularity.width == 1 || it.transferGranularity.height == 1) && toInt(_queueOps) < toInt(it.preferred)) {
						_queueOps = it.preferred;
					}
					break;
				default:
					break;
				}
			}
		}
	}

	return true;
}

QueueOperations FontRenderPassHandle::getQueueOps() const {
	return _queueOps;
}


bool FontRenderPassHandle::prepare(FrameQueue &handle, Function<void(bool)> &&cb) {
	if (auto a = handle.getAttachment(static_cast<FontRenderPass *>(_queuePass.get())->getRenderFontAttachment())) {
		_fontAttachment = static_cast<FontAttachmentHandle *>(a->handle.get());
	}
	return QueuePassHandle::prepare(handle, move(cb));
}

void FontRenderPassHandle::finalize(FrameQueue &handle, bool successful) {
	QueuePassHandle::finalize(handle, successful);
}

Vector<const CommandBuffer *> FontRenderPassHandle::doPrepareCommands(FrameHandle &handle) {
	Vector<VkCommandBuffer> ret;

	auto &input = _fontAttachment->getInput();
	auto &copyFromTmp = _fontAttachment->getCopyFromTmpBufferData();
	auto &copyFromPersistent = _fontAttachment->getCopyFromPersistentBufferData();
	auto &copyToPersistent = _fontAttachment->getCopyToPersistentBufferData();

	auto &masterImage = input->image;
	auto instance = masterImage->getInstance();
	if (!instance) {
		return Vector<const CommandBuffer *>();
	}

	auto atlas = _fontAttachment->getAtlas();

	core::ImageInfo info = masterImage->getInfo();

	info.format = core::ImageFormat::R8_UNORM;
	info.extent = _fontAttachment->getImageExtent();

	auto allocator = _device->getAllocator();

	if (_device->hasDynamicIndexedBuffers()) {
		_targetImage = allocator->preallocate(info, false, instance->data.image->getIndex());
		_targetAtlasIndex = allocator->preallocate(core::BufferInfo(atlas->getIndexData().size(), core::BufferUsage::StorageBuffer));
		_targetAtlasData = allocator->preallocate(core::BufferInfo(atlas->getData().size(), core::BufferUsage::StorageBuffer));

		Rc<Buffer> atlasBuffers[] = {
			_targetAtlasIndex,
			_targetAtlasData
		};

		allocator->emplaceObjects(AllocationUsage::DeviceLocal, makeSpanView(&_targetImage, 1), makeSpanView(atlasBuffers, 2));
	} else {
		_targetImage = allocator->spawnPersistent(AllocationUsage::DeviceLocal, info, false, instance->data.image->getIndex());
	}

	auto frame = static_cast<DeviceFrameHandle *>(&handle);
	auto &memPool = frame->getMemPool(&handle);

	Rc<Buffer> stageAtlasIndex, stageAtlasData;

	if (_targetAtlasIndex && _targetAtlasData) {
		stageAtlasIndex = memPool->spawn(AllocationUsage::HostTransitionSource,
				core::BufferInfo(atlas->getIndexData().size(), core::ForceBufferUsage(core::BufferUsage::TransferSrc)));
		stageAtlasData = memPool->spawn(AllocationUsage::HostTransitionSource,
				core::BufferInfo(atlas->getData().size(), core::ForceBufferUsage(core::BufferUsage::TransferSrc)));

		stageAtlasIndex->setData(atlas->getIndexData());
		stageAtlasData->setData(atlas->getData());
	}

	auto buf = _pool->recordBuffer(*_device, [&] (CommandBuffer &buf) {
		Vector<BufferMemoryBarrier> persistentBarriers;
		for (auto &it : copyFromPersistent) {
			if (auto b = it.first->getPendingBarrier()) {
				persistentBarriers.emplace_back(*b);
				it.first->dropPendingBarrier();
			}
		}

		ImageMemoryBarrier inputBarrier(_targetImage,
			0, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				persistentBarriers, makeSpanView(&inputBarrier, 1));

		// copy from temporary buffer
		if (!copyFromTmp.empty()) {
			buf.cmdCopyBufferToImage(_fontAttachment->getTmpBuffer(), _targetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copyFromTmp);
		}

		// copy from persistent buffers
		for (auto &it : copyFromPersistent) {
			buf.cmdCopyBufferToImage(it.first, _targetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, it.second);
		}

		if (!copyToPersistent.empty()) {
			buf.cmdCopyBuffer(_fontAttachment->getTmpBuffer(), _fontAttachment->getPersistentTargetBuffer(), copyToPersistent);
			_fontAttachment->getPersistentTargetBuffer()->setPendingBarrier(BufferMemoryBarrier(_fontAttachment->getPersistentTargetBuffer(),
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
				QueueFamilyTransfer(), 0, _fontAttachment->getPersistentTargetBuffer()->getReservedSize()
			));
		}

		if (_targetAtlasIndex && _targetAtlasData) {
			buf.cmdCopyBuffer(stageAtlasIndex, _targetAtlasIndex);
			buf.cmdCopyBuffer(stageAtlasData, _targetAtlasData);
		}

		auto sourceLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		if (auto q = _device->getQueueFamily(getQueueOperations(info.type))) {
			uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			if (q->index != _pool->getFamilyIdx()) {
				srcQueueFamilyIndex = _pool->getFamilyIdx();
				dstQueueFamilyIndex = q->index;
			}

			if (input->output) {
				auto extent = _targetImage->getInfo().extent;
				auto &frame = static_cast<DeviceFrameHandle &>(handle);
				auto memPool = frame.getMemPool(&handle);

				_outBuffer = memPool->spawn(AllocationUsage::HostTransitionDestination, core::BufferInfo(
					core::ForceBufferUsage(core::BufferUsage::TransferDst),
					size_t(extent.width * extent.height * extent.depth),
					core::PassType::Transfer
				));

				ImageMemoryBarrier reverseBarrier(_targetImage,
					VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
					sourceLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, makeSpanView(&reverseBarrier, 1));

				sourceLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				buf.cmdCopyImageToBuffer(_targetImage, sourceLayout, _outBuffer, 0);

				BufferMemoryBarrier bufferOutBarrier(_outBuffer,
					VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

				buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, makeSpanView(&bufferOutBarrier, 1));
			}

			ImageMemoryBarrier outputBarrier(_targetImage,
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				sourceLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				QueueFamilyTransfer{srcQueueFamilyIndex, dstQueueFamilyIndex});

			VkPipelineStageFlags targetOps = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
					| VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			auto ops = getQueueOps();
			switch (ops) {
			case QueueOperations::Transfer:
				targetOps = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
				break;
			case QueueOperations::Compute:
				targetOps = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				break;
			default:
				break;
			}

			if (q->index != _pool->getFamilyIdx()) {
				_targetImage->setPendingBarrier(outputBarrier);
			}

			if (_targetAtlasIndex && _targetAtlasData) {
				BufferMemoryBarrier outputBufferBarrier[] = {
					BufferMemoryBarrier(_targetAtlasIndex,
						VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
						QueueFamilyTransfer{srcQueueFamilyIndex, dstQueueFamilyIndex}, 0, _targetAtlasIndex->getSize()),
					BufferMemoryBarrier(_targetAtlasData,
						VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
						QueueFamilyTransfer{srcQueueFamilyIndex, dstQueueFamilyIndex}, 0, _targetAtlasData->getSize()),
				};

				if (q->index != _pool->getFamilyIdx()) {
					_targetAtlasIndex->setPendingBarrier(outputBufferBarrier[0]);
					_targetAtlasData->setPendingBarrier(outputBufferBarrier[1]);
				}

				buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, targetOps, 0,
						makeSpanView(outputBufferBarrier, 2), makeSpanView(&outputBarrier, 1));
			} else {
				buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, targetOps, 0,
						makeSpanView(&outputBarrier, 1));
			}
		}
		return true;
	});

	return Vector<const CommandBuffer *>{buf};
}

void FontRenderPassHandle::doSubmitted(FrameHandle &frame, Function<void(bool)> &&func, bool success, Rc<Fence> &&fence) {
	if (success) {
		auto &input = _fontAttachment->getInput();

		auto atlas = _fontAttachment->getAtlas();
		if (_device->hasDynamicIndexedBuffers()) {
			atlas->setIndexBuffer(_targetAtlasIndex);
			atlas->setDataBuffer(_targetAtlasData);
		}

		auto &sig = frame.getSignalDependencies();

		input->image->updateInstance(*frame.getLoop(), _targetImage, move(atlas),
				Rc<Ref>(_fontAttachment->getUserdata()), sig);

		if (input->output) {
			_outBuffer->map([&] (uint8_t *ptr, VkDeviceSize size) {
				input->output(_targetImage->getInfo(), BytesView(ptr, size));
			});
		}
	}

	vk::QueuePassHandle::doSubmitted(frame, move(func), success, move(fence));
	frame.signalDependencies(success);
}

void FontRenderPassHandle::doComplete(FrameQueue &queue, Function<void(bool)> &&func, bool success) {
	QueuePassHandle::doComplete(queue, move(func), success);
}

}

#endif
