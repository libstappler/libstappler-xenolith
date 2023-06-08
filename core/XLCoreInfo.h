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

#ifndef XENOLITH_CORE_XLCOREINFO_H_
#define XENOLITH_CORE_XLCOREINFO_H_

#include "XLCore.h"
#include "XLCoreEnum.h"
#include "XLCorePlatform.h"

namespace stappler::xenolith::core {

class BufferObject;
class ImageObject;
class DataAtlas;
class Resource;

using MaterialId = uint32_t;
using StateId = uint32_t;

using MipLevels = ValueWrapper<uint32_t, class MipLevelFlag>;
using ArrayLayers = ValueWrapper<uint32_t, class ArrayLayersFlag>;
using Extent1 = ValueWrapper<uint32_t, class Extent1Flag>;
using BaseArrayLayer = ValueWrapper<uint32_t, class BaseArrayLayerFlag>;

struct SamplerInfo {
	Filter magFilter = Filter::Nearest;
	Filter minFilter = Filter::Nearest;
	SamplerMipmapMode mipmapMode = SamplerMipmapMode::Nearest;
	SamplerAddressMode addressModeU = SamplerAddressMode::Repeat;
	SamplerAddressMode addressModeV = SamplerAddressMode::Repeat;
	SamplerAddressMode addressModeW = SamplerAddressMode::Repeat;
	float mipLodBias = 0.0f;
	bool anisotropyEnable = false;
	float maxAnisotropy = 0.0f;
	bool compareEnable = false;
	CompareOp compareOp = CompareOp::Never;
	float minLod = 0.0;
	float maxLod = 0.0;

	bool operator==(const SamplerInfo &) const = default;
	bool operator!=(const SamplerInfo &) const = default;
	SP_THREE_WAY_COMPARISON_TYPE(SamplerInfo)
};

using ForceBufferFlags = ValueWrapper<BufferFlags, class ForceBufferFlagsFlag>;
using ForceBufferUsage = ValueWrapper<BufferUsage, class ForceBufferUsageFlag>;
using BufferPersistent = ValueWrapper<bool, class BufferPersistentFlag>;

struct BufferInfo : NamedMem {
	BufferFlags flags = BufferFlags::None;
	BufferUsage usage = BufferUsage::TransferDst;

	// on which type of RenderPass this buffer will be used (there is no universal usage, so, think carefully)
	PassType type = PassType::Graphics;
	uint64_t size = 0;
	bool persistent = true;

	BufferInfo() = default;

	template<typename ... Args>
	BufferInfo(Args && ... args) {
		define(std::forward<Args>(args)...);
	}

	void setup(const BufferInfo &value) { *this = value; }
	void setup(BufferFlags value) { flags |= value; }
	void setup(ForceBufferFlags value) { flags = value.get(); }
	void setup(BufferUsage value) { usage |= value; }
	void setup(ForceBufferUsage value) { usage = value.get(); }
	void setup(uint64_t value) { size = value; }
	void setup(BufferPersistent value) { persistent = value.get(); }
	void setup(PassType value) { type = value; }

	template <typename T>
	void define(T && t) {
		setup(std::forward<T>(t));
	}

	template <typename T, typename ... Args>
	void define(T && t, Args && ... args) {
		define(std::forward<T>(t));
		define(std::forward<Args>(args)...);
	}

	String description() const;
};

struct BufferData : BufferInfo {
	using DataCallback = memory::callback<void(BytesView)>;

	BytesView data;
	memory::function<void(uint8_t *, uint64_t, const DataCallback &)> callback = nullptr;
	Rc<BufferObject> buffer; // GL implementation-dependent object
	Rc<DataAtlas> atlas;
	const Resource *resource = nullptr; // owning resource;
};


using ForceImageFlags = ValueWrapper<ImageFlags, class ForceImageFlagsFlag>;
using ForceImageUsage = ValueWrapper<ImageUsage, class ForceImageUsageFlag>;

struct ImageViewInfo;

struct ImageInfoData {
	ImageFormat format = ImageFormat::Undefined;
	ImageFlags flags = ImageFlags::None;
	ImageType imageType = ImageType::Image2D;
	Extent3 extent = Extent3(1, 1, 1);
	MipLevels mipLevels = MipLevels(1);
	ArrayLayers arrayLayers = ArrayLayers(1);
	SampleCount samples = SampleCount::X1;
	ImageTiling tiling = ImageTiling::Optimal;
	ImageUsage usage = ImageUsage::TransferDst;

	// on which type of RenderPass this image will be used (there is no universal usage, so, think carefully)
	PassType type = PassType::Graphics;
	ImageHints hints = ImageHints::None;

	bool operator==(const ImageInfoData &) const = default;
	bool operator!=(const ImageInfoData &) const = default;
	SP_THREE_WAY_COMPARISON_TYPE(ImageInfoData)
};

struct ImageInfo : NamedMem, ImageInfoData {
	ImageInfo() = default;

	template<typename ... Args>
	ImageInfo(Args && ... args) {
		define(std::forward<Args>(args)...);
	}

	void setup(Extent1 value) { extent = Extent3(value.get(), 1, 1); }
	void setup(Extent2 value) { extent = Extent3(value.width, value.height, 1); }
	void setup(Extent3 value) { extent = value; }
	void setup(ImageFlags value) { flags |= value; }
	void setup(ForceImageFlags value) { flags = value.get(); }
	void setup(ImageType value) { imageType = value; }
	void setup(MipLevels value) { mipLevels = value; }
	void setup(ArrayLayers value) { arrayLayers = value; }
	void setup(SampleCount value) { samples = value; }
	void setup(ImageTiling value) { tiling = value; }
	void setup(ImageUsage value) { usage |= value; }
	void setup(ForceImageUsage value) { usage = value.get(); }
	void setup(ImageFormat value) { format = value; }
	void setup(PassType value) { type = value; }
	void setup(ImageHints value) { hints |= value; }
	void setup(StringView value) { key = value; }

	template <typename T>
	void define(T && t) {
		setup(std::forward<T>(t));
	}

	template <typename T, typename ... Args>
	void define(T && t, Args && ... args) {
		define(std::forward<T>(t));
		define(std::forward<Args>(args)...);
	}

	bool isCompatible(const ImageInfo &) const;

	ImageViewInfo getViewInfo(const ImageViewInfo &info) const;

	String description() const;
};

struct ImageData : ImageInfo {
	using DataCallback = memory::callback<void(BytesView)>;

	static ImageData make(Rc<ImageObject> &&);

	BytesView data;
	memory::function<void(uint8_t *, uint64_t, const DataCallback &)> memCallback = nullptr;
	std::function<void(uint8_t *, uint64_t, const DataCallback &)> stdCallback = nullptr;
	Rc<ImageObject> image; // GL implementation-dependent object
	Rc<DataAtlas> atlas;
	const Resource *resource = nullptr; // owning resource;
};


using ComponentMappingR = ValueWrapper<ComponentMapping, class ComponentMappingRFlag>;
using ComponentMappingG = ValueWrapper<ComponentMapping, class ComponentMappingGFlag>;
using ComponentMappingB = ValueWrapper<ComponentMapping, class ComponentMappingBFlag>;
using ComponentMappingA = ValueWrapper<ComponentMapping, class ComponentMappingAFlag>;

struct ImageViewInfo {
	ImageFormat format = ImageFormat::Undefined; // inherited from Image if undefined
	ImageViewType type = ImageViewType::ImageView2D;
	ComponentMapping r = ComponentMapping::Identity;
	ComponentMapping g = ComponentMapping::Identity;
	ComponentMapping b = ComponentMapping::Identity;
	ComponentMapping a = ComponentMapping::Identity;
	BaseArrayLayer baseArrayLayer = BaseArrayLayer(0);
	ArrayLayers layerCount = ArrayLayers::max();

	ImageViewInfo() = default;
	ImageViewInfo(const ImageViewInfo &) = default;
	ImageViewInfo(ImageViewInfo &&) = default;
	ImageViewInfo &operator=(const ImageViewInfo &) = default;
	ImageViewInfo &operator=(ImageViewInfo &&) = default;

	template<typename ... Args>
	ImageViewInfo(Args && ... args) {
		define(std::forward<Args>(args)...);
	}

	void setup(const ImageViewInfo &);
	void setup(const ImageInfoData &);
	void setup(ImageViewType value) { type = value; }
	void setup(ImageFormat value) { format = value; }
	void setup(ArrayLayers value) { layerCount = value; }
	void setup(BaseArrayLayer value) { baseArrayLayer = value; }
	void setup(ComponentMappingR value) { r = value.get(); }
	void setup(ComponentMappingG value) { g = value.get(); }
	void setup(ComponentMappingB value) { b = value.get(); }
	void setup(ComponentMappingA value) { a = value.get(); }
	void setup(ColorMode value, bool allowSwizzle = true);
	void setup(ImageType, ArrayLayers);

	ColorMode getColorMode() const;

	template <typename T>
	void define(T && t) {
		setup(std::forward<T>(t));
	}

	template <typename T, typename ... Args>
	void define(T && t, Args && ... args) {
		define(std::forward<T>(t));
		define(std::forward<Args>(args)...);
	}

	bool isCompatible(const ImageInfo &) const;
	String description() const;

	bool operator==(const ImageViewInfo &) const = default;
	bool operator!=(const ImageViewInfo &) const = default;
	SP_THREE_WAY_COMPARISON_TYPE(ImageViewInfo)
};

struct FrameContraints {
	Extent2 extent;
	Padding contentPadding;
	SurfaceTransformFlags transform = SurfaceTransformFlags::Identity;
	float density = 1.0f;

	Size2 getScreenSize() const {
		switch (transform) {
		case SurfaceTransformFlags::Rotate90:
		case SurfaceTransformFlags::Rotate270:
		case SurfaceTransformFlags::MirrorRotate90:
		case SurfaceTransformFlags::MirrorRotate270:
			return Size2(extent.height, extent.width);
			break;
		default:
			return extent;
			break;
		}
		return extent;
	}

	Padding getRotatedPadding() const {
		Padding out = contentPadding;
		switch (transform) {
		case SurfaceTransformFlags::Rotate90:
			out.left = contentPadding.top;
			out.top = contentPadding.right;
			out.right = contentPadding.bottom;
			out.bottom = contentPadding.left;
			break;
		case SurfaceTransformFlags::Rotate180:
			out.left = contentPadding.right;
			out.top = contentPadding.bottom;
			out.right = contentPadding.left;
			out.bottom = contentPadding.top;
			break;
		case SurfaceTransformFlags::Rotate270:
			out.left = contentPadding.bottom;
			out.top = contentPadding.left;
			out.right = contentPadding.top;
			out.bottom = contentPadding.right;
			break;
		case SurfaceTransformFlags::Mirror:
			out.left = contentPadding.right;
			out.right = contentPadding.left;
			break;
		case SurfaceTransformFlags::MirrorRotate90:
			break;
		case SurfaceTransformFlags::MirrorRotate180:
			out.top = contentPadding.bottom;
			out.bottom = contentPadding.top;
			break;
		case SurfaceTransformFlags::MirrorRotate270:
			break;
		default: break;
		}
		return out;
	}

	bool operator==(const FrameContraints &) const = default;
};

struct SwapchainConfig {
	PresentMode presentMode = PresentMode::Mailbox;
	PresentMode presentModeFast = PresentMode::Unsupported;
	ImageFormat imageFormat = platform::getCommonFormat();
	ColorSpace colorSpace = ColorSpace::SRGB_NONLINEAR_KHR;
	CompositeAlphaFlags alpha = CompositeAlphaFlags::Opaque;
	SurfaceTransformFlags transform = SurfaceTransformFlags::Identity;
	uint32_t imageCount = 3;
	Extent2 extent;
	bool clipped = false;
	bool transfer = true;

	String description() const;
};

struct SurfaceInfo {
	uint32_t minImageCount;
	uint32_t maxImageCount;
	Extent2 currentExtent;
	Extent2 minImageExtent;
	Extent2 maxImageExtent;
	uint32_t maxImageArrayLayers;
	CompositeAlphaFlags supportedCompositeAlpha;
	SurfaceTransformFlags supportedTransforms;
	SurfaceTransformFlags currentTransform;
	ImageUsage supportedUsageFlags;
	Vector<Pair<ImageFormat, ColorSpace>> formats;
	Vector<PresentMode> presentModes;
	float surfaceDensity = 1.0f;

	bool isSupported(const SwapchainConfig &) const;

	String description() const;
};

String getBufferFlagsDescription(BufferFlags fmt);
String getBufferUsageDescription(BufferUsage fmt);
String getImageFlagsDescription(ImageFlags fmt);
String getSampleCountDescription(SampleCount fmt);
StringView getImageTypeName(ImageType type);
StringView getImageViewTypeName(ImageViewType type);
StringView getImageFormatName(ImageFormat fmt);
StringView getImageTilingName(ImageTiling type);
StringView getComponentMappingName(ComponentMapping);
StringView getPresentModeName(PresentMode);
StringView getColorSpaceName(ColorSpace);
String getCompositeAlphaFlagsDescription(CompositeAlphaFlags);
String getSurfaceTransformFlagsDescription(SurfaceTransformFlags);
String getImageUsageDescription(ImageUsage fmt);
size_t getFormatBlockSize(ImageFormat format);
PixelFormat getImagePixelFormat(ImageFormat format);
bool isStencilFormat(ImageFormat format);
bool isDepthFormat(ImageFormat format);

std::ostream & operator<<(std::ostream &stream, const ImageInfoData &value);

}

#endif /* XENOLITH_CORE_XLCOREINFO_H_ */
