/**
Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_BACKEND_VK_XLVKTEXTURESET_H_
#define XENOLITH_BACKEND_VK_XLVKTEXTURESET_H_

#include "XLVkDevice.h"
#include "XLVkObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class TextureSet;
class Loop;

// persistent object, part of Device
class SP_PUBLIC TextureSetLayout : public Ref {
public:
	using AttachmentLayout = core::AttachmentLayout;

	virtual ~TextureSetLayout() { }

	bool init(Device &dev, uint32_t imageCount, uint32_t bufferLimit);
	void invalidate(Device &dev);

	bool compile(Device &dev, const Vector<VkSampler> &);

	const uint32_t &getImageCount() const { return _imageCount; }
	const uint32_t &getSamplersCount() const { return _samplersCount; }
	const uint32_t &getBuffersCount() const { return _bufferCount; }
	VkDescriptorSetLayout getLayout() const { return _layout; }
	const Rc<ImageView> &getEmptyImageView() const { return _emptyImageView; }
	const Rc<ImageView> &getSolidImageView() const { return _solidImageView; }
	const Rc<Buffer> &getEmptyBuffer() const { return _emptyBuffer; }

	Rc<TextureSet> acquireSet(Device &dev);
	void releaseSet(Rc<TextureSet> &&);

	void initDefault(Device &dev, Loop &, Function<void(bool)> &&);

	bool isPartiallyBound() const { return _partiallyBound; }

	Rc<Image> getEmptyImageObject() const;
	Rc<Image> getSolidImageObject() const;

	void readImage(Device &dev, Loop &loop, const Rc<Image> &, AttachmentLayout, Function<void(const ImageInfoData &, BytesView)> &&);
	void readBuffer(Device &dev, Loop &loop, const Rc<Buffer> &, Function<void(const BufferInfo &, BytesView)> &&);

protected:
	void writeDefaults(CommandBuffer &buf);
	void writeImageTransfer(Device &dev, CommandBuffer &buf, uint32_t, const Rc<Buffer> &, const Rc<Image> &);
	void writeImageRead(Device &dev, CommandBuffer &buf, uint32_t qidx, const Rc<Image> &,
			AttachmentLayout, const Rc<Buffer> &);
	void writeBufferRead(Device &dev, CommandBuffer &buf, uint32_t qidx, const Rc<Buffer> &,
			const Rc<Buffer> &);

	bool _partiallyBound = false;
	uint32_t _imageCount = 0;
	uint32_t _bufferCount = 0;
	uint32_t _samplersCount = 0;
	VkDescriptorSetLayout _layout = VK_NULL_HANDLE;

	Rc<Image> _emptyImage;
	Rc<ImageView> _emptyImageView;
	Rc<Image> _solidImage;
	Rc<ImageView> _solidImageView;
	Rc<Buffer> _emptyBuffer;

	mutable Mutex _mutex;
	Vector<Rc<TextureSet>> _sets;
};

class SP_PUBLIC TextureSet : public core::TextureSet {
public:
	virtual ~TextureSet() { }

	bool init(Device &dev, const TextureSetLayout &);

	VkDescriptorSet getSet() const { return _set; }

	virtual void write(const core::MaterialLayout &) override;

	Vector<const ImageMemoryBarrier *> getPendingImageBarriers() const;
	Vector<const BufferMemoryBarrier *> getPendingBufferBarriers() const;

	void foreachPendingImageBarriers(const Callback<void(const ImageMemoryBarrier &)> &) const;
	void foreachPendingBufferBarriers(const Callback<void(const BufferMemoryBarrier &)> &) const;

	void dropPendingBarriers();

	Device *getDevice() const;

protected:
	using core::TextureSet::init;

	void writeImages(Vector<VkWriteDescriptorSet> &writes, const core::MaterialLayout &set,
			std::forward_list<Vector<VkDescriptorImageInfo>> &imagesList);
	void writeBuffers(Vector<VkWriteDescriptorSet> &writes, const core::MaterialLayout &set,
			std::forward_list<Vector<VkDescriptorBufferInfo>> &bufferList);

	bool _partiallyBound = false;
	const TextureSetLayout *_layout = nullptr;
	uint32_t _imageCount = 0;
	uint32_t _bufferCount = 0;
	VkDescriptorSet _set = VK_NULL_HANDLE;
	VkDescriptorPool _pool = VK_NULL_HANDLE;
	Vector<Image *> _pendingImageBarriers;
	Vector<Buffer *> _pendingBufferBarriers;
};

}

#endif /* XENOLITH_BACKEND_VK_XLVKTEXTURESET_H_ */
