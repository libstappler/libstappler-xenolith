/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_SCENE_INPUT_XLTEXTINPUTMANAGER_H_
#define XENOLITH_SCENE_INPUT_XLTEXTINPUTMANAGER_H_

#include "XLInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class TextInputManager;

// TextInputHandler used to capture text input
// - Only one handler can be active for one view, new running handler will displace old one
// - Owner is responsible for handler's existence and callback's correctness when handler
// is active (attached to manager)
// - setString/setCursor has no meaning if handler is not active
// - Whole input string from handler will be transferred to device input manager, so, try to
// keep it small (e.g. when working with paragraphs, send only current paragraph). Large
// strings can significantly reduce performance
struct SP_PUBLIC TextInputHandler {
	Function<void(WideStringView, TextCursor, TextCursor)> onText;
	Function<void(bool, const Rect &, float)> onKeyboard;
	Function<void(bool)> onInput;

	Rc<TextInputManager> manager;

	~TextInputHandler();

	bool run(TextInputManager *, WideStringView str = WideStringView(),
			TextCursor cursor = TextCursor(), TextCursor marked = TextCursor::InvalidCursor,
			TextInputType = TextInputType::Empty);
	void cancel();

	// only if this handler is active
	bool setString(WideStringView str, TextCursor cursor = TextCursor(),
			TextCursor marked = TextCursor::InvalidCursor);
	bool setCursor(TextCursor);
	bool setMarked(TextCursor);

	WideStringView getString() const;
	TextCursor getCursor() const;
	TextCursor getMarked() const;

	bool isInputEnabled() const;
	bool isActive() const;
};

class SP_PUBLIC TextInputManager final : public Ref {
public:
	TextInputManager();

	bool init(TextInputViewInterface *);

	void insertText(WideStringView sInsert, bool compose = false);
	void insertText(WideStringView sInsert, TextCursor replacement);
	void setMarkedText(WideStringView sInsert, TextCursor replacement, TextCursor marked);
	void deleteBackward();
	void deleteForward();
	void unmarkText();

	bool hasText();
	void textChanged(WideStringView text, TextCursor, TextCursor);
	void cursorChanged(TextCursor);
	void markedChanged(TextCursor);

	void setInputEnabled(bool enabled);

	void handleTextChanged();

	// run input capture (or update it with new params)
	// propagates all data to device input manager, enables screen keyboard if needed
	bool run(TextInputHandler *, WideStringView str, TextCursor cursor, TextCursor marked,
			TextInputType type);

	// update current buffer string (and/or internal cursor)
	// propagates string and cursor to device input manager to enable autocorrections, suggestions, etc...
	void setString(WideStringView str, TextCursor cursor = TextCursor(),
			TextCursor marked = TextCursor::InvalidCursor);
	void setCursor(TextCursor);
	void setMarked(TextCursor);

	WideStringView getStringByRange(TextCursor);
	WideStringView getString() const;
	TextCursor getCursor() const;
	TextCursor getMarked() const;

	// disable text input, disables keyboard connection and keyboard input event interception
	// default manager automatically disabled when app goes background
	void cancel();

	bool isRunning() const { return _running; }
	bool isInputEnabled() const { return _isInputEnabled; }

	TextInputHandler *getHandler() const { return _handler; }

	bool canHandleInputEvent(const InputEventData &);
	bool handleInputEvent(const InputEventData &);

protected:
	bool doInsertText(WideStringView, bool compose);

	TextInputViewInterface *_view = nullptr;
	TextInputHandler *_handler = nullptr;
	bool _isInputEnabled = false;
	bool _running = false;

	TextInputType _type = TextInputType::Empty;
	WideString _string;
	TextCursor _cursor;
	TextCursor _marked;
	InputKeyComposeState _compose = InputKeyComposeState::Nothing;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_SCENE_INPUT_XLTEXTINPUTMANAGER_H_ */
