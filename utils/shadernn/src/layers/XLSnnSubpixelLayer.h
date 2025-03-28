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

#ifndef SRC_LAYERS_XLSNNSUBPIXELLAYER_H_
#define SRC_LAYERS_XLSNNSUBPIXELLAYER_H_

#include "XLSnnLayer.h"

namespace stappler::xenolith::shadernn {

class SubpixelLayer : public Layer {
public:
	virtual ~SubpixelLayer() = default;

	virtual bool init(Model *, StringView tag, size_t idx, const Value&) override;

	virtual Extent3 getOutputExtent() const override {
		auto ret = Layer::getOutputExtent();
		ret.depth = 1;
		return ret;
	}

	virtual LayerTransformInfo getOutputTransform() const override {
		return {0, { {static_cast<float>(_kernelSize), static_cast<float>(_kernelSize), 0.0f, 0.0f}}};
	}

	virtual const core::QueuePassData *prepare(core::Queue::Builder &builder,
			Map<Layer *, const core::AttachmentData *> inputs,
			Map<Attachment *, const core::AttachmentData *> attachments) override;

protected:
    uint32_t _kernelSize = 2;
    std::vector<double> _biases; // make it float?
};

}

#endif /* SRC_LAYERS_XLSNNSUBPIXELLAYER_H_ */
