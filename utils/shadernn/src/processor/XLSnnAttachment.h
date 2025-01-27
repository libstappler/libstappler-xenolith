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

#ifndef SRC_PROCESSOR_XLSNNATTACHMENT_H_
#define SRC_PROCESSOR_XLSNNATTACHMENT_H_

#include "XLSnnModel.h"

namespace stappler::xenolith::shadernn {

class Attachment : public Ref {
public:
	virtual ~Attachment() = default;

	bool init(uint32_t, Extent3, Layer *);

	void addInputBy(Layer *);

	uint32_t getId() const { return _id; }

	uint32_t getOutputIds() const;

	Extent3 getExtent() const { return _extent; }

	StringView getName() const { return _name; }

	Layer *getOutputBy() const { return _outputBy; }

protected:
	String _name;
	uint32_t _id = 0;
	Extent3 _extent;
	Vector<Layer *> _inputBy;
	Layer *_outputBy;
};

}

#endif /* SRC_PROCESSOR_XLSNNATTACHMENT_H_ */
