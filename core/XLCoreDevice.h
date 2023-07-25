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

#ifndef XENOLITH_CORE_XLCOREDEVICE_H_
#define XENOLITH_CORE_XLCOREDEVICE_H_

#include "XLCoreQueueData.h"
#include "XLCoreObject.h"
#include "XLCoreImageStorage.h"

namespace stappler::xenolith::core {

class Instance;
class ObjectInterface;
class Loop;

class Device : public Ref {
public:
	using DescriptorType = core::DescriptorType;
	using ImageStorage = core::ImageStorage;

	Device();
	virtual ~Device();

	virtual bool init(const Instance *instance);
	virtual void end();

	Rc<Shader> getProgram(StringView);
	Rc<Shader> addProgram(Rc<Shader>);

	void addObject(ObjectInterface *);
	void removeObject(ObjectInterface *);

	uint32_t getSamplersCount() const { return _samplersCount; }
	bool isSamplersCompiled() const { return _samplersCompiled; }

	uint32_t getTextureLayoutImagesCount() const { return _textureLayoutImagesCount; }
	uint32_t getTextureLayoutBuffersCount() const { return _textureLayoutBuffersCount; }

	const Vector<ImageFormat> &getSupportedDepthStencilFormat() const { return _depthFormats; }
	const Vector<ImageFormat> &getSupportedColorFormat() const { return _colorFormats; }

	virtual void onLoopStarted(Loop &);
	virtual void onLoopEnded(Loop &);

	virtual bool supportsUpdateAfterBind(DescriptorType) const;

	virtual Rc<ImageObject> getEmptyImageObject() const = 0;
	virtual Rc<ImageObject> getSolidImageObject() const = 0;

	virtual Rc<Framebuffer> makeFramebuffer(const QueuePassData *, SpanView<Rc<ImageView>>);
	virtual Rc<ImageStorage> makeImage(const ImageInfoData &);
	virtual Rc<Semaphore> makeSemaphore();
	virtual Rc<ImageView> makeImageView(const Rc<ImageObject> &, const ImageViewInfo &);

	uint32_t getPresentatonMask() const { return _presentMask; }

	virtual void waitIdle() const { }

protected:
	friend class Loop;

	void clearShaders();
	void invalidateObjects();

	bool _started = false;
	const Instance *_glInstance = nullptr;
	Mutex _shaderMutex;
	Mutex _objectMutex;

	Map<String, Rc<Shader>> _shaders;

	std::unordered_set<ObjectInterface *> _objects;

	Vector<SamplerInfo> _samplersInfo;
	Vector<ImageFormat> _depthFormats;
	Vector<ImageFormat> _colorFormats;

	uint32_t _samplersCount = 0;
	bool _samplersCompiled = false;
	uint32_t _textureLayoutImagesCount = 0;
	uint32_t _textureLayoutBuffersCount = 0;

	std::thread::id _loopThreadId;
	uint32_t _presentMask = 0;
};

}

#endif /* XENOLITH_CORE_XLCOREDEVICE_H_ */
