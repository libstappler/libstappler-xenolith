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

#ifndef XENOLITH_SCENE_XLSCENECONFIG_H_
#define XENOLITH_SCENE_XLSCENECONFIG_H_

#include "XLCommon.h"

namespace stappler::xenolith::config {

static constexpr size_t NodePreallocateChilds = 4;

static constexpr size_t MaxMaterialImages = 4;

static constexpr uint32_t MaxAmbientLights = 16;
static constexpr uint32_t MaxDirectLights = 16;

#if DEBUG
static constexpr uint64_t MaxDirectorDeltaTime = 10'000'000 / 16;
#else
static constexpr uint64_t MaxDirectorDeltaTime = 100'000'000 / 16;
#endif

}

#endif /* XENOLITH_SCENE_XLSCENECONFIG_H_ */
