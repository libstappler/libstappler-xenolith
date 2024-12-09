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

#include "XLCorePipelineInfo.h"
#include "XLCorePlatform.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class BufferObject;
class ImageObject;
class DataAtlas;
class Resource;

using MipLevels = ValueWrapper<uint32_t, class MipLevelFlag>;
using ArrayLayers = ValueWrapper<uint32_t, class ArrayLayersFlag>;
using Extent1 = ValueWrapper<uint32_t, class Extent1Flag>;
using BaseArrayLayer = ValueWrapper<uint32_t, class BaseArrayLayerFlag>;

struct SP_PUBLIC SamplerInfo {
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

	SP_THREE_WAY_COMPARISON_TYPE(SamplerInfo)
};

using ForceBufferFlags = ValueWrapper<BufferFlags, class ForceBufferFlagsFlag>;
using ForceBufferUsage = ValueWrapper<BufferUsage, class ForceBufferUsageFlag>;
using BufferPersistent = ValueWrapper<bool, class BufferPersistentFlag>;

struct SP_PUBLIC BufferInfo : NamedMem {
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
	void setup(StringView n) { key = n; }

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

struct SP_PUBLIC BufferData : BufferInfo {
	using DataCallback = memory::callback<void(BytesView)>;

	BytesView data;
	memory::function<void(uint8_t *, uint64_t, const DataCallback &)> memCallback = nullptr;
	std::function<void(uint8_t *, uint64_t, const DataCallback &)> stdCallback = nullptr;
	Rc<BufferObject> buffer; // GL implementation-dependent object
	Rc<DataAtlas> atlas;
	const Resource *resource = nullptr; // owning resource;
	core::AccessType targetAccess = core::AccessType::ShaderRead;

	size_t writeData(uint8_t *mem, size_t expected) const;
};


using ForceImageFlags = ValueWrapper<ImageFlags, class ForceImageFlagsFlag>;
using ForceImageUsage = ValueWrapper<ImageUsage, class ForceImageUsageFlag>;

struct ImageViewInfo;

struct SP_PUBLIC ImageInfoData {
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

	ImageViewInfo getViewInfo(const ImageViewInfo &info) const;

#if SP_HAVE_THREE_WAY_COMPARISON
	SP_THREE_WAY_COMPARISON_TYPE(ImageInfoData)
#else
	constexpr bool operator==(const ImageInfoData &other) const {
		return format == other.format
			&& flags == other.flags
			&& imageType == other.imageType
			&& extent == other.extent
			&& mipLevels == other.mipLevels
			&& arrayLayers == other.arrayLayers
			&& samples == other.samples
			&& tiling == other.tiling
			&& usage == other.usage
			&& type == other.type
			&& hints == other.hints
			;
	}
	constexpr bool operator!=(const ImageInfoData &other) const {
		return format != other.format
			|| flags != other.flags
			|| imageType != other.imageType
			|| extent != other.extent
			|| mipLevels != other.mipLevels
			|| arrayLayers != other.arrayLayers
			|| samples != other.samples
			|| tiling != other.tiling
			|| usage != other.usage
			|| type != other.type
			|| hints != other.hints
			;
	}
	constexpr bool operator<(const ImageInfoData &other) const {
		if (format < other.format) { return true; } else if (format > other.format) { return false; }
		if (flags < other.flags) { return true; } else if (flags > other.flags) { return false; }
		if (imageType < other.imageType) { return true; } else if (imageType > other.imageType) { return false; }
		if (extent < other.extent) { return true; } else if (extent > other.extent) { return false; }
		if (mipLevels < other.mipLevels) { return true; } else if (mipLevels > other.mipLevels) { return false; }
		if (arrayLayers < other.arrayLayers) { return true; } else if (arrayLayers > other.arrayLayers) { return false; }
		if (samples < other.samples) { return true; } else if (samples > other.samples) { return false; }
		if (tiling < other.tiling) { return true; } else if (tiling > other.tiling) { return false; }
		if (usage < other.usage) { return true; } else if (usage > other.usage) { return false; }
		if (type < other.type) { return true; } else if (type > other.type) { return false; }
		if (hints < other.hints) { return true; } else if (hints > other.hints) { return false; }
		return false;
	}
#endif
};

struct SP_PUBLIC ImageInfo : NamedMem, ImageInfoData {
	ImageInfo() = default;

	template<typename ... Args>
	ImageInfo(Args && ... args) {
		define(std::forward<Args>(args)...);
	}

	void setup(Extent1 value) {
		extent = Extent3(value.get(), 1, 1);
	}
	void setup(Extent2 value) {
		extent = Extent3(value.width, value.height, 1);
	}
	void setup(Extent3 value) {
		extent = value;
		if (extent.depth > 1 && imageType != ImageType::Image3D) {
			imageType = ImageType::Image3D;
		}
	}
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

	String description() const;
};

struct SP_PUBLIC ImageData : ImageInfo {
	using DataCallback = memory::callback<void(BytesView)>;

	BytesView data;
	memory::function<void(uint8_t *, uint64_t, const DataCallback &)> memCallback = nullptr;
	std::function<void(uint8_t *, uint64_t, const DataCallback &)> stdCallback = nullptr;
	Rc<ImageObject> image; // GL implementation-dependent object
	Rc<DataAtlas> atlas;
	const Resource *resource = nullptr; // owning resource;
	core::AccessType targetAccess = core::AccessType::ShaderRead;
	core::AttachmentLayout targetLayout = core::AttachmentLayout::ShaderReadOnlyOptimal;

	size_t writeData(uint8_t *mem, size_t expected) const;
};


using ComponentMappingR = ValueWrapper<ComponentMapping, class ComponentMappingRFlag>;
using ComponentMappingG = ValueWrapper<ComponentMapping, class ComponentMappingGFlag>;
using ComponentMappingB = ValueWrapper<ComponentMapping, class ComponentMappingBFlag>;
using ComponentMappingA = ValueWrapper<ComponentMapping, class ComponentMappingAFlag>;

struct SP_PUBLIC ImageViewInfo {
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

#if SP_HAVE_THREE_WAY_COMPARISON
	SP_THREE_WAY_COMPARISON_TYPE(ImageViewInfo)
#else
	constexpr bool operator==(const ImageViewInfo &other) const {
		return format == other.format && type == other.type && r == other.r && g == other.g && b == other.b && a == other.a
				&& baseArrayLayer == other.baseArrayLayer && layerCount == other.layerCount;
	}
	constexpr bool operator!=(const ImageViewInfo &other) const {
		return format != other.format || type != other.type || r != other.r || g != other.g || b != other.b || a != other.a
				|| baseArrayLayer != other.baseArrayLayer || layerCount != other.layerCount;
	}
	constexpr bool operator<(const ImageViewInfo &other) const {
		if (format < other.format) { return true; } else if (format > other.format) { return false; }
		if (type < other.type) { return true; } else if (type > other.type) { return false; }
		if (r < other.r) { return true; } else if (r > other.r) { return false; }
		if (g < other.g) { return true; } else if (g > other.g) { return false; }
		if (b < other.b) { return true; } else if (b > other.b) { return false; }
		if (a < other.a) { return true; } else if (a > other.a) { return false; }
		if (baseArrayLayer < other.baseArrayLayer) { return true; } else if (baseArrayLayer > other.baseArrayLayer) { return false; }
		if (layerCount < other.layerCount) { return true; } else if (layerCount > other.layerCount) { return false; }
		return false;
	}
#endif
};

struct SP_PUBLIC FrameContraints {
	Extent3 extent;
	Padding contentPadding;
	SurfaceTransformFlags transform = SurfaceTransformFlags::Identity;
	float density = 1.0f;

	Size2 getScreenSize() const {
		if ((transform & core::SurfaceTransformFlags::PreRotated) != core::SurfaceTransformFlags::None) {
			switch (core::getPureTransform(transform)) {
			case SurfaceTransformFlags::Rotate90:
			case SurfaceTransformFlags::Rotate270:
			case SurfaceTransformFlags::MirrorRotate90:
			case SurfaceTransformFlags::MirrorRotate270:
				return Size2(extent.height, extent.width);
				break;
			default:
				break;
			}
		}
		return Size2(extent.width, extent.height);
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

	constexpr bool operator==(const FrameContraints &) const = default;
	constexpr bool operator!=(const FrameContraints &) const = default;
};

struct SP_PUBLIC SwapchainConfig {
	PresentMode presentMode = PresentMode::Mailbox;
	PresentMode presentModeFast = PresentMode::Unsupported;
	ImageFormat imageFormat = ImageFormat::R8G8B8A8_UNORM;
	ColorSpace colorSpace = ColorSpace::SRGB_NONLINEAR_KHR;
	CompositeAlphaFlags alpha = CompositeAlphaFlags::Opaque;
	SurfaceTransformFlags transform = SurfaceTransformFlags::Identity;
	uint32_t imageCount = 3;
	Extent2 extent;
	bool clipped = false;
	bool transfer = true;

	String description() const;

	constexpr bool operator==(const SwapchainConfig &) const = default;
	constexpr bool operator!=(const SwapchainConfig &) const = default;
};

struct SP_PUBLIC SurfaceInfo {
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

SP_PUBLIC String getBufferFlagsDescription(BufferFlags fmt);
SP_PUBLIC String getBufferUsageDescription(BufferUsage fmt);
SP_PUBLIC String getImageFlagsDescription(ImageFlags fmt);
SP_PUBLIC String getSampleCountDescription(SampleCount fmt);
SP_PUBLIC StringView getImageTypeName(ImageType type);
SP_PUBLIC StringView getImageViewTypeName(ImageViewType type);
SP_PUBLIC StringView getImageFormatName(ImageFormat fmt);
SP_PUBLIC StringView getImageTilingName(ImageTiling type);
SP_PUBLIC StringView getComponentMappingName(ComponentMapping);
SP_PUBLIC StringView getPresentModeName(PresentMode);
SP_PUBLIC StringView getColorSpaceName(ColorSpace);
SP_PUBLIC String getCompositeAlphaFlagsDescription(CompositeAlphaFlags);
SP_PUBLIC String getSurfaceTransformFlagsDescription(SurfaceTransformFlags);
SP_PUBLIC String getImageUsageDescription(ImageUsage fmt);
SP_PUBLIC size_t getFormatBlockSize(ImageFormat format);
SP_PUBLIC PixelFormat getImagePixelFormat(ImageFormat format);
SP_PUBLIC bool isStencilFormat(ImageFormat format);
SP_PUBLIC bool isDepthFormat(ImageFormat format);

SP_PUBLIC ImageViewType getImageViewType(ImageType, ArrayLayers);

SP_PUBLIC bool hasReadAccess(AccessType);
SP_PUBLIC bool hasWriteAccess(AccessType);

SP_PUBLIC std::ostream & operator<<(std::ostream &stream, const ImageInfoData &value);

}

#endif /* XENOLITH_CORE_XLCOREINFO_H_ */
