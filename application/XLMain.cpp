/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "SPCommon.h"
#include "SPSharedModule.h"
#include "SPLog.h"

#if MODULE_XENOLITH_APPLICATION
#include "XLContext.h"
#endif

int main(int argc, const char *argv[]) {
	// Main symbol should depend only on stappler_core for successful linkage
	// So, use SharedModule to load `Context::run`
#if MODULE_XENOLITH_APPLICATION
	auto runFn =
			stappler::SharedModule::acquireTypedSymbol<decltype(&stappler::xenolith::Context::run)>(
					stappler::buildconfig::MODULE_XENOLITH_APPLICATION_NAME,
					stappler::xenolith::Context::SymbolContextRunName);
	if (runFn) {
		return runFn(argc, argv);
	}
	stappler::log::error("main",
			"Fail to load entry point `Context::run` from MODULE_XENOLITH_APPLICATION_NAME");
	return -1;
#else
	stappler::log::error("main",
			"MODULE_XENOLITH_APPLICATION is not defined for the default entry point");
	return -1;
#endif
}
