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

#ifndef XENOLITH_APPLICATION_INPUT_XLINPUT_H_
#define XENOLITH_APPLICATION_INPUT_XLINPUT_H_

#include "XLNodeInfo.h"
#include "XLCoreTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

using core::InputFlags;
using core::InputMouseButton;
using core::InputModifier;
using core::InputKeyCode;
using core::InputKeyComposeState;
using core::InputEventName;
using core::InputEventData;
using core::TextInputType;
using core::TextCursor;
using core::TextCursorPosition;
using core::TextCursorLength;
using core::TextInputString;
using core::TextInputState;
using core::TextInputRequest;

enum class InputEventState {
	Declined, // Получатель не заинтересован в этой цепочке событий
	Processed, // Получатель заинтересован в цепочке событий, но не требует эксклюзивности
	Captured, // Получатель запрашивает цепочку событий в эксклюзивную обработку
};

struct SP_PUBLIC InputEvent {
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

class SP_PUBLIC TextInputViewInterface {
public:
	SP_COVERAGE_TRIVIAL
	virtual ~TextInputViewInterface() { }

	virtual void updateTextCursor(uint32_t pos, uint32_t len) = 0;
	virtual void updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) = 0;
	virtual void runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) = 0;
	virtual void cancelTextInput() = 0;
};

#ifndef __LCC__

constexpr const TextCursor TextCursor::InvalidCursor(maxOf<uint32_t>(), 0.0f);

#endif

} // namespace stappler::xenolith

namespace std {

template <>
struct SP_PUBLIC hash<STAPPLER_VERSIONIZED_NAMESPACE::xenolith::InputEventData> {
	size_t operator()(const STAPPLER_VERSIONIZED_NAMESPACE::xenolith::InputEventData &ev) const {
		return std::hash<uint32_t>{}(ev.id);
	}
};

} // namespace std

#endif /* XENOLITH_APPLICATION_INPUT_XLINPUT_H_ */
