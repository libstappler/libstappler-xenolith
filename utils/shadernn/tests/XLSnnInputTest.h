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

#ifndef TESTS_XLSNNINPUTTEST_H_
#define TESTS_XLSNNINPUTTEST_H_

#include "XLSnnVkInputLayer.h"
#include "XLApplication.h"

namespace stappler::xenolith::vk::shadernn {

class InputQueue : public core::Queue {
public:
	virtual ~InputQueue();

	bool init();

	const AttachmentData *getImageAttachment() const { return _imageAttachment; }
	const AttachmentData *getOutputAttachment() const { return _outputAttachment; }
	const AttachmentData *getDataAttachment() const;

	void run(Application *, StringView image);

protected:
	using core::Queue::init;

	const core::QueuePassData * _inputLayer = nullptr;
	const AttachmentData *_imageAttachment = nullptr;
	const AttachmentData *_outputAttachment = nullptr;
};

}

#endif /* TESTS_XLSNNINPUTTEST_H_ */
