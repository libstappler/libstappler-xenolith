# Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

MODULE_XENOLITH_RENDERER_BASIC2D_PRECOMPILED_HEADERS :=
MODULE_XENOLITH_RENDERER_BASIC2D_SRCS_DIRS := $(XENOLITH_MODULE_DIR)/renderer/basic2d
MODULE_XENOLITH_RENDERER_BASIC2D_SRCS_OBJS :=
MODULE_XENOLITH_RENDERER_BASIC2D_INCLUDES_DIRS := 
MODULE_XENOLITH_RENDERER_BASIC2D_INCLUDES_OBJS := $(XENOLITH_MODULE_DIR)/renderer/basic2d
MODULE_XENOLITH_RENDERER_BASIC2D_SHADERS_DIR := $(XENOLITH_MODULE_DIR)/renderer/basic2d/glsl/shaders
MODULE_XENOLITH_RENDERER_BASIC2D_SHADERS_INCLUDE := $(XENOLITH_MODULE_DIR)/renderer/basic2d/glsl/include
MODULE_XENOLITH_RENDERER_BASIC2D_DEPENDS_ON := xenolith_scene xenolith_font

# module name resolution
MODULE_xenolith_renderer_basic2d := MODULE_XENOLITH_RENDERER_BASIC2D
