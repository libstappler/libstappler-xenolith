/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_SCENE_INPUT_XLINPUT_H_
#define XENOLITH_SCENE_INPUT_XLINPUT_H_

#include "XLNodeInfo.h"
#include "XLCoreInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

using InputFlags = core::InputFlags;
using InputMouseButton = core::InputMouseButton;
using InputModifier = core::InputModifier;
using InputKeyCode = core::InputKeyCode;
using InputKeyComposeState = core::InputKeyComposeState;
using InputEventName = core::InputEventName;
using InputEventData = core::InputEventData;
using TextInputType = core::TextInputType;
using TextCursor = core::TextCursor;
using TextCursorPosition = core::TextCursorPosition;
using TextCursorLength = core::TextCursorLength;

struct InputEvent {
	InputEventData data;
	Vec2 originalLocation;
	Vec2 currentLocation;
	Vec2 previousLocation;
	uint64_t originalTime = 0;
	uint64_t currentTime = 0;
	uint64_t previousTime = 0;
	InputModifier originalModifiers = InputModifier::None;
	InputModifier previousModifiers = InputModifier::None;
};

class TextInputViewInterface {
public:
	virtual ~TextInputViewInterface() { }

	virtual void updateTextCursor(uint32_t pos, uint32_t len) = 0;
	virtual void updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) = 0;
	virtual void runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) = 0;
	virtual void cancelTextInput() = 0;
};

constexpr const TextCursor TextCursor::InvalidCursor(maxOf<uint32_t>(), 0.0f);

}

namespace std {

template <>
struct hash<STAPPLER_VERSIONIZED_NAMESPACE::xenolith::InputEventData> {
	size_t operator() (const STAPPLER_VERSIONIZED_NAMESPACE::xenolith::InputEventData &ev) const {
		return std::hash<uint32_t>{}(ev.id);
	}
};

}

#endif /* XENOLITH_SCENE_INPUT_XLINPUT_H_ */
