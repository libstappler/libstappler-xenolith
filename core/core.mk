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

MODULE_XENOLITH_CORE_PRECOMPILED_HEADERS := $(XENOLITH_MODULE_DIR)/core/XLCommon.h
MODULE_XENOLITH_CORE_SRCS_DIRS := $(XENOLITH_MODULE_DIR)/core $(XENOLITH_MODULE_DIR)/thirdparty
MODULE_XENOLITH_CORE_SRCS_OBJS :=
MODULE_XENOLITH_CORE_INCLUDES_DIRS := 
MODULE_XENOLITH_CORE_INCLUDES_OBJS := $(XENOLITH_MODULE_DIR)/core $(XENOLITH_MODULE_DIR)/thirdparty
MODULE_XENOLITH_CORE_DEPENDS_ON := stappler_bitmap stappler_threads stappler_geom stappler_backtrace

# module name resolution
MODULE_xenolith_core := MODULE_XENOLITH_CORE