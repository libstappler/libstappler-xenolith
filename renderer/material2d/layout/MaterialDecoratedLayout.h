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

#ifndef XENOLITH_RENDERER_MATERIAL2D_LAYOUT_MATERIALDECORATEDLAYOUT_H_
#define XENOLITH_RENDERER_MATERIAL2D_LAYOUT_MATERIALDECORATEDLAYOUT_H_

#include "MaterialLayerSurface.h"
#include "XL2dSceneLayout.h"
#include "XL2dScrollView.h"

namespace stappler::xenolith::material2d {

class DecoratedLayout : public SceneLayout2d {
public:
	virtual ~DecoratedLayout() { }

	virtual bool init(ColorRole decorationColorRole = ColorRole::PrimaryContainer);
	virtual void onContentSizeDirty() override;

	virtual void setDecorationColorRole(ColorRole);
	virtual ColorRole getDecorationColorRole() const;

	virtual void setViewDecorationTracked(bool value);
	virtual bool isViewDecorationTracked() const;

	virtual void onForeground(SceneContent2d *l, SceneLayout2d *overlay) override;

	LayerSurface *getBackground() const { return _background; }

protected:
	virtual void updateStatusBar(const SurfaceStyleData &style);

	LayerSurface *_decorationLeft = nullptr;
	LayerSurface *_decorationRight = nullptr;
	LayerSurface *_decorationTop = nullptr;
	LayerSurface *_decorationBottom = nullptr;
	LayerSurface *_background = nullptr;
	bool _viewDecorationTracked = false;
	bool _decorationStyleTracked = true;
};

}

#endif /* XENOLITH_RENDERER_MATERIAL2D_LAYOUT_MATERIALDECORATEDLAYOUT_H_ */
