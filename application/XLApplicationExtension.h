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

#ifndef XENOLITH_APPLICATION_XLAPPLICATIONEXTENSION_H_
#define XENOLITH_APPLICATION_XLAPPLICATIONEXTENSION_H_

#include "XLApplicationInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Application;

class SP_PUBLIC ApplicationExtension : public Ref {
public:
	virtual ~ApplicationExtension() { }

	virtual void initialize(Application *) = 0;
	virtual void invalidate(Application *) = 0;

	virtual void update(Application *, const UpdateTime &t) = 0;
};

}

#endif /* XENOLITH_APPLICATION_XLAPPLICATIONEXTENSION_H_ */
