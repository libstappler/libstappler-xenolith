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

#include "Renderer2dParticleTest.h"
#include "SPMath.h"
#include "SPTime.h"
#include "SPVec2.h"
#include "XL2dParticleEmitter.h"
#include "XL2dParticleSystem.h"

namespace stappler::xenolith::app {

bool Renderer2dParticleTest::init() {
	if (!LayoutTest::init(LayoutName::Renderer2dParticleTest, "2d particle system test")) {
		return false;
	}

	auto system = Rc<ParticleSystem>::create(64, TimeInterval::milliseconds(16).toMicros(), 3.0f);

	system->setParticleSize(Size2(4.0f, 4.0f));
	system->setRandomness(0.0f);
	system->setNormal(0.0f, numbers::pi * 4);
	system->setVelocity(10.0f);
	system->setAcceleration(20.0f);

	_emitter = addChild(Rc<ParticleEmitter>::create(system));
	_emitter->setAnchorPoint(Anchor::Middle);

	return true;
}

void Renderer2dParticleTest::handleEnter(Scene *scene) { LayoutTest::handleEnter(scene); }

void Renderer2dParticleTest::handleExit() { LayoutTest::handleExit(); }

void Renderer2dParticleTest::handleContentSizeDirty() {
	LayoutTest::handleContentSizeDirty();

	if (_emitter) {
		_emitter->setPosition(_contentSize / 2.0f);
	}
}

} // namespace stappler::xenolith::app
