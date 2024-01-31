/**
Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_CORE_XLCORERESOURCE_H_
#define XENOLITH_CORE_XLCORERESOURCE_H_

#include "XLCoreInfo.h"
#include "XLCoreObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class Queue;

class Resource : public NamedRef {
public:
	class Builder;

	static void loadImageFileData(uint8_t *, uint64_t expectedSize, StringView path, ImageFormat fmt, const ImageData::DataCallback &dcb);

	Resource();
	virtual ~Resource();

	virtual bool init(Builder &&);

	void clear();

	bool isCompiled() const;
	void setCompiled(bool);

	const Queue *getOwner() const;
	void setOwner(const Queue *);

	virtual StringView getName() const override;
	memory::pool_t *getPool() const;

	const HashTable<BufferData *> &getBuffers() const;
	const HashTable<ImageData *> &getImages() const;

	const BufferData *getBuffer(StringView) const;
	const ImageData *getImage(StringView) const;

protected:
	friend class ResourceCache;

	struct ResourceData;

	ResourceData *_data = nullptr;
};

class Resource::Builder final {
public:
	Builder(StringView);
	~Builder();

	const BufferData * addBufferByRef(StringView key, BufferInfo &&, BytesView data,
			Rc<DataAtlas> &&atlas = Rc<DataAtlas>(), AccessType = AccessType::ShaderRead);
	const BufferData * addBuffer(StringView key, BufferInfo &&, FilePath data,
			Rc<DataAtlas> &&atlas = Rc<DataAtlas>(), AccessType = AccessType::ShaderRead);
	const BufferData * addBuffer(StringView key, BufferInfo &&, BytesView data,
			Rc<DataAtlas> &&atlas = Rc<DataAtlas>(), AccessType = AccessType::ShaderRead);
	const BufferData * addBuffer(StringView key, BufferInfo &&,
			const memory::function<void(uint8_t *, uint64_t, const BufferData::DataCallback &)> &cb,
			Rc<DataAtlas> &&atlas = Rc<DataAtlas>(), AccessType = AccessType::ShaderRead);

	const ImageData * addImageByRef(StringView key, ImageInfo &&, BytesView data,
			AttachmentLayout = AttachmentLayout::ShaderReadOnlyOptimal, AccessType = AccessType::ShaderRead);
	const ImageData * addImage(StringView key, ImageInfo &&img, FilePath data,
			AttachmentLayout = AttachmentLayout::ShaderReadOnlyOptimal, AccessType = AccessType::ShaderRead);
	const ImageData * addImage(StringView key, ImageInfo &&img, BytesView data,
			AttachmentLayout = AttachmentLayout::ShaderReadOnlyOptimal, AccessType = AccessType::ShaderRead);
	const ImageData * addImage(StringView key, ImageInfo &&img,
			const memory::function<void(uint8_t *, uint64_t, const ImageData::DataCallback &)> &cb,
			AttachmentLayout = AttachmentLayout::ShaderReadOnlyOptimal, AccessType = AccessType::ShaderRead);

	bool empty() const;

protected:
	friend class Resource;

	ResourceData *_data = nullptr;
};

}

#endif /* XENOLITH_CORE_XLCORERESOURCE_H_ */
