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

#include "XLLinux.h"
#include "SPCore.h"
#include "SPSpanView.h"
#include "XLContextInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

void _xl_null_fn() { }

static constexpr StringView s_cursorsArrow[] = {
	StringView("left_ptr"),
	StringView("top_left_arrow."),
	StringView("arrow"),
	StringView("default"),
};

static constexpr StringView s_cursorsRightArrow[] = {
	StringView("right_ptr"),
};

static constexpr StringView s_cursorsText[] = {
	StringView("text"),
	StringView("xterm"),
};

static constexpr StringView s_cursorsVerticalText[] = {
	StringView("vertical-text"),
	StringView("xterm"),
};

static constexpr StringView s_cursorsPointer[] = {
	StringView("hand2"),
	StringView("hand"),
	StringView("pointer"),
};

static constexpr StringView s_cursorsGrab[] = {
	StringView("grab"),
	StringView("openhand"),
	StringView("hand1"),
};

static constexpr StringView s_cursorsGrabbed[] = {
	StringView("closedhand"),
	StringView("grabbing"),
	StringView("size_all"),
};

static constexpr StringView s_cursorsTarget[] = {
	StringView("target"),
	StringView("icon"),
	StringView("draped_box"),
	StringView("dotbox"),
};

static constexpr StringView s_cursorsPencil[] = {
	StringView("pencil"),
	StringView("draft"),
};

static constexpr StringView s_cursorsHelp[] = {
	StringView("help"),
	StringView("question_arrow"),
	StringView("whats_this"),
};

static constexpr StringView s_cursorsProgress[] = {
	StringView("progress"),
	StringView("left_ptr_watch"),
	StringView("half-busy"),
};

static constexpr StringView s_cursorsWait[] = {
	StringView("wait"),
	StringView("watch"),
};

static constexpr StringView s_cursorsCopy[] = {
	StringView("copy"),
};

static constexpr StringView s_cursorsAlias[] = {
	StringView("alias"),
	StringView("dnd-link"),
};

static constexpr StringView s_cursorsNoDrop[] = {
	StringView("no-drop"),
	StringView("forbidden"),
};

static constexpr StringView s_cursorsNotAllowed[] = {
	StringView("not-allowed"),
	StringView("crossed_circle"),
};

static constexpr StringView s_cursorsMove[] = {
	StringView("move"),
};

static constexpr StringView s_cursorsResizeAll[] = {
	StringView("all-scroll"),
};

static constexpr StringView s_cursorsResizeTopLeft[] = {
	StringView("top_left_corner"),
	StringView("nw-resize"),
};

static constexpr StringView s_cursorsResizeTopRight[] = {
	StringView("top_right_corner"),
	StringView("ne-resize"),
};

static constexpr StringView s_cursorsResizeTop[] = {
	StringView("top_side"),
	StringView("n-resize"),
};

static constexpr StringView s_cursorsResizeLeft[] = {
	StringView("left_side"),
	StringView("w-resize"),
};

static constexpr StringView s_cursorsResizeRight[] = {
	StringView("right_side"),
	StringView("e-resize"),
};

static constexpr StringView s_cursorsResizeBottomLeft[] = {
	StringView("bottom_left_corner"),
	StringView("sw-resize"),
};

static constexpr StringView s_cursorsResizeBottomRight[] = {
	StringView("bottom_right_corner"),
	StringView("se-resize"),
};

static constexpr StringView s_cursorsResizeBottom[] = {
	StringView("bottom_side"),
	StringView("s-resize"),
};

static constexpr StringView s_cursorsResizeTopBottom[] = {
	StringView("ns-resize"),
	StringView("row-resize"),
	StringView("v_double_arrow"),
	StringView("sb_v_double_arrow"),
	StringView("split_v"),
	StringView("size-ver"),
	StringView("size_ver"),
	StringView("double_arrow"),
};

static constexpr StringView s_cursorsResizeLeftRight[] = {
	StringView("ew-resize"),
	StringView("col-resize"),
	StringView("h_double_arrow"),
	StringView("sb_h_double_arrow"),
	StringView("split_h"),
};

static constexpr StringView s_cursorssResizeTopRightBottomLeft[] = {
	StringView("nesw-resize"),
	StringView("fd_double_arrow"),
	StringView("size-bdiag"),
	StringView("size_bdiag"),
};

static constexpr StringView s_cursorssResizeTopLeftBottomRight[] = {
	StringView("nwse-resize"),
	StringView("bd_double_arrow"),
	StringView("size-fdiag"),
	StringView("size_fdiag"),
};

SpanView<StringView> getCursorNames(WindowLayerFlags flags) {
	auto cursor = flags & WindowLayerFlags::CursorMask;

	if (cursor != WindowLayerFlags::None) {
		switch (cursor) {
		case WindowLayerFlags::CursorArrow: return makeSpanView(s_cursorsArrow); break;
		case WindowLayerFlags::CursorRightArrow: return makeSpanView(s_cursorsRightArrow); break;
		case WindowLayerFlags::CursorText: return makeSpanView(s_cursorsText); break;
		case WindowLayerFlags::CursorVerticalText:
			return makeSpanView(s_cursorsVerticalText);
			break;
		case WindowLayerFlags::CursorPointer: return makeSpanView(s_cursorsPointer); break;
		case WindowLayerFlags::CursorGrab: return makeSpanView(s_cursorsGrab); break;
		case WindowLayerFlags::CursorGrabbed: return makeSpanView(s_cursorsGrabbed); break;
		case WindowLayerFlags::CursorTarget: return makeSpanView(s_cursorsTarget); break;
		case WindowLayerFlags::CursorPencil: return makeSpanView(s_cursorsPencil); break;
		case WindowLayerFlags::CursorHelp: return makeSpanView(s_cursorsHelp); break;
		case WindowLayerFlags::CursorProgress: return makeSpanView(s_cursorsProgress); break;
		case WindowLayerFlags::CursorWait: return makeSpanView(s_cursorsWait); break;
		case WindowLayerFlags::CursorCopy: return makeSpanView(s_cursorsCopy); break;
		case WindowLayerFlags::CursorAlias: return makeSpanView(s_cursorsAlias); break;
		case WindowLayerFlags::CursorNoDrop: return makeSpanView(s_cursorsNoDrop); break;
		case WindowLayerFlags::CursorNotAllowed: return makeSpanView(s_cursorsNotAllowed); break;
		case WindowLayerFlags::CursorMove: return makeSpanView(s_cursorsMove); break;
		case WindowLayerFlags::CursorResizeTop: return makeSpanView(s_cursorsResizeTop); break;
		case WindowLayerFlags::CursorResizeTopRight:
			return makeSpanView(s_cursorsResizeTopRight);
			break;
		case WindowLayerFlags::CursorResizeRight: return makeSpanView(s_cursorsResizeRight); break;
		case WindowLayerFlags::CursorResizeBottomRight:
			return makeSpanView(s_cursorsResizeBottomRight);
			break;
		case WindowLayerFlags::CursorResizeBottom:
			return makeSpanView(s_cursorsResizeBottom);
			break;
		case WindowLayerFlags::CursorResizeBottomLeft:
			return makeSpanView(s_cursorsResizeBottomLeft);
			break;
		case WindowLayerFlags::CursorResizeLeft: return makeSpanView(s_cursorsResizeLeft); break;
		case WindowLayerFlags::CursorResizeTopLeft:
			return makeSpanView(s_cursorsResizeTopLeft);
			break;
		case WindowLayerFlags::CursorResizeTopBottom:
			return makeSpanView(s_cursorsResizeTopBottom);
			break;
		case WindowLayerFlags::CursorResizeLeftRight:
			return makeSpanView(s_cursorsResizeLeftRight);
			break;
		case WindowLayerFlags::CursorResizeTopLeftBottomRight:
			return makeSpanView(s_cursorssResizeTopLeftBottomRight);
			break;
		case WindowLayerFlags::CursorResizeTopRightBottomLeft:
			return makeSpanView(s_cursorssResizeTopRightBottomLeft);
			break;
		case WindowLayerFlags::CursorResizeAll: return makeSpanView(s_cursorsResizeAll); break;
		default: break;
		}
	}
	return SpanView<StringView>();
}

} // namespace stappler::xenolith::platform
