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

#include "XLCommon.h"

#include "XL2dCommandList.cc"
#include "XL2dVertexArray.cc"
#include "XL2dFrameContext.cc"

#include "XL2dSprite.cc"
#include "XL2dLayer.cc"
#include "XL2dLabel.cc"
#include "XL2dScene.cc"
#include "XL2dSceneContent.cc"
#include "XL2dSceneLayout.cc"
#include "XL2dSceneLight.cc"

#include "XL2dVectorCanvas.cc"
#include "XL2dVectorSprite.cc"

#include "XL2dActionAcceleratedMove.cc"
#include "XL2dImageLayer.cc"
#include "XL2dLayerRounded.cc"
#include "XL2dLinearProgress.cc"
#include "XL2dRoundedProgress.cc"

#include "XL2dScrollController.cc"
#include "XL2dScrollItemHandle.cc"
#include "XL2dScrollViewBase.cc"
#include "XL2dScrollView.cc"

#include "XL2dLinearGradient.cc"
#include "XL2dBootstrapApplication.cc"

#ifdef MODULE_XENOLITH_BACKEND_VK
#include "backend/vk/XL2dVkMaterial.cc"
#include "backend/vk/XL2dVkVertexPass.cc"
#include "backend/vk/XL2dVkShadow.cc"
#include "backend/vk/XL2dVkShadowPass.cc"
#endif
