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

#ifndef XENOLITH_CORE_XLCOREENUM_H_
#define XENOLITH_CORE_XLCOREENUM_H_

#include "SPCommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

enum class FrameRenderPassState {
	Initial,
	Ready,
	ResourcesAcquired,
	Prepared,
	Submission,
	Submitted,
	Complete,
	Finalized,
};

enum class FrameAttachmentState {
	Initial,
	Setup,
	InputRequired,
	Ready,
	ResourcesPending,
	ResourcesAcquired,
	Detached, // resource ownership transferred out of Frame
	Complete,
	ResourcesReleased,
	Finalized,
};

enum class AttachmentType {
	Image,
	Buffer,
	Generic
};

// VkPipelineStageFlagBits
enum class PipelineStage {
	None,
	TopOfPipe = 0x00000001,
	DrawIndirect = 0x00000002,
	VertexInput = 0x00000004,
	VertexShader = 0x00000008,
	TesselationControl = 0x00000010,
	TesselationEvaluation = 0x00000020,
	GeometryShader = 0x00000040,
	FragmentShader = 0x00000080,
	EarlyFragmentTest = 0x00000100,
	LateFragmentTest = 0x00000200,
	ColorAttachmentOutput = 0x00000400,
	ComputeShader = 0x00000800,
	Transfer = 0x00001000,
	BottomOfPipe = 0x00002000,
	Host = 0x00004000,
	AllGraphics = 0x00008000,
	AllCommands = 0x00010000,
	TransformFeedback = 0x01000000,
	ConditionalRendering = 0x00040000,
	AccelerationStructureBuild = 0x02000000,
	RayTracingShader = 0x00200000,
	ShadingRateImage = 0x00400000,
	TaskShader = 0x00080000,
	MeshShader = 0x00100000,
	FragmentDensityProcess = 0x00800000,
	CommandPreprocess = 0x00020000,
};

SP_DEFINE_ENUM_AS_MASK(PipelineStage);

// VkAccessFlag
enum class AccessType {
	None,
    IndirectCommandRead = 0x00000001,
    IndexRead = 0x00000002,
    VertexAttributeRead = 0x00000004,
    UniformRead = 0x00000008,
    InputAttachmantRead = 0x00000010,
    ShaderRead = 0x00000020,
	ShaderWrite = 0x00000040,
    ColorAttachmentRead = 0x00000080,
    ColorAttachmentWrite = 0x00000100,
    DepthStencilAttachmentRead = 0x00000200,
	DepthStencilAttachmentWrite = 0x00000400,
    TransferRead = 0x00000800,
    TransferWrite = 0x00001000,
    HostRead = 0x00002000,
    HostWrite = 0x00004000,
    MemoryRead = 0x00008000,
    MemoryWrite = 0x00010000,
	TransformFeedbackWrite = 0x02000000,
	TransformFeedbackCounterRead = 0x04000000,
	TransformFeedbackCounterWrite = 0x08000000,
	ConditionalRenderingRead = 0x00100000,
	ColorAttachmentReadNonCoherent = 0x00080000,
	AccelerationStructureRead = 0x00200000,
	AccelerationStructureWrite = 0x00400000,
	ShadingRateImageRead = 0x00800000,
	FragmentDensityMapRead = 0x01000000,
	CommandPreprocessRead = 0x00020000,
	CommandPreprocessWrite = 0x00040000,
};

SP_DEFINE_ENUM_AS_MASK(AccessType);

// read-write operations on attachment within passes
enum class AttachmentOps {
	Undefined,
	ReadColor = 1,
	ReadStencil = 2,
	WritesColor = 4,
	WritesStencil = 8
};

SP_DEFINE_ENUM_AS_MASK(AttachmentOps);

// VkAttachmentLoadOp
enum class AttachmentLoadOp {
	Load = 0,
	Clear = 1,
	DontCare = 2,
};

// VkAttachmentStoreOp
enum class AttachmentStoreOp {
	Store = 0,
	DontCare = 1,
};

// Attachment usage within subpasses
enum class AttachmentUsage {
	None,
	Input = 1,
	Output = 2,
	InputOutput = Input | Output,
	Resolve = 4,
	DepthStencil = 8,
	InputDepthStencil = Input | DepthStencil
};

SP_DEFINE_ENUM_AS_MASK(AttachmentUsage);

// VkDescriptorType
enum class DescriptorType : uint32_t {
	Sampler = 0,
	CombinedImageSampler = 1,
	SampledImage = 2,
	StorageImage = 3,
	UniformTexelBuffer = 4,
	StorageTexelBuffer = 5,
	UniformBuffer = 6,
	StorageBuffer = 7,
	UniformBufferDynamic = 8,
	StorageBufferDynamic = 9,
	InputAttachment = 10,
	Attachment = maxOf<uint32_t>() - 1,
	Unknown = maxOf<uint32_t>()
};

// mapping to VkShaderStageFlagBits
enum class ProgramStage {
	None,
	Vertex = 0x00000001,
	TesselationControl = 0x00000002,
	TesselationEvaluation = 0x00000004,
	Geometry = 0x00000008,
	Fragment = 0x00000010,
	Compute = 0x00000020,
	RayGen = 0x00000100,
	AnyHit = 0x00000200,
	ClosestHit = 0x00000400,
	MissHit = 0x00000800,
	Intersection = 0x00001000,
	Callable = 0x00002000,
	Task = 0x00000040,
	Mesh = 0x00000080,
};

SP_DEFINE_ENUM_AS_MASK(ProgramStage);

// mapping to VkImageLayout
enum class AttachmentLayout : uint32_t {
	Undefined = 0,
	General = 1,
	ColorAttachmentOptimal = 2,
	DepthStencilAttachmentOptimal = 3,
	DepthStencilReadOnlyOptimal = 4,
	ShaderReadOnlyOptimal = 5,
	TransferSrcOptimal = 6,
	TransferDstOptimal = 7,
	Preinitialized = 8,
	DepthReadOnlyStencilAttachmentOptimal = 1000117000,
	DepthAttachmentStencilReadOnlyOptimal = 1000117001,
	DepthAttachmentOptimal = 1000241000,
	DepthReadOnlyOptimal = 1000241001,
	StencilAttachmentOptimal = 1000241002,
	StencilReadOnlyOptimal = 1000241003,
	PresentSrc = 1000001002,
	Ignored = maxOf<uint32_t>()
};

enum class PassType {
	Graphics,
	Compute,
	Transfer,
	Generic
};

// engine-defined specialization constants for shaders
enum class PredefinedConstant {
	SamplersArraySize,
	SamplersDescriptorIdx,
	TexturesArraySize,
	TexturesDescriptorIdx,
	BuffersArraySize,
	BuffersDescriptorIdx,
	CurrentSamplerIdx,
};

struct SpecializationConstant {
	enum Type {
		Int,
		Float,
		Predefined
	};

	Type type = Int;
	union {
		int intValue;
		float floatValue;
		PredefinedConstant predefinedValue;
	};

	SpecializationConstant(int val) : type(Int), intValue(val) { }
	SpecializationConstant(uint32_t val) : type(Int), intValue(val) { }
	SpecializationConstant(float val) : type(Float), floatValue(val) { }
	SpecializationConstant(PredefinedConstant val) : type(Predefined), predefinedValue(val) { }
};

enum class DynamicState {
	None,
	Viewport = 1,
	Scissor = 2,

	Default = Viewport | Scissor,
};

SP_DEFINE_ENUM_AS_MASK(DynamicState)

// Mapping to VkBufferCreateFlagBits
enum class BufferFlags {
	None,
	SparceBinding = 0x00000001,
	SparceResidency = 0x00000002,
	SparceAliased = 0x00000004,
	Protected = 0x00000008,
};

SP_DEFINE_ENUM_AS_MASK(BufferFlags)


// Mapping to VkBufferUsageFlagBits
enum class BufferUsage {
	None,
	TransferSrc = 0x00000001,
	TransferDst = 0x00000002,
	UniformTexelBuffer = 0x00000004,
	StorageTexelBuffer = 0x00000008,
	UniformBuffer = 0x00000010,
	StorageBuffer = 0x00000020,
	IndexBuffer = 0x00000040,
	VertexBuffer = 0x00000080,
	IndirectBuffer = 0x00000100,
	ShaderDeviceAddress = 0x00020000,

	TransformFeedback = 0x00000800,
	TransformFeedbackCounter = 0x00001000,
	ConditionalRendering = 0x00000200,
	AccelerationStructureBuildInputReadOnly = 0x00080000,
	AccelerationStructureStorage = 0x00100000,
	ShaderBindingTable = 0x00000400,
};

SP_DEFINE_ENUM_AS_MASK(BufferUsage)


// Mapping to VkImageCreateFlagBits
enum ImageFlags {
	None,
	SparceBinding = 0x00000001,
	SparceResidency = 0x00000002,
	SparceAliased = 0x00000004,
	MutableFormat = 0x00000008,
	CubeCompatible = 0x00000010,
	Alias = 0x00000400,
	SplitInstanceBindRegions = 0x00000040,
	Array2dCompatible = 0x00000020,
	BlockTexelViewCompatible = 0x00000080,
	ExtendedUsage = 0x00000100,
	Protected = 0x00000800,
	Disjoint = 0x00000200,
};

SP_DEFINE_ENUM_AS_MASK(ImageFlags)


// Mapping to VkSampleCountFlagBits
enum class SampleCount {
	None,
	X1 = 0x00000001,
	X2 = 0x00000002,
	X4 = 0x00000004,
	X8 = 0x00000008,
	X16 = 0x00000010,
	X32 = 0x00000020,
	X64 = 0x00000040,
};

SP_DEFINE_ENUM_AS_MASK(SampleCount)


// Mapping to VkImageType
enum class ImageType {
	Image1D = 0,
	Image2D = 1,
	Image3D = 2,
};

// Mapping to VkImageViewType
enum class ImageViewType {
    ImageView1D = 0,
    ImageView2D = 1,
	ImageView3D = 2,
	ImageViewCube = 3,
	ImageView1DArray = 4,
	ImageView2DArray = 5,
	ImageViewCubeArray = 6,
};

// Mapping to VkFormat
enum class ImageFormat {
	Undefined = 0,
	R4G4_UNORM_PACK8 = 1,
	R4G4B4A4_UNORM_PACK16 = 2,
	B4G4R4A4_UNORM_PACK16 = 3,
	R5G6B5_UNORM_PACK16 = 4,
	B5G6R5_UNORM_PACK16 = 5,
	R5G5B5A1_UNORM_PACK16 = 6,
	B5G5R5A1_UNORM_PACK16 = 7,
	A1R5G5B5_UNORM_PACK16 = 8,
	R8_UNORM = 9,
	R8_SNORM = 10,
	R8_USCALED = 11,
	R8_SSCALED = 12,
	R8_UINT = 13,
	R8_SINT = 14,
	R8_SRGB = 15,
	R8G8_UNORM = 16,
	R8G8_SNORM = 17,
	R8G8_USCALED = 18,
	R8G8_SSCALED = 19,
	R8G8_UINT = 20,
	R8G8_SINT = 21,
	R8G8_SRGB = 22,
	R8G8B8_UNORM = 23,
	R8G8B8_SNORM = 24,
	R8G8B8_USCALED = 25,
	R8G8B8_SSCALED = 26,
	R8G8B8_UINT = 27,
	R8G8B8_SINT = 28,
	R8G8B8_SRGB = 29,
	B8G8R8_UNORM = 30,
	B8G8R8_SNORM = 31,
	B8G8R8_USCALED = 32,
	B8G8R8_SSCALED = 33,
	B8G8R8_UINT = 34,
	B8G8R8_SINT = 35,
	B8G8R8_SRGB = 36,
	R8G8B8A8_UNORM = 37,
	R8G8B8A8_SNORM = 38,
	R8G8B8A8_USCALED = 39,
	R8G8B8A8_SSCALED = 40,
	R8G8B8A8_UINT = 41,
	R8G8B8A8_SINT = 42,
	R8G8B8A8_SRGB = 43,
	B8G8R8A8_UNORM = 44,
	B8G8R8A8_SNORM = 45,
	B8G8R8A8_USCALED = 46,
	B8G8R8A8_SSCALED = 47,
	B8G8R8A8_UINT = 48,
	B8G8R8A8_SINT = 49,
	B8G8R8A8_SRGB = 50,
	A8B8G8R8_UNORM_PACK32 = 51,
	A8B8G8R8_SNORM_PACK32 = 52,
	A8B8G8R8_USCALED_PACK32 = 53,
	A8B8G8R8_SSCALED_PACK32 = 54,
	A8B8G8R8_UINT_PACK32 = 55,
	A8B8G8R8_SINT_PACK32 = 56,
	A8B8G8R8_SRGB_PACK32 = 57,
	A2R10G10B10_UNORM_PACK32 = 58,
	A2R10G10B10_SNORM_PACK32 = 59,
	A2R10G10B10_USCALED_PACK32 = 60,
	A2R10G10B10_SSCALED_PACK32 = 61,
	A2R10G10B10_UINT_PACK32 = 62,
	A2R10G10B10_SINT_PACK32 = 63,
	A2B10G10R10_UNORM_PACK32 = 64,
	A2B10G10R10_SNORM_PACK32 = 65,
	A2B10G10R10_USCALED_PACK32 = 66,
	A2B10G10R10_SSCALED_PACK32 = 67,
	A2B10G10R10_UINT_PACK32 = 68,
	A2B10G10R10_SINT_PACK32 = 69,
	R16_UNORM = 70,
	R16_SNORM = 71,
	R16_USCALED = 72,
	R16_SSCALED = 73,
	R16_UINT = 74,
	R16_SINT = 75,
	R16_SFLOAT = 76,
	R16G16_UNORM = 77,
	R16G16_SNORM = 78,
	R16G16_USCALED = 79,
	R16G16_SSCALED = 80,
	R16G16_UINT = 81,
	R16G16_SINT = 82,
	R16G16_SFLOAT = 83,
	R16G16B16_UNORM = 84,
	R16G16B16_SNORM = 85,
	R16G16B16_USCALED = 86,
	R16G16B16_SSCALED = 87,
	R16G16B16_UINT = 88,
	R16G16B16_SINT = 89,
	R16G16B16_SFLOAT = 90,
	R16G16B16A16_UNORM = 91,
	R16G16B16A16_SNORM = 92,
	R16G16B16A16_USCALED = 93,
	R16G16B16A16_SSCALED = 94,
	R16G16B16A16_UINT = 95,
	R16G16B16A16_SINT = 96,
	R16G16B16A16_SFLOAT = 97,
	R32_UINT = 98,
	R32_SINT = 99,
	R32_SFLOAT = 100,
	R32G32_UINT = 101,
	R32G32_SINT = 102,
	R32G32_SFLOAT = 103,
	R32G32B32_UINT = 104,
	R32G32B32_SINT = 105,
	R32G32B32_SFLOAT = 106,
	R32G32B32A32_UINT = 107,
	R32G32B32A32_SINT = 108,
	R32G32B32A32_SFLOAT = 109,
	R64_UINT = 110,
	R64_SINT = 111,
	R64_SFLOAT = 112,
	R64G64_UINT = 113,
	R64G64_SINT = 114,
	R64G64_SFLOAT = 115,
	R64G64B64_UINT = 116,
	R64G64B64_SINT = 117,
	R64G64B64_SFLOAT = 118,
	R64G64B64A64_UINT = 119,
	R64G64B64A64_SINT = 120,
	R64G64B64A64_SFLOAT = 121,
	B10G11R11_UFLOAT_PACK32 = 122,
	E5B9G9R9_UFLOAT_PACK32 = 123,
	D16_UNORM = 124,
	X8_D24_UNORM_PACK32 = 125,
	D32_SFLOAT = 126,
	S8_UINT = 127,
	D16_UNORM_S8_UINT = 128,
	D24_UNORM_S8_UINT = 129,
	D32_SFLOAT_S8_UINT = 130,
	BC1_RGB_UNORM_BLOCK = 131,
	BC1_RGB_SRGB_BLOCK = 132,
	BC1_RGBA_UNORM_BLOCK = 133,
	BC1_RGBA_SRGB_BLOCK = 134,
	BC2_UNORM_BLOCK = 135,
	BC2_SRGB_BLOCK = 136,
	BC3_UNORM_BLOCK = 137,
	BC3_SRGB_BLOCK = 138,
	BC4_UNORM_BLOCK = 139,
	BC4_SNORM_BLOCK = 140,
	BC5_UNORM_BLOCK = 141,
	BC5_SNORM_BLOCK = 142,
	BC6H_UFLOAT_BLOCK = 143,
	BC6H_SFLOAT_BLOCK = 144,
	BC7_UNORM_BLOCK = 145,
	BC7_SRGB_BLOCK = 146,
	ETC2_R8G8B8_UNORM_BLOCK = 147,
	ETC2_R8G8B8_SRGB_BLOCK = 148,
	ETC2_R8G8B8A1_UNORM_BLOCK = 149,
	ETC2_R8G8B8A1_SRGB_BLOCK = 150,
	ETC2_R8G8B8A8_UNORM_BLOCK = 151,
	ETC2_R8G8B8A8_SRGB_BLOCK = 152,
	EAC_R11_UNORM_BLOCK = 153,
	EAC_R11_SNORM_BLOCK = 154,
	EAC_R11G11_UNORM_BLOCK = 155,
	EAC_R11G11_SNORM_BLOCK = 156,
	ASTC_4x4_UNORM_BLOCK = 157,
	ASTC_4x4_SRGB_BLOCK = 158,
	ASTC_5x4_UNORM_BLOCK = 159,
	ASTC_5x4_SRGB_BLOCK = 160,
	ASTC_5x5_UNORM_BLOCK = 161,
	ASTC_5x5_SRGB_BLOCK = 162,
	ASTC_6x5_UNORM_BLOCK = 163,
	ASTC_6x5_SRGB_BLOCK = 164,
	ASTC_6x6_UNORM_BLOCK = 165,
	ASTC_6x6_SRGB_BLOCK = 166,
	ASTC_8x5_UNORM_BLOCK = 167,
	ASTC_8x5_SRGB_BLOCK = 168,
	ASTC_8x6_UNORM_BLOCK = 169,
	ASTC_8x6_SRGB_BLOCK = 170,
	ASTC_8x8_UNORM_BLOCK = 171,
	ASTC_8x8_SRGB_BLOCK = 172,
	ASTC_10x5_UNORM_BLOCK = 173,
	ASTC_10x5_SRGB_BLOCK = 174,
	ASTC_10x6_UNORM_BLOCK = 175,
	ASTC_10x6_SRGB_BLOCK = 176,
	ASTC_10x8_UNORM_BLOCK = 177,
	ASTC_10x8_SRGB_BLOCK = 178,
	ASTC_10x10_UNORM_BLOCK = 179,
	ASTC_10x10_SRGB_BLOCK = 180,
	ASTC_12x10_UNORM_BLOCK = 181,
	ASTC_12x10_SRGB_BLOCK = 182,
	ASTC_12x12_UNORM_BLOCK = 183,
	ASTC_12x12_SRGB_BLOCK = 184,
	G8B8G8R8_422_UNORM = 1000156000,
	B8G8R8G8_422_UNORM = 1000156001,
	G8_B8_R8_3PLANE_420_UNORM = 1000156002,
	G8_B8R8_2PLANE_420_UNORM = 1000156003,
	G8_B8_R8_3PLANE_422_UNORM = 1000156004,
	G8_B8R8_2PLANE_422_UNORM = 1000156005,
	G8_B8_R8_3PLANE_444_UNORM = 1000156006,
	R10X6_UNORM_PACK16 = 1000156007,
	R10X6G10X6_UNORM_2PACK16 = 1000156008,
	R10X6G10X6B10X6A10X6_UNORM_4PACK16 = 1000156009,
	G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 = 1000156010,
	B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 = 1000156011,
	G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 = 1000156012,
	G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 = 1000156013,
	G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 = 1000156014,
	G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 = 1000156015,
	G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 = 1000156016,
	R12X4_UNORM_PACK16 = 1000156017,
	R12X4G12X4_UNORM_2PACK16 = 1000156018,
	R12X4G12X4B12X4A12X4_UNORM_4PACK16 = 1000156019,
	G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 = 1000156020,
	B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 = 1000156021,
	G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 = 1000156022,
	G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 = 1000156023,
	G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 = 1000156024,
	G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 = 1000156025,
	G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 = 1000156026,
	G16B16G16R16_422_UNORM = 1000156027,
	B16G16R16G16_422_UNORM = 1000156028,
	G16_B16_R16_3PLANE_420_UNORM = 1000156029,
	G16_B16R16_2PLANE_420_UNORM = 1000156030,
	G16_B16_R16_3PLANE_422_UNORM = 1000156031,
	G16_B16R16_2PLANE_422_UNORM = 1000156032,
	G16_B16_R16_3PLANE_444_UNORM = 1000156033,
	PVRTC1_2BPP_UNORM_BLOCK_IMG = 1000054000,
	PVRTC1_4BPP_UNORM_BLOCK_IMG = 1000054001,
	PVRTC2_2BPP_UNORM_BLOCK_IMG = 1000054002,
	PVRTC2_4BPP_UNORM_BLOCK_IMG = 1000054003,
	PVRTC1_2BPP_SRGB_BLOCK_IMG = 1000054004,
	PVRTC1_4BPP_SRGB_BLOCK_IMG = 1000054005,
	PVRTC2_2BPP_SRGB_BLOCK_IMG = 1000054006,
	PVRTC2_4BPP_SRGB_BLOCK_IMG = 1000054007,
	ASTC_4x4_SFLOAT_BLOCK_EXT = 1000066000,
	ASTC_5x4_SFLOAT_BLOCK_EXT = 1000066001,
	ASTC_5x5_SFLOAT_BLOCK_EXT = 1000066002,
	ASTC_6x5_SFLOAT_BLOCK_EXT = 1000066003,
	ASTC_6x6_SFLOAT_BLOCK_EXT = 1000066004,
	ASTC_8x5_SFLOAT_BLOCK_EXT = 1000066005,
	ASTC_8x6_SFLOAT_BLOCK_EXT = 1000066006,
	ASTC_8x8_SFLOAT_BLOCK_EXT = 1000066007,
	ASTC_10x5_SFLOAT_BLOCK_EXT = 1000066008,
	ASTC_10x6_SFLOAT_BLOCK_EXT = 1000066009,
	ASTC_10x8_SFLOAT_BLOCK_EXT = 1000066010,
	ASTC_10x10_SFLOAT_BLOCK_EXT = 1000066011,
	ASTC_12x10_SFLOAT_BLOCK_EXT = 1000066012,
	ASTC_12x12_SFLOAT_BLOCK_EXT = 1000066013,
	G8_B8R8_2PLANE_444_UNORM_EXT = 1000330000,
	G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT = 1000330001,
	G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT = 1000330002,
	G16_B16R16_2PLANE_444_UNORM_EXT = 1000330003,
	A4R4G4B4_UNORM_PACK16_EXT = 1000340000,
	A4B4G4R4_UNORM_PACK16_EXT = 1000340001,
};

// VkColorSpaceKHR
enum class ColorSpace {
	SRGB_NONLINEAR_KHR = 0,
	DISPLAY_P3_NONLINEAR_EXT = 1000104001,
	EXTENDED_SRGB_LINEAR_EXT = 1000104002,
	DISPLAY_P3_LINEAR_EXT = 1000104003,
	DCI_P3_NONLINEAR_EXT = 1000104004,
	BT709_LINEAR_EXT = 1000104005,
	BT709_NONLINEAR_EXT = 1000104006,
	BT2020_LINEAR_EXT = 1000104007,
	HDR10_ST2084_EXT = 1000104008,
	DOLBYVISION_EXT = 1000104009,
	HDR10_HLG_EXT = 1000104010,
	ADOBERGB_LINEAR_EXT = 1000104011,
	ADOBERGB_NONLINEAR_EXT = 1000104012,
	PASS_THROUGH_EXT = 1000104013,
	EXTENDED_SRGB_NONLINEAR_EXT = 1000104014,
	DISPLAY_NATIVE_AMD = 1000213000,
};

// VkCompositeAlphaFlagBitsKHR
enum class CompositeAlphaFlags {
	None = 0,
	Opaque = 0x00000001,
	Premultiplied = 0x00000002,
	Postmultiplied = 0x00000004,
	Inherit = 0x00000008,
};

SP_DEFINE_ENUM_AS_MASK(CompositeAlphaFlags)

// mapping to VkImageTiling
enum class ImageTiling {
	Optimal = 0,
	Linear = 1,
};

// mapping to VkImageUsageFlagBits
enum class ImageUsage {
	None,
	TransferSrc = 0x00000001,
	TransferDst = 0x00000002,
    Sampled = 0x00000004,
    Storage = 0x00000008,
    ColorAttachment = 0x00000010,
    DepthStencilAttachment = 0x00000020,
    TransientAttachment = 0x00000040,
    InputAttachment = 0x00000080,
};

SP_DEFINE_ENUM_AS_MASK(ImageUsage);

enum class PresentMode : uint32_t {
	Unsupported,
	Immediate,
	FifoRelaxed,
	Fifo,
	Mailbox
};

enum class AttachmentStorageType {
	// Implementation-defined transient memory storage (if supported)
	Transient,

	// Attachment data stored in per-frame memory
	FrameStateless,

	// Attachment stored in independent, but persistent memory
	ObjectStateless,

	// Attachment has a persistent state
	Stateful,
};

enum class ImageHints {
	None = 0,
	Opaque = 1 << 0,
	FixedSize = 1 << 1,
	DoNotCache = 1 << 2,
	ReadOnly = 1 << 3,
	Static = FixedSize | DoNotCache | ReadOnly
};

SP_DEFINE_ENUM_AS_MASK(ImageHints);

// VkComponentSwizzle
enum class ComponentMapping : uint32_t {
	Identity = 0,
	Zero = 1,
	One = 2,
	R = 3,
	G = 4,
	B = 5,
	A = 6,
};

// VkFilter
enum class Filter {
	Nearest = 0,
	Linear = 1,
	Cubic = 1000015000
};

// VkSamplerMipmapMode
enum class SamplerMipmapMode {
	Nearest = 0,
	Linear = 1,
};

// VkSamplerAddressMode
enum class SamplerAddressMode {
	Repeat = 0,
	MirroredRepeat = 1,
	ClampToEdge = 2,
	ClampToBorder = 3,
};

// VkCompareOp
enum class CompareOp : uint8_t {
	Never = 0,
	Less = 1,
	Equal = 2,
	LessOrEqual = 3,
	Greater = 4,
	NotEqual = 5,
	GreaterOrEqual = 6,
	Always = 7,
};

enum class BlendFactor : uint8_t {
	Zero = 0,
	One = 1,
	SrcColor = 2,
	OneMinusSrcColor = 3,
	DstColor = 4,
	OneMinusDstColor = 5,
	SrcAlpha = 6,
	OneMinusSrcAlpha = 7,
	DstAlpha = 8,
	OneMinusDstAlpha = 9,
};

enum class BlendOp : uint8_t {
	Add = 0,
	Subtract = 1,
	ReverseSubtract = 2,
	Min = 3,
	Max = 4,
};

enum class ColorComponentFlags {
	R = 0x00000001,
	G = 0x00000002,
	B = 0x00000004,
	A = 0x00000008,
	All = 0x0000000F
};

enum class StencilOp : uint8_t {
	Keep = 0,
	Zero = 1,
	Replace = 2,
	IncrementAndClamp = 3,
	DecrementAndClamp = 4,
	Invert = 5,
	InvertAndWrap = 6,
	DecrementAndWrap = 7,
};

SP_DEFINE_ENUM_AS_MASK(ColorComponentFlags)

enum class SurfaceTransformFlags {
	None,
	Identity = 0x00000001,
	Rotate90 = 0x00000002,
	Rotate180 = 0x00000004,
	Rotate270 = 0x00000008,
	Mirror = 0x00000010,
	MirrorRotate90 = 0x00000020,
	MirrorRotate180 = 0x00000040,
	MirrorRotate270 = 0x00000080,
	Inherit = 0x00000100,
	PreRotated = 0x01000000,
	TransformMask = 0x000001FF,
};

SP_DEFINE_ENUM_AS_MASK(SurfaceTransformFlags)

inline SurfaceTransformFlags getPureTransform(SurfaceTransformFlags flags) {
	return flags & SurfaceTransformFlags::TransformMask;
}

enum class RenderingLevel {
	Default,
	Solid,
	Surface,
	Transparent
};

enum class ObjectType {
	Unknown,
	Buffer,
	BufferView,
	CommandPool,
	DescriptorPool,
	DescriptorSetLayout,
	Event,
	Fence,
	Framebuffer,
	Image,
	ImageView,
	Pipeline,
	PipelineCache,
	PipelineLayout,
	QueryPool,
	RenderPass,
	Sampler,
	Semaphore,
	ShaderModule,
	DeviceMemory,
	Surface,
	Swapchain
};

enum class PixelFormat {
	Unknown,
	A, // single-channel color
	IA, // dual-channel color
	RGB,
	RGBA,
	D, // depth
	DS, // depth-stencil
	S // stencil
};

// VkQueueFlagBits
enum class QueueFlags : uint32_t {
	None,
	Graphics = 1 << 0,
	Compute = 1 << 1,
	Transfer = 1 << 2,
	SparceBinding = 1 << 3,
	Protected = 1 << 4,
	VideoDecode = 1 << 5,
	VideoEncode = 1 << 6,
	Present = 0x8000'0000,
};

SP_DEFINE_ENUM_AS_MASK(QueueFlags)

enum class DeviceIdleFlags {
	None,
	PreQueue = 1 << 0,
	PreDevice = 1 << 1,
	PostQueue = 1 << 2,
	PostDevice = 1 << 3,
};

SP_DEFINE_ENUM_AS_MASK(DeviceIdleFlags)

enum class FenceType {
	Default,
	Swapchain,
};

enum class SemaphoreType {
	Default,
	Timeline,
};

}

#endif /* XENOLITH_CORE_XLCOREENUM_H_ */
