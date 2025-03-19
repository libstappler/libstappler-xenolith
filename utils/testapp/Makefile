# Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

STAPPLER_BUILD_ROOT ?= ../../../build/make

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

LOCAL_OUTDIR ?= stappler-build
LOCAL_EXECUTABLE := testapp

LOCAL_ROOT = .

LOCAL_SRCS_DIRS :=  src proj.android/src
LOCAL_SRCS_OBJS :=

LOCAL_INCLUDES_DIRS := src
LOCAL_INCLUDES_OBJS :=

LOCAL_MAIN := main.cpp

LOCAL_MODULES_PATHS = \
	core/stappler-modules.mk \
	xenolith/xenolith-modules.mk

LOCAL_MODULES ?= \
	xenolith_backend_vkgui \
	xenolith_renderer_material2d \
	xenolith_resources_assets \
	stappler_backtrace

# Android config
LOCAL_ANDROID_MK := proj.android/app/Android.mk # path to root Android.mk
LOCAL_APPLICATION_MK := proj.android/app/Application.mk # path to Application.mk
LOCAL_ANDROID_TARGET := application # target to build
LOCAL_ANDROID_PLATFORM := android-24 # android platform version

#ASAN := 1

include $(STAPPLER_BUILD_ROOT)/universal.mk

prepare-android:
	@mkdir -p proj.android/app/assets
	rsync -ar resources proj.android/app/assets

ifdef STAPPLER_TARGET

local-shaders-clean:
	@$(GLOBAL_RM) $(subst /glsl/,/compiled/shader_,$(LOCAL_SHADERS))

ifdef MACOS_ARCH
$(BUILD_EXECUTABLE): xenolith-install-loader
endif

android-export: prepare-android

endif

.PHONY: prepare-linux prepare-android
