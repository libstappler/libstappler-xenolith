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

#ifndef SRC_PROCESSOR_XLSNNMODELPROCESSOR_H_
#define SRC_PROCESSOR_XLSNNMODELPROCESSOR_H_

#include "XLSnnModel.h"
#include "XLCoreQueue.h"

namespace stappler::xenolith::shadernn {

struct ModelSpecialization {
	Map<const Layer *, Extent3> inputs;
	Map<const Attachment *, Extent3> attachments;
};

class ModelProcessor : public Ref {
public:
	using LayerConstructor = Rc<Layer> (*)(Model *, StringView tag, size_t idx, const Value &);

	virtual ~ModelProcessor() = default;

	bool init();

	Rc<Model> load(const FileInfo &modelPath, ModelFlags);

	ModelSpecialization specializeModel(Model *, Extent3);
	ModelSpecialization specializeModel(Model *, Map<const Layer *, Extent3> &&inputs);

protected:
	bool loadFromJson(Model *, Value &&) const;

	Rc<Layer> makeLayer(Model *m, StringView tag, size_t idx, Value &&) const;

	Map<String, LayerConstructor> _layers;
};

} // namespace stappler::xenolith::shadernn

#endif /* SRC_PROCESSOR_XLSNNMODELPROCESSOR_H_ */
