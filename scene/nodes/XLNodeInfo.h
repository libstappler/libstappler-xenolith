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

#ifndef XENOLITH_SCENE_NODES_XLNODEINFO_H_
#define XENOLITH_SCENE_NODES_XLNODEINFO_H_

#include "XLApplicationInfo.h"
#include "XLSceneConfig.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

using RenderingLevel = core::RenderingLevel;

using StateId = uint32_t;

static constexpr uint64_t InvalidTag = maxOf<uint64_t>();

enum class NodeFlags {
	None,
	TransformDirty = 1 << 0,
	ContentSizeDirty = 1 << 1,

	DirtyMask = TransformDirty | ContentSizeDirty
};

SP_DEFINE_ENUM_AS_MASK(NodeFlags)

enum class CommandFlags : uint16_t {
	None,
	DoNotCount = 1 << 0
};

SP_DEFINE_ENUM_AS_MASK(CommandFlags)

struct MaterialInfo {
	std::array<uint64_t, config::MaxMaterialImages> images = { 0 };
	std::array<uint16_t, config::MaxMaterialImages> samplers = { 0 };
	std::array<core::ColorMode, config::MaxMaterialImages> colorModes = { core::ColorMode() };
	core::PipelineMaterialInfo pipeline;

	uint64_t hash() const {
		return hash::hash64(reinterpret_cast<const char *>(this), sizeof(MaterialInfo));
	}

	String description() const;

	bool operator==(const MaterialInfo &info) const = default;
	bool operator!=(const MaterialInfo &info) const = default;

	bool hasImage(uint64_t id) const;

	MaterialInfo() {
		memset(this, 0, sizeof(MaterialInfo));
	}
};

struct ZOrderLess {
	bool operator()(const SpanView<ZOrder> &l, const SpanView<ZOrder> &r) const noexcept {
		auto len = std::max(l.size(), r.size());
#if __LCC__ && __LCC__ <= 126
		// compiler bug workaround
		auto lData = (const ZOrder::Type *)l.data();
		auto rData = (const ZOrder::Type *)r.data();
		for (size_t i = 0; i < len; ++ i) {
			ZOrder::Type valL = (i < l.size()) ? lData[i] : 0;
			ZOrder::Type valR = (i < r.size()) ? rData[i] : 0;
			if (valL != valR) {
				return valL < valR;
			}
		}
#else
		for (size_t i = 0; i < len; ++ i) {
			auto valL = (i < l.size()) ? l.at(i) : ZOrder(0);
			auto valR = (i < r.size()) ? r.at(i) : ZOrder(0);
			if (valL != valR) {
				return valL < valR;
			}
		}
#endif
		return false;
	}
};

struct DrawStateValues {
	core::DynamicState enabled = core::DynamicState::None;
	URect viewport;
	URect scissor;

	bool operator==(const DrawStateValues &) const = default;

	bool isScissorEnabled() const { return (enabled & core::DynamicState::Scissor) != core::DynamicState::None; }
	bool isViewportEnabled() const { return (enabled & core::DynamicState::Viewport) != core::DynamicState::None; }
};

struct DrawStat {
	uint32_t vertexes;
	uint32_t triangles;
	uint32_t zPaths;
	uint32_t drawCalls;

	uint32_t cachedImages;
	uint32_t cachedFramebuffers;
	uint32_t cachedImageViews;
	uint32_t materials;

	uint32_t solidCmds;
	uint32_t surfaceCmds;
	uint32_t transparentCmds;

	uint32_t vertexInputTime;
};

}

#endif /* XENOLITH_SCENE_XLNODEINFO_H_ */
