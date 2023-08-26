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

#ifndef XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKSHADOW_H_
#define XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKSHADOW_H_

#include "XL2dVkMaterial.h"
#include "XLVkQueuePass.h"
#include "XL2dCommandList.h"

namespace stappler::xenolith::basic2d::vk {

class ShadowLightDataAttachment : public BufferAttachment {
public:
	virtual ~ShadowLightDataAttachment();

	virtual bool init(AttachmentBuilder &) override;

	virtual bool validateInput(const Rc<core::AttachmentInputData> &) const override;

protected:
	using BufferAttachment::init;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

// this attachment should provide vertex & index buffers
class ShadowVertexAttachment : public BufferAttachment {
public:
	virtual ~ShadowVertexAttachment();

	virtual bool init(AttachmentBuilder &) override;

	virtual bool validateInput(const Rc<core::AttachmentInputData> &) const override;

protected:
	using BufferAttachment::init;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

// this attachment should provide vertex & index buffers
class ShadowPrimitivesAttachment : public BufferAttachment {
public:
	virtual ~ShadowPrimitivesAttachment();

	virtual bool init(AttachmentBuilder &) override;

protected:
	using BufferAttachment::init;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class ShadowSdfImageAttachment : public ImageAttachment {
public:
	virtual ~ShadowSdfImageAttachment();

	virtual bool init(AttachmentBuilder &, Extent2 extent);

protected:
	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class ShadowVertexAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~ShadowVertexAttachmentHandle();

	virtual void submitInput(FrameQueue &, Rc<core::AttachmentInputData> &&, Function<void(bool)> &&) override;

	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
			uint32_t, bool isExternal) const override;

	virtual bool writeDescriptor(const core::QueuePassHandle &, DescriptorBufferInfo &) override;

	bool empty() const;

	uint32_t getTrianglesCount() const { return _trianglesCount; }
	uint32_t getCirclesCount() const { return _circlesCount; }
	uint32_t getRectsCount() const { return _rectsCount; }
	uint32_t getRoundedRectsCount() const { return _roundedRectsCount; }
	uint32_t getPolygonsCount() const { return _polygonsCount; }
	float getMaxValue() const { return _maxValue; }

	const Rc<DeviceBuffer> &getVertexes() const { return _vertexes; }

protected:
	virtual bool loadVertexes(FrameHandle &, const Rc<FrameContextHandle2d> &);

	Rc<DeviceBuffer> _indexes;
	Rc<DeviceBuffer> _vertexes;
	Rc<DeviceBuffer> _transforms;
	Rc<DeviceBuffer> _circles;
	Rc<DeviceBuffer> _rects;
	Rc<DeviceBuffer> _roundedRects;
	Rc<DeviceBuffer> _polygons;
	uint32_t _trianglesCount = 0;
	uint32_t _circlesCount = 0;
	uint32_t _rectsCount = 0;
	uint32_t _roundedRectsCount = 0;
	uint32_t _polygonsCount = 0;
	float _maxValue = 0.0f;
};

class ShadowLightDataAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~ShadowLightDataAttachmentHandle();

	virtual void submitInput(FrameQueue &, Rc<core::AttachmentInputData> &&, Function<void(bool)> &&) override;

	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
			uint32_t, bool isExternal) const override;

	virtual bool writeDescriptor(const core::QueuePassHandle &, DescriptorBufferInfo &) override;

	void allocateBuffer(DeviceFrameHandle *, const ShadowVertexAttachmentHandle *vertexes, uint32_t gridCells);

	float getBoxOffset(float value) const;

	uint32_t getLightsCount() const;
	uint32_t getObjectsCount() const;

	const ShadowData &getShadowData() const { return _shadowData; }
	const Rc<DeviceBuffer> &getBuffer() const { return _data; }

protected:
	Rc<DeviceBuffer> _data;
	Rc<FrameContextHandle2d> _input;
	ShadowData _shadowData;
};

class ShadowPrimitivesAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~ShadowPrimitivesAttachmentHandle();

	void allocateBuffer(DeviceFrameHandle *, uint32_t objects, const ShadowData &);

	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
			uint32_t, bool isExternal) const override;

	virtual bool writeDescriptor(const core::QueuePassHandle &, DescriptorBufferInfo &) override;

	const Rc<DeviceBuffer> &getTriangles() const { return _triangles; }
	const Rc<DeviceBuffer> &getCircles() const { return _circles; }
	const Rc<DeviceBuffer> &getRects() const { return _rects; }
	const Rc<DeviceBuffer> &getRoundedRects() const { return _roundedRects; }
	const Rc<DeviceBuffer> &getPolygons() const { return _polygons; }
	const Rc<DeviceBuffer> &getGridSize() const { return _gridSize; }
	const Rc<DeviceBuffer> &getGridIndex() const { return _gridIndex; }

protected:
	Rc<DeviceBuffer> _triangles;
	Rc<DeviceBuffer> _circles;
	Rc<DeviceBuffer> _rects;
	Rc<DeviceBuffer> _roundedRects;
	Rc<DeviceBuffer> _polygons;
	Rc<DeviceBuffer> _gridSize;
	Rc<DeviceBuffer> _gridIndex;
};

class ShadowSdfImageAttachmentHandle : public ImageAttachmentHandle {
public:
	virtual ~ShadowSdfImageAttachmentHandle() { }

	virtual void submitInput(FrameQueue &, Rc<core::AttachmentInputData> &&, Function<void(bool)> &&) override;

	ImageInfo getImageInfo() const { return _currentImageInfo; }
	float getShadowDensity() const { return _shadowDensity; }
	float getSceneDensity() const { return _sceneDensity; }

protected:
	float _sceneDensity = 1.0f;
	float _shadowDensity = 1.0f;
	ImageInfo _currentImageInfo;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKSHADOW_H_ */
