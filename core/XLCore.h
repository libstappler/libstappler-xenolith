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

#ifndef XENOLITH_CORE_XLCORE_H_
#define XENOLITH_CORE_XLCORE_H_

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPFilesystem.h"
#include "SPThreadTask.h"
#include "SPSpanView.h"
#include "SPLog.h"
#include "SPHashTable.h"

#include "SPVec2.h"
#include "SPVec3.h"
#include "SPVec4.h"
#include "SPGeometry.h"
#include "SPQuaternion.h"
#include "SPMat4.h"
#include "SPPadding.h"
#include "SPColor.h"
#include "SPFontStyle.h"

#include <typeindex>
#include <forward_list>
#include <optional>

#include "XLCoreConfig.h"

#define XL_ASSERT(cond, msg)  do { if (!(cond)) { stappler::log::text(stappler::log::LogType::Fatal, "Assert", msg); } assert((cond)); } while (0)

#ifndef XLASSERT
#if DEBUG
#define XLASSERT(cond, msg) XL_ASSERT(cond, msg)
#else
#define XLASSERT(cond, msg)
#endif
#endif

#if __CDT_PARSER__
// IDE-specific definition

// enable all modules

#define MODULE_XENOLITH_CORE 1
#define MODULE_XENOLITH_APPLICATION 1
#define MODULE_XENOLITH_FONT 1
#define MODULE_XENOLITH_PLATFORM 1
#define MODULE_XENOLITH_SCENE 1
#define MODULE_XENOLITH_BACKEND_VK 1
#define MODULE_XENOLITH_BACKEND_VKGUI 1
#define MODULE_XENOLITH_RENDERER_BASIC2D 1

#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// Import std memory model as default
using namespace mem_std;

using Vec2 = geom::Vec2;
using Vec3 = geom::Vec3;
using Vec4 = geom::Vec4;
using Mat4 = geom::Mat4;
using Size2 = geom::Size2;
using Size3 = geom::Size3;
using Extent2 = geom::Extent2;
using Extent3 = geom::Extent3;
using Rect = geom::Rect;
using URect = geom::URect;
using UVec2 = geom::UVec2;
using UVec3 = geom::UVec3;
using UVec4 = geom::UVec4;
using Quaternion = geom::Quaternion;
using Color = geom::Color;
using Color3B = geom::Color3B;
using Color4B = geom::Color4B;
using Color4F = geom::Color4F;
using ColorMask = geom::ColorMask;
using Padding = geom::Padding;
namespace Anchor = geom::Anchor;

inline uint32_t XL_MAKE_API_VERSION(uint32_t variant, uint32_t major, uint32_t minor, uint32_t patch) {
   return (uint32_t(variant) << 29) | (uint32_t(major) << 22) | (uint32_t(minor) << 12) | uint32_t(patch);
}

// based on VK_MAKE_API_VERSION
inline uint32_t XL_MAKE_API_VERSION(StringView version) {
	uint32_t ver[4];
	uint32_t i = 0;
	version.split<StringView::Chars<'.'>>([&] (StringView str) {
		if (i < 4) {
			ver[i] = str.readInteger(10).get(0);
		}
	});

	uint32_t verCode = 0;
	switch (i) {
	case 0: verCode = XL_MAKE_API_VERSION(0, 0, 1, 0); break;
	case 1: verCode = XL_MAKE_API_VERSION(0, ver[0], 0, 0); break;
	case 2: verCode = XL_MAKE_API_VERSION(0, ver[0], ver[1], 0); break;
	case 3: verCode = XL_MAKE_API_VERSION(0, ver[0], ver[1], ver[2]); break;
	case 4: verCode = XL_MAKE_API_VERSION(ver[0], ver[1], ver[2], ver[3]); break;
	default: break;
	}
	return verCode;
}

inline String getVersionDescription(uint32_t version) {
	return toString(version >> 29, ".", version >> 22, ".", (version >> 12) & 0b1111111111, ".", version & 0b111111111111);
}

class PoolRef : public Ref {
public:
	virtual ~PoolRef() {
		memory::pool::destroy(_pool);
	}

	PoolRef(memory::pool_t *root = nullptr) {
		_pool = memory::pool::create(root);
	}

	PoolRef(PoolRef *p) : PoolRef(p->_pool) { }

	memory::pool_t *getPool() const { return _pool; }

	void *palloc(size_t size) {
		return memory::pool::palloc(_pool, size);
	}

	template <typename Callable>
	auto perform(const Callable &cb) {
		memory::pool::context<memory::pool_t *> ctx(_pool);
		return cb();
	}

protected:
	memory::pool_t *_pool = nullptr;
};

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::profiling {

struct ProfileData {
	uint64_t timestamp;
	StringView tag;
	StringView variant;
	uint64_t limit;
};

ProfileData begin(StringView tag, StringView variant, uint64_t limit);

void end(ProfileData &);
void store(ProfileData &);

#define XL_PROFILE_DEBUG 0

#if XL_PROFILE_DEBUG
#define XL_PROFILE_BEGIN(name, tag, variant, limit) \
	auto __xl_profile__ ## name = stappler::xenolith::profiling::begin(tag, variant, limit);

#define XL_PROFILE_END(name) \
	stappler::xenolith::profiling::end(__xl_profile__ ## name);
#else
#define XL_PROFILE_BEGIN(name, tag, variant, limit)
#define XL_PROFILE_END(name)
#endif

}

#include "XLCoreEnum.h"
#include "XLCorePipelineInfo.h"

#endif /* XENOLITH_CORE_XLCORE_H_ */
