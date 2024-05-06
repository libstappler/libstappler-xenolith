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

#ifndef XENOLITH_PLATFORM_XLPLATFROMLINUXWAYLANDVIEW_H_
#define XENOLITH_PLATFORM_XLPLATFROMLINUXWAYLANDVIEW_H_

#include "XLPlatformLinuxWayland.h"
#include "XLPlatformLinuxView.h"

#if LINUX
#if XL_ENABLE_WAYLAND

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class WaylandView : public LinuxViewInterface, public WaylandViewInterface {
public:
	static constexpr auto DecorWidth = 20;
	static constexpr auto DecorInset = 16;
	static constexpr auto DecorOffset = 6;
	static constexpr auto IconSize = DecorInset + DecorOffset;

	struct PointerEvent {
		enum Event : uint32_t {
			None,
			Enter,
			Leave,
			Motion,
			Button,
			Axis,
			AxisSource,
			AxisStop,
			AxisDiscrete
		};

		Event event;
		union {
			struct {
				wl_fixed_t x;
				wl_fixed_t y;
			} enter;
			struct {
				uint32_t time;
				wl_fixed_t x;
				wl_fixed_t y;
			} motion;
			struct {
				uint32_t serial;
				uint32_t time;
				uint32_t button;
				uint32_t state;
			} button;
			struct {
				uint32_t time;
				uint32_t axis;
				wl_fixed_t value;
			} axis;
			struct {
				uint32_t axis_source;
			} axisSource;
			struct {
				uint32_t time;
				uint32_t axis;
			} axisStop;
			struct {
				uint32_t axis;
				int32_t discrete;
			} axisDiscrete;
		};
	};

	WaylandView(WaylandLibrary *, ViewInterface *, StringView windowName, StringView bundleName, URect);
	virtual ~WaylandView();

	virtual bool poll(bool frameReady) override;

	virtual int getSocketFd() const override;

	virtual uint64_t getScreenFrameInterval() const override;

	virtual void mapWindow() override;

	WaylandDisplay *getDisplay() const { return _display; }
	wl_surface *getSurface() const { return _surface; }

	void handleSurfaceEnter(wl_surface *surface, wl_output *output);
	void handleSurfaceLeave(wl_surface *surface, wl_output *output);

	void handleSurfaceConfigure(xdg_surface *, uint32_t serial);
	void handleToplevelConfigure(xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, wl_array *states);
	void handleToplevelClose(xdg_toplevel *xdg_toplevel);
	void handleToplevelBounds(xdg_toplevel *xdg_toplevel, int32_t width, int32_t height);
	void handleSurfaceFrameDone(wl_callback *wl_callback, uint32_t callback_data);

	virtual void handlePointerEnter(wl_fixed_t surface_x, wl_fixed_t surface_y) override;
	virtual void handlePointerLeave() override;
	virtual void handlePointerMotion(uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) override;
	virtual void handlePointerButton(uint32_t serial, uint32_t time, uint32_t button, uint32_t state) override;
	virtual void handlePointerAxis(uint32_t time, uint32_t axis, wl_fixed_t value) override;
	virtual void handlePointerAxisSource(uint32_t axis_source) override;
	virtual void handlePointerAxisStop(uint32_t time, uint32_t axis) override;
	virtual void handlePointerAxisDiscrete(uint32_t axis, int32_t discrete) override;
	virtual void handlePointerFrame() override;

	virtual void handleKeyboardEnter(Vector<uint32_t> &&, uint32_t depressed, uint32_t latched, uint32_t locked) override;
	virtual void handleKeyboardLeave() override;
	virtual void handleKey(uint32_t time, uint32_t key, uint32_t state) override;
	virtual void handleKeyModifiers(uint32_t depressed, uint32_t latched, uint32_t locked) override;
	void handleKeyRepeat();

	virtual void handleDecorationPress(WaylandDecoration *, uint32_t serial, bool released = false) override;

	virtual void scheduleFrame() override;

	virtual void onSurfaceInfo(core::SurfaceInfo &) const override;

	virtual void commit(uint32_t width, uint32_t height) override;

	virtual void readFromClipboard(Function<void(BytesView, StringView)> &&, Ref *) override;
	virtual void writeToClipboard(BytesView, StringView contentType) override;

protected:
	void createDecorations();

	ViewInterface *_view = nullptr;

	xdg_surface *_xdgSurface = nullptr;
	xdg_toplevel *_toplevel = nullptr;
	Extent2 _currentExtent;
	Extent2 _commitedExtent;

	bool _continuousRendering = true;
	bool _scheduleNext = false;
	bool _clientSizeDecoration = true;
	bool _shouldClose = false;
	bool _surfaceDirty = false;
	bool _fullscreen = false;
	bool _pointerInit = false;

	Set<WaylandOutput *> _activeOutputs;

	double _surfaceX = 0.0;
	double _surfaceY = 0.0;
	core::InputModifier _activeModifiers = core::InputModifier::None;
	Vector<PointerEvent> _pointerEvents;

	std::bitset<size_t(XDG_TOPLEVEL_STATE_TILED_BOTTOM + 1)> _state;
	Vector<Rc<WaylandDecoration>> _decors;
	Rc<WaylandDecoration> _iconMaximized;

	uint32_t _configureSerial = maxOf<uint32_t>();
	uint64_t _screenFrameInterval = 0;

	struct KeyData {
		uint32_t scancode;
		char32_t codepoint;
		uint64_t time;
		bool repeats;
		uint64_t lastRepeat = 0;
	};

	Map<uint32_t, KeyData> _keys;
};


}

#endif
#endif

#endif /* XENOLITH_PLATFORM_XLPLATFROMLINUXWAYLANDVIEW_H_ */
