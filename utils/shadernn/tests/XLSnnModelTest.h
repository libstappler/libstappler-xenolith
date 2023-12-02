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

#ifndef TESTS_XLSNNMODELTEST_H_
#define TESTS_XLSNNMODELTEST_H_

#include "XLCoreQueue.h"
#include "XLApplication.h"
#include "XLSnnModel.h"
#include "XLSnnModelProcessor.h"

namespace stappler::xenolith::shadernn {

class ModelQueue : public core::Queue {
public:
	virtual ~ModelQueue() = default;

	bool init(StringView model, ModelFlags, StringView input);

	const Vector<const AttachmentData *> &getInputAttachments() const { return _inputAttachments; }
	const AttachmentData *getOutputAttachment() const { return _outputAttachment; }

	void run(Application *);

protected:
	using core::Queue::init;

	String _image;
	Vector<const AttachmentData *> _inputAttachments;
	const AttachmentData *_outputAttachment = nullptr;

	Rc<Model> _model;
	Rc<ModelProcessor> _processor;
};

}

#endif /* TESTS_XLSNNMODELTEST_H_ */
