/* Copyright © 2017-2020 ABBYY Production LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
--------------------------------------------------------------------------------------------------------------*/

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

#ifndef SRC_PROCESSOR_XLSNNRANDOM_H_
#define SRC_PROCESSOR_XLSNNRANDOM_H_

#include "XLCommon.h"

namespace stappler::xenolith::shadernn {

// The class to generate random values
// It uses the complementary-multiply-with-carry algorithm
// C lag-1024, multiplier(a) = 108798, initial carry(c) = 12345678
class Random {
public:
	explicit Random( unsigned int seed = 0xBADF00D );

	// Resets to the starting state
	void reset( unsigned int seed );

	// Returns the next random value
	unsigned int next();
	// Returns a double value from a uniform distribution in [ min, max ) range
	// If min == max, min is returned
	double uniform( double min, double max );
	// Returns an int value from a uniform distribution in [ min, max ] range. Note that max return value is possible!
	// If min == max, min is returned
	int uniformInt( int min, int max );
	// Returns a double value from a normal distribution N(mean, sigma)
	double normal( double mean, double sigma );

private:
	static const int lagSize = 1024;
	static const unsigned int stdLag[]; // the standard (pre-generated) lag
	static const unsigned int multiplier = 108798;
	static const unsigned int initialCarry = 12345678;

	unsigned int lag[lagSize];
	unsigned int carry;
	int lagPosition;
};

}

#endif /* SRC_PROCESSOR_XLSNNRANDOM_H_ */
