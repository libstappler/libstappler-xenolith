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

namespace stappler::xenolith {

using InputFlags = core::InputFlags;
using InputMouseButton = core::InputMouseButton;
using InputModifier = core::InputModifier;
using InputKeyCode = core::InputKeyCode;
using InputKeyComposeState = core::InputKeyComposeState;
using InputEventName = core::InputEventName;
using InputEventData = core::InputEventData;

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

using TextCursorPosition = ValueWrapper<uint32_t, class TextCursorPositionFlag>;
using TextCursorLength = ValueWrapper<uint32_t, class TextCursorStartFlag>;

enum class TextInputType {
	Empty				= 0,
	Date_Date			= 1,
	Date_DateTime		= 2,
	Date_Time			= 3,
	Date				= Date_DateTime,

	Number_Numbers		= 4,
	Number_Decimial		= 5,
	Number_Signed		= 6,
	Number				= Number_Numbers,

	Phone				= 7,

	Text_Text			= 8,
	Text_Search			= 9,
	Text_Punctuation	= 10,
	Text_Email			= 11,
	Text_Url			= 12,
	Text				= Text_Text,

	Default				= Text_Text,

	ClassMask			= 0b00011111,
	PasswordBit			= 0b00100000,
	MultiLineBit		= 0b01000000,
	AutoCorrectionBit	= 0b10000000,

	ReturnKeyMask		= 0b00001111 << 8,

	ReturnKeyDefault	= 1 << 8,
	ReturnKeyGo			= 2 << 8,
	ReturnKeyGoogle		= 3 << 8,
	ReturnKeyJoin		= 4 << 8,
	ReturnKeyNext		= 5 << 8,
	ReturnKeyRoute		= 6 << 8,
	ReturnKeySearch		= 7 << 8,
	ReturnKeySend		= 8 << 8,
	ReturnKeyYahoo		= 9 << 8,
	ReturnKeyDone		= 10 << 8,
	ReturnKeyEmergencyCall = 11 << 8,
};

SP_DEFINE_ENUM_AS_MASK(TextInputType);

struct TextCursor {
	static const TextCursor InvalidCursor;

	uint32_t start;
	uint32_t length;

	constexpr TextCursor() : start(maxOf<uint32_t>()), length(0) { }
	constexpr TextCursor(uint32_t pos) : start(pos), length(0) { }
	constexpr TextCursor(uint32_t start, uint32_t length) : start(start), length(length) { }
	constexpr TextCursor(TextCursorPosition pos) : start(pos.get()), length(0) { }
	constexpr TextCursor(TextCursorPosition pos, TextCursorLength len) : start(pos.get()), length(len.get()) { }
	constexpr TextCursor(TextCursorPosition first, TextCursorPosition last)
	: start(std::min(first.get(), last.get()))
	, length(((first > last)?(first - last).get():(last - first).get()) + 1) { }

	constexpr bool operator==(const TextCursor &) const = default;
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
struct hash<stappler::xenolith::InputEventData> {
	size_t operator() (const stappler::xenolith::InputEventData &ev) const {
		return std::hash<uint32_t>{}(ev.id);
	}
};

}

#endif /* XENOLITH_SCENE_INPUT_XLINPUT_H_ */
