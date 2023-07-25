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

#include "XLVkPlatform.h"

#if LINUX

#include <dlfcn.h>

namespace stappler::xenolith::vk::platform {

struct FunctionTable : public vk::LoaderTable {
	using LoaderTable::LoaderTable;

	operator bool () const {
		return vkGetInstanceProcAddr != nullptr
			&& vkCreateInstance != nullptr
			&& vkEnumerateInstanceExtensionProperties != nullptr
			&& vkEnumerateInstanceLayerProperties != nullptr;
	}
};

static uint32_t s_InstanceVersion = 0;
static Vector<VkLayerProperties> s_InstanceAvailableLayers;
static Vector<VkExtensionProperties> s_InstanceAvailableExtensions;

/*SPUNUSED static VKAPI_ATTR VkBool32 VKAPI_CALL s_debugMessageCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);*/

Rc<core::Instance> createInstance(const Callback<bool(VulkanInstanceData &, const VulkanInstanceInfo &)> &cb) {
	auto handle = ::dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_LOCAL);
	if (!handle) {
		stappler::log::text("Vk", "Fail to open libvulkan.so.1");
		return nullptr;
	}

	auto getInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(handle, "vkGetInstanceProcAddr");
	if (!getInstanceProcAddr) {
		return nullptr;
	}

	FunctionTable table(getInstanceProcAddr);

	if (!table) {
		::dlclose(handle);
		return nullptr;
	}

	if (table.vkEnumerateInstanceVersion) {
		table.vkEnumerateInstanceVersion(&s_InstanceVersion);
	} else {
		s_InstanceVersion = VK_API_VERSION_1_0;
	}

	VulkanInstanceInfo info;
	VulkanInstanceData data;
	data.targetVulkanVersion = info.targetVersion = s_InstanceVersion;

	uint32_t layerCount = 0;
	table.vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	s_InstanceAvailableLayers.resize(layerCount);
	table.vkEnumerateInstanceLayerProperties(&layerCount, s_InstanceAvailableLayers.data());

	uint32_t extensionCount = 0;
	table.vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	s_InstanceAvailableExtensions.resize(extensionCount);
	table.vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, s_InstanceAvailableExtensions.data());

	info.availableLayers = s_InstanceAvailableLayers;
	info.availableExtensions = s_InstanceAvailableExtensions;

	if constexpr (vk::s_enableValidationLayers) {
		for (const char *layerName : vk::s_validationLayers) {
			bool layerFound = false;

			for (const auto &layerProperties : s_InstanceAvailableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					data.layersToEnable.emplace_back(layerName);
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				log::format("Vk", "Required validation layer not found: %s", layerName);
				return nullptr;
			}
		}
	}

	const char *debugExt = nullptr;

	for (auto &extension : s_InstanceAvailableExtensions) {
		if constexpr (vk::s_enableValidationLayers) {
			if (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, extension.extensionName) == 0) {
				data.extensionsToEnable.emplace_back(extension.extensionName);
				debugExt = extension.extensionName;
				continue;
			}
		}
	}

	bool completeExt = true;
	if constexpr (vk::s_enableValidationLayers) {
		if (!debugExt) {
			log::format("Vk", "Required extension not found: %s", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			completeExt = false;
		}
	}

	if (!cb(data, info)) {
		log::text("Vk", "VkInstance creation was aborted by client");
		::dlclose(handle);
		return nullptr;
	}

	for (auto &it : vk::s_requiredExtension) {
		if (!it) {
			break;
		}

		for (auto &extension : data.extensionsToEnable) {
			if (strcmp(it, extension) == 0) {
				continue;
			}
		}

		bool found = false;
		for (auto &extension : s_InstanceAvailableExtensions) {
			if (strcmp(it, extension.extensionName) == 0) {
				found = true;
				data.extensionsToEnable.emplace_back(it);
			}
		}
		if (!found) {
			log::format("Vk", "Required extension not found: %s", it);
			completeExt = false;
		}
	}

	if (!completeExt) {
		log::text("Vk", "Not all required extensions found, fail to create VkInstance");
		::dlclose(handle);
		return nullptr;
	}

	Vector<StringView> enabledOptionals;
	for (auto &opt : s_optionalExtension) {
		if (!opt) {
			break;
		}
		bool found = false;
		for (auto &it : data.extensionsToEnable) {
			if (strcmp(it, opt) == 0) {
				enabledOptionals.emplace_back(StringView(opt));
				found = true;
				break;
			}
		}
		if (!found) {
			data.extensionsToEnable.emplace_back(opt);
		}
	}

	debugExt = nullptr;
	for (auto &it : data.extensionsToEnable) {
		if (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, it) == 0) {
			debugExt = it;
			break;
		}
	}

	bool validationEnabled = false;
	for (auto &v : s_validationLayers) {
		for (auto &it : data.layersToEnable) {
			if (strcmp(it, v) == 0) {
				validationEnabled = true;
				break;
			}
		}
	}

	VkInstance instance;

	VkApplicationInfo appInfo{}; vk::sanitizeVkStruct(appInfo);
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = data.applicationName.data();
	appInfo.applicationVersion = data.applicationVersion;
	appInfo.pEngineName = xenolith::platform::name();
	appInfo.engineVersion = xenolith::platform::version();
	appInfo.apiVersion = data.targetVulkanVersion;

	VkInstanceCreateInfo createInfo{}; vk::sanitizeVkStruct(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.pApplicationInfo = &appInfo;

	createInfo.enabledExtensionCount = data.extensionsToEnable.size();
	createInfo.ppEnabledExtensionNames = data.extensionsToEnable.data();

	enum VkResult ret = VK_SUCCESS;
	if (validationEnabled) {
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
		debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugCreateInfo.pfnUserCallback = s_debugMessageCallback;
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
	} else{
		createInfo.pNext = nullptr;
	}

	createInfo.enabledLayerCount = data.layersToEnable.size();
	createInfo.ppEnabledLayerNames = data.layersToEnable.data();
	ret = table.vkCreateInstance(&createInfo, nullptr, &instance);

	if (ret != VK_SUCCESS) {
		log::text("Vk", "Fail to create Vulkan instance");
		return nullptr;
	}

	auto vkInstance = Rc<vk::Instance>::alloc(instance, table.vkGetInstanceProcAddr, data.targetVulkanVersion, move(enabledOptionals),
			[handle] {
		::dlclose(handle);
	}, move(data.checkPresentationSupport), validationEnabled && (debugExt != nullptr), move(data.userdata));

	if constexpr (vk::s_printVkInfo) {
		StringStream out;
		out << "\n\tLayers:\n";
		for (const auto &layerProperties : s_InstanceAvailableLayers) {
			out << "\t\t" << layerProperties.layerName << " ("
					<< getVersionDescription(layerProperties.specVersion) << "/"
					<< getVersionDescription(layerProperties.implementationVersion)
					<< ")\t - " << layerProperties.description << "\n";
		}

		out << "\tExtensions:\n";
		for (const auto &extension : s_InstanceAvailableExtensions) {
			out << "\t\t" << extension.extensionName << ": " << getVersionDescription(extension.specVersion) << "\n";
		}

		vkInstance->printDevicesInfo(out);

		log::text("Vk-Info", out.str());
	}

	return vkInstance;
}

}

#endif
