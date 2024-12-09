/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DSPRITE_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DSPRITE_H_

#include "XLResourceCache.h"
#include "XLNode.h"
#include "XL2dVertexArray.h"
#include "XL2dLinearGradient.h"
#include "XL2dCommandList.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class SP_PUBLIC Sprite : public Node {
public:
	static constexpr uint16_t SamplerIndexDefaultFilterNearest = 0;
	static constexpr uint16_t SamplerIndexDefaultFilterLinear = 1;

	Sprite();
	virtual ~Sprite();

	virtual bool init() override;
	virtual bool init(StringView);
	virtual bool init(Rc<Texture> &&);

	virtual void setTexture(StringView);
	virtual void setTexture(Rc<Texture> &&);
	const Rc<Texture> &getTexture() const;

	virtual void setLinearGradient(Rc<LinearGradient> &&);
	const Rc<LinearGradient> &getLinearGradient() const;

	// texture rect should be normalized
	virtual void setTextureRect(const Rect &);
	virtual const Rect &getTextureRect() const { return _texturePlacement.textureRect; }

	virtual bool visitDraw(FrameInfo &, NodeFlags parentFlags) override;
	virtual void draw(FrameInfo &, NodeFlags flags) override;

	virtual void handleEnter(xenolith::Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;
	virtual void handleTextureLoaded();

	virtual void setColorMode(const core::ColorMode &);
	virtual const core::ColorMode &getColorMode() const { return _colorMode; }

	virtual void setBlendInfo(const core::BlendInfo &);
	virtual const core::BlendInfo &getBlendInfo() const { return _materialInfo.getBlendInfo(); }

	virtual void setTextureLayer(float);
	virtual float getTextureLayer() const { return _textureLayer; }

	// used for debug purposes only, follow rules from PipelineMaterialInfo.lineWidth:
	// 0.0f - draw triangles, < 0.0f - points,  > 0.0f - lines with width
	// corresponding pipeline should be precompiled
	// points and lines are always RenderingLevel::Transparent, when Default level resolves
	virtual void setLineWidth(float);
	virtual float getLineWidth() const { return _materialInfo.getLineWidth(); }

	virtual void setRenderingLevel(RenderingLevel);
	virtual RenderingLevel getRenderingLevel() const { return _renderingLevel; }

	virtual void setNormalized(bool);
	virtual bool isNormalized() const { return _normalized; }

	virtual void setTextureAutofit(Autofit);
	virtual Autofit getTextureAutofit() const { return _texturePlacement.autofit; }

	virtual void setTextureAutofitPosition(const Vec2 &);
	virtual const Vec2 &getTextureAutofitPosition() const { return _texturePlacement.autofitPos; }

	/** Семплеры определяются во время старта цикла графики (gl::Loop) и неизменны в последствии
	 * По умолчанию, семплер с индексом 0 использует фильтр nearest, 1 - linear.
	 * Разработчики приложений могут определять свою схему для семплеров,
	 * но рекомендуем следовать соглашению в отношении 0 и 1
	 *
	 * Если семплер с указанным индексом не определён в движке - поведение не определено
	 */
	virtual void setSamplerIndex(uint16_t);
	virtual uint16_t getSamplerIndex() const { return _samplerIdx; }

	virtual void setCommandFlags(CommandFlags flags) { _commandFlags = flags; }
	virtual void addCommandFlags(CommandFlags flags) { _commandFlags |= flags; }
	virtual void removeCommandFlags(CommandFlags flags) { _commandFlags &= ~flags; }
	virtual CommandFlags getCommandFlags() const { return _commandFlags; }

	virtual void setTextureLoadedCallback(Function<void()> &&);

protected:
	using Node::init;

	virtual void pushCommands(FrameInfo &, NodeFlags flags);

	virtual MaterialInfo getMaterialInfo() const;
	virtual Vector<core::MaterialImage> getMaterialImages() const;
	virtual bool isMaterialRevokable() const;
	virtual void updateColor() override;
	virtual void updateVertexesColor();
	virtual void initVertexes();
	virtual void updateVertexes(FrameInfo &frame);

	virtual void updateBlendAndDepth();

	virtual RenderingLevel getRealRenderingLevel() const;

	virtual bool checkVertexDirty() const;

	virtual CmdInfo buildCmdInfo(const FrameInfo &) const;

	String _textureName;
	Rc<Texture> _texture;
	VertexArray _vertexes;

	uint16_t _samplerIdx = 0;

	bool _materialDirty = true;
	bool _normalized = false;
	bool _vertexesDirty = true;
	bool _vertexColorDirty = true;

	bool _flippedX = false;
	bool _flippedY = false;
	bool _rotated = false;
	bool _isTextureLoaded = false;

	ImagePlacementInfo _texturePlacement;

	float _textureLayer = 0.0f;

	// Track dynamic texture size
	Extent3 _targetTextureSize;

	RenderingLevel _renderingLevel = RenderingLevel::Default;
	RenderingLevel _realRenderingLevel = RenderingLevel::Default;
	core::MaterialId _materialId = 0;
	CommandFlags _commandFlags = CommandFlags::None;

	Color4F _tmpColor;
	core::ColorMode _colorMode;
	core::BlendInfo _blendInfo;
	core::PipelineMaterialInfo _materialInfo;

	Vector<Rc<core::DependencyEvent>> _pendingDependencies;
	Function<void()> _textureLoadedCallback;

	Rc<LinearGradient> _linearGradient;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DSPRITE_H_ */
