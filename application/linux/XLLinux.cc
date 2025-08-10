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

static constexpr StringView s_cursorsDefault[] = {
	StringView("left_ptr"),
	StringView("top_left_arrow."),
	StringView("arrow"),
	StringView("default"),
};

static constexpr StringView s_cursorsCell[] = {
	StringView("cell"),
};

static constexpr StringView s_cursorsCrosshair[] = {
	StringView("crosshair"),
};

static constexpr StringView s_cursorsContextMenu[] = {
	StringView("context-menu"),
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

static constexpr StringView s_cursorsAllScroll[] = {
	StringView("all-scroll"),
};

static constexpr StringView s_cursorsZoomIn[] = {
	StringView("zoom-in"),
};

static constexpr StringView s_cursorsZoomOut[] = {
	StringView("zoom-out"),
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
	StringView("nw-resize"),
	StringView("top_left_corner"),
};

static constexpr StringView s_cursorsResizeTopRight[] = {
	StringView("ne-resize"),
	StringView("top_right_corner"),
};

static constexpr StringView s_cursorsResizeTop[] = {
	StringView("n-resize"),
	StringView("top_side"),
};

static constexpr StringView s_cursorsResizeLeft[] = {
	StringView("w-resize"),
	StringView("left_side"),
};

static constexpr StringView s_cursorsResizeRight[] = {
	StringView("e-resize"),
	StringView("right_side"),
};

static constexpr StringView s_cursorsResizeBottomLeft[] = {
	StringView("sw-resize"),
	StringView("bottom_left_corner"),
};

static constexpr StringView s_cursorsResizeBottomRight[] = {
	StringView("se-resize"),
	StringView("bottom_right_corner"),
};

static constexpr StringView s_cursorsResizeBottom[] = {
	StringView("s-resize"),
	StringView("bottom_side"),
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
		case WindowLayerFlags::CursorDefault: return makeSpanView(s_cursorsDefault); break;
		case WindowLayerFlags::CursorContextMenu: return makeSpanView(s_cursorsContextMenu); break;
		case WindowLayerFlags::CursorHelp: return makeSpanView(s_cursorsHelp); break;
		case WindowLayerFlags::CursorPointer: return makeSpanView(s_cursorsPointer); break;
		case WindowLayerFlags::CursorProgress: return makeSpanView(s_cursorsProgress); break;
		case WindowLayerFlags::CursorWait: return makeSpanView(s_cursorsWait); break;
		case WindowLayerFlags::CursorCell: return makeSpanView(s_cursorsCell); break;
		case WindowLayerFlags::CursorCrosshair: return makeSpanView(s_cursorsCrosshair); break;
		case WindowLayerFlags::CursorText: return makeSpanView(s_cursorsText); break;
		case WindowLayerFlags::CursorVerticalText:
			return makeSpanView(s_cursorsVerticalText);
			break;
		case WindowLayerFlags::CursorAlias: return makeSpanView(s_cursorsAlias); break;
		case WindowLayerFlags::CursorCopy: return makeSpanView(s_cursorsCopy); break;
		case WindowLayerFlags::CursorMove: return makeSpanView(s_cursorsMove); break;
		case WindowLayerFlags::CursorNoDrop: return makeSpanView(s_cursorsNoDrop); break;
		case WindowLayerFlags::CursorNotAllowed: return makeSpanView(s_cursorsNotAllowed); break;
		case WindowLayerFlags::CursorGrab: return makeSpanView(s_cursorsGrab); break;
		case WindowLayerFlags::CursorGrabbing: return makeSpanView(s_cursorsGrabbed); break;
		case WindowLayerFlags::CursorAllScroll: return makeSpanView(s_cursorsAllScroll); break;
		case WindowLayerFlags::CursorZoomIn: return makeSpanView(s_cursorsZoomIn); break;
		case WindowLayerFlags::CursorZoomOut: return makeSpanView(s_cursorsZoomOut); break;
		case WindowLayerFlags::CursorRightPtr: return makeSpanView(s_cursorsRightArrow); break;
		case WindowLayerFlags::CursorPencil: return makeSpanView(s_cursorsPencil); break;
		case WindowLayerFlags::CursorTarget: return makeSpanView(s_cursorsTarget); break;
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
