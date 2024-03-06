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

#ifndef XENOLITH_APPLICATION_XLRESOURCECACHE_H_
#define XENOLITH_APPLICATION_XLRESOURCECACHE_H_

#include "XLApplicationInfo.h"
#include "XLCoreResource.h"
#include "XLCoreLoop.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class TemporaryResource;
class Texture;
class MeshIndex;

enum class TemporaryResourceFlags {
	None = 0,
	Loaded = 1 << 0, // Resource is a wrapper around already loaded data (usually, when data created by GPU)
	RemoveOnClear = 1 << 1, // Remove whole resource from cache, when no one uses it
	CompileWhenAdded = 1 << 2, // Run resource compilation immediately after addTemporaryResource
};

SP_DEFINE_ENUM_AS_MASK(TemporaryResourceFlags)

class ResourceCache : public Ref {
public:
	virtual ~ResourceCache();

	bool init(Application *);

	void initialize(const core::Loop &);
	void invalidate();

	void update(const UpdateTime &);

	void addImage(StringView, const Rc<core::ImageObject> &);
	void addResource(const Rc<core::Resource> &);
	void removeResource(StringView);

	Application *getApplication() const { return _application; }

	Rc<Texture> acquireTexture(StringView) const;
	Rc<MeshIndex> acquireMeshIndex(StringView) const;

	const core::ImageData *getEmptyImage() const;
	const core::ImageData *getSolidImage() const;

	/*const gl::BufferData * addExternalBufferByRef(StringView key, gl::BufferInfo &&, BytesView data);
	const gl::BufferData * addExternalBuffer(StringView key, gl::BufferInfo &&, FilePath data);
	const gl::BufferData * addExternalBuffer(StringView key, gl::BufferInfo &&, BytesView data);
	const gl::BufferData * addExternalBuffer(StringView key, gl::BufferInfo &&, size_t,
			const memory::function<void(const gl::BufferData::DataCallback &)> &cb);*/

	Rc<Texture> addExternalImageByRef(StringView key, core::ImageInfo &&, BytesView data,
			TimeInterval = TimeInterval(), TemporaryResourceFlags flags = TemporaryResourceFlags::None);
	Rc<Texture> addExternalImage(StringView key, core::ImageInfo &&, FilePath data,
			TimeInterval = TimeInterval(), TemporaryResourceFlags flags = TemporaryResourceFlags::None);
	Rc<Texture> addExternalImage(StringView key, core::ImageInfo &&, BytesView data,
			TimeInterval = TimeInterval(), TemporaryResourceFlags flags = TemporaryResourceFlags::None);
	Rc<Texture> addExternalImage(StringView key, core::ImageInfo &&,
			const memory::function<void(uint8_t *, uint64_t, const core::ImageData::DataCallback &)> &cb,
			TimeInterval = TimeInterval(), TemporaryResourceFlags flags = TemporaryResourceFlags::None);

	Rc<TemporaryResource> addTemporaryResource(Rc<core::Resource> &&, TimeInterval = TimeInterval(),
			TemporaryResourceFlags flags = TemporaryResourceFlags::None);

	Rc<TemporaryResource> getTemporaryResource(StringView str) const;
	bool hasTemporaryResource(StringView) const;
	void removeTemporaryResource(StringView);

protected:
	void compileResource(TemporaryResource *);
	bool clearResource(TemporaryResource *);

	Application *_application = nullptr;
	const core::Loop *_loop = nullptr;
	Map<StringView, core::ImageData> _images;
	Map<StringView, Rc<core::Resource>> _resources;
	Map<StringView, Rc<TemporaryResource>> _temporaries;
};

}

#endif /* XENOLITH_APPLICATION_XLRESOURCECACHE_H_ */
