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

MODULE_XENOLITH_BACKEND_VKGUI_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_XENOLITH_BACKEND_VKGUI_PRECOMPILED_HEADERS :=
MODULE_XENOLITH_BACKEND_VKGUI_SRCS_DIRS := $(XENOLITH_MODULE_DIR)/backend/vkgui
MODULE_XENOLITH_BACKEND_VKGUI_SRCS_OBJS :=
MODULE_XENOLITH_BACKEND_VKGUI_INCLUDES_DIRS := 
MODULE_XENOLITH_BACKEND_VKGUI_INCLUDES_OBJS := $(XENOLITH_MODULE_DIR)/backend/vkgui
MODULE_XENOLITH_BACKEND_VKGUI_DEPENDS_ON := xenolith_backend_vk xenolith_scene xenolith_platform

#spec

MODULE_XENOLITH_BACKEND_VKGUI_SHARED_SPEC_SUMMARY := Xenolith GUI interface for Vulkan API

define MODULE_XENOLITH_BACKEND_VKGUI_SHARED_SPEC_DESCRIPTION
Module libxenolith-backend-vkgui implements window system dependent GUI output support.
endef

# module name resolution
MODULE_xenolith_backend_vkgui := MODULE_XENOLITH_BACKEND_VKGUI
