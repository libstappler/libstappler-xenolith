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

MODULE_XENOLITH_BACKEND_VK_PRECOMPILED_HEADERS :=
MODULE_XENOLITH_BACKEND_VK_SRCS_DIRS := $(XENOLITH_MODULE_DIR)/backend/vk
MODULE_XENOLITH_BACKEND_VK_SRCS_OBJS :=
MODULE_XENOLITH_BACKEND_VK_INCLUDES_DIRS := 
MODULE_XENOLITH_BACKEND_VK_INCLUDES_OBJS := $(XENOLITH_MODULE_DIR)/backend/vk
MODULE_XENOLITH_BACKEND_VK_DEPENDS_ON := xenolith_core

# module name resolution
MODULE_xenolith_backend_vk := MODULE_XENOLITH_BACKEND_VK

ifdef VULKAN_SDK_PREFIX
MODULE_XENOLITH_BACKEND_VK_INCLUDES_OBJS += $(call sp_os_path,$(VULKAN_SDK_PREFIX)/include)
endif

ifdef MACOS

MODULE_XENOLITH_BACKEND_VK_GENERAL_LDFLAGS := -framework Cocoa -framework Metal -framework Quartz

$(info VULKAN_SDK_PREFIX $(VULKAN_SDK_PREFIX) $(realpath $(VULKAN_SDK_PREFIX)/lib/libvulkan.1.dylib))
VULKAN_LOADER_PATH = $(realpath $(VULKAN_SDK_PREFIX)/lib/libvulkan.1.dylib)
VULKAN_MOLTENVK_PATH = $(realpath $(VULKAN_SDK_PREFIX)/lib/libMoltenVK.dylib)
VULKAN_MOLTENVK_ICD_PATH = $(realpath $(VULKAN_SDK_PREFIX)/share/vulkan/icd.d/MoltenVK_icd.json)
VULKAN_LAYERS_PATH = $(realpath $(VULKAN_SDK_PREFIX)/share/vulkan/explicit_layer.d)
VULKAN_LIBDIR = $(dir $(BUILD_EXECUTABLE))vulkan

$(VULKAN_LIBDIR)/lib/libvulkan.dylib: $(VULKAN_LOADER_PATH)
	@$(GLOBAL_MKDIR) $(VULKAN_LIBDIR)/lib
	cp $(VULKAN_LOADER_PATH) $(VULKAN_LIBDIR)/lib/libvulkan.dylib

$(VULKAN_LIBDIR)/lib/libMoltenVK.dylib: $(VULKAN_MOLTENVK_PATH)
	@$(GLOBAL_MKDIR) $(VULKAN_LIBDIR)/lib
	cp $(VULKAN_MOLTENVK_PATH) $(VULKAN_LIBDIR)/lib/libMoltenVK.dylib

$(VULKAN_LIBDIR)/icd.d/MoltenVK_icd.json: $(VULKAN_MOLTENVK_ICD_PATH)
	@$(GLOBAL_MKDIR) $(VULKAN_LIBDIR)/icd.d
	sed 's/..\/..\/..\/lib\/libMoltenVK/..\/lib\/libMoltenVK/g' $(VULKAN_MOLTENVK_ICD_PATH) >  $(VULKAN_LIBDIR)/icd.d/MoltenVK_icd.json

$(VULKAN_LIBDIR)/explicit_layer.d/%.json: $(VULKAN_LAYERS_PATH)/%.json
	@$(GLOBAL_MKDIR) $(VULKAN_LIBDIR)/explicit_layer.d
	sed 's/..\/..\/..\/lib\/libVkLayer/..\/lib\/libVkLayer/g' $(VULKAN_LAYERS_PATH)/$*.json > $(VULKAN_LIBDIR)/explicit_layer.d/$*.json

$(VULKAN_LIBDIR)/lib/%.dylib: $(VULKAN_SDK_PREFIX)/lib/%.dylib
	@$(GLOBAL_MKDIR) $(VULKAN_LIBDIR)/lib
	cp $(VULKAN_SDK_PREFIX)/lib/$*.dylib $(VULKAN_LIBDIR)/lib/$*.dylib

xenolith-install-loader: $(VULKAN_LIBDIR)/lib/libvulkan.dylib $(VULKAN_LIBDIR)/lib/libMoltenVK.dylib $(VULKAN_LIBDIR)/icd.d/MoltenVK_icd.json \
	$(subst $(VULKAN_LAYERS_PATH),$(VULKAN_LIBDIR)/explicit_layer.d,$(wildcard $(VULKAN_LAYERS_PATH)/*.json)) \
	$(subst $(VULKAN_SDK_PREFIX),$(VULKAN_LIBDIR),$(wildcard $(VULKAN_SDK_PREFIX)/lib/libVkLayer_*.dylib))

$(BUILD_EXECUTABLE): xenolith-install-loader
$(BUILD_SHARED_LIBRARY): xenolith-install-loader
$(BUILD_STATIC_LIBRARY): xenolith-install-loader

.PHONY: xenolith-install-loader

endif
