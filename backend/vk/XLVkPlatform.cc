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

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

SPUNUSED static VKAPI_ATTR VkBool32 VKAPI_CALL s_debugMessageCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk::platform {

static uint32_t s_InstanceVersion = 0;
static Vector<VkLayerProperties> s_InstanceAvailableLayers;
static Vector<VkExtensionProperties> s_InstanceAvailableExtensions;

Rc<Instance> FunctionTable::createInstance(const Callback<bool(VulkanInstanceData &, const VulkanInstanceInfo &)> &setupCb, Dso &&vulkanModule, Instance::TerminateCallback &&termCb) const {
	VulkanInstanceInfo info = loadInfo();
	VulkanInstanceData data;

	if (!prepareData(data, info)) {
		return nullptr;
	}

	if (!setupCb(data, info)) {
		log::warn("Vk", "VkInstance creation was aborted by client");
		return nullptr;
	}

	if (!validateData(data, info)) {
		return nullptr;
	}

	return doCreateInstance(data, move(vulkanModule), move(termCb));
}

VulkanInstanceInfo FunctionTable::loadInfo() const {
	VulkanInstanceInfo ret;

	if (s_InstanceVersion == 0) {
		if (vkEnumerateInstanceVersion) {
			vkEnumerateInstanceVersion(&s_InstanceVersion);
		} else {
			s_InstanceVersion = VK_API_VERSION_1_0;
		}
	}

	if (s_InstanceAvailableLayers.empty()) {
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		s_InstanceAvailableLayers.resize(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, s_InstanceAvailableLayers.data());
	}

	if (s_InstanceAvailableExtensions.empty()) {
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		s_InstanceAvailableExtensions.resize(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, s_InstanceAvailableExtensions.data());
	}

	ret.targetVersion = s_InstanceVersion;
	ret.availableLayers = s_InstanceAvailableLayers;
	ret.availableExtensions = s_InstanceAvailableExtensions;

	return ret;
}

bool FunctionTable::prepareData(VulkanInstanceData &data, const VulkanInstanceInfo &info) const {
	data.targetVulkanVersion = info.targetVersion;

	bool layerFound = false;
	if constexpr (vk::s_enableValidationLayers) {
		for (const char *layerName : vk::s_validationLayers) {

			for (const auto &layerProperties : s_InstanceAvailableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					data.layersToEnable.emplace_back(layerName);
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				log::error("Vk", "Validation layer not found: ", layerName);
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

	if constexpr (vk::s_enableValidationLayers) {
		if (!debugExt && layerFound) {
			for (const auto &layerName : vk::s_validationLayers) {
				uint32_t layer_ext_count;
				vkEnumerateInstanceExtensionProperties(layerName, &layer_ext_count, nullptr);
				if (layer_ext_count == 0) {
					continue;
				}

				VkExtensionProperties layer_exts[layer_ext_count];
				vkEnumerateInstanceExtensionProperties(layerName, &layer_ext_count, layer_exts);

				for (auto &extension : layer_exts) {
					if (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, extension.extensionName) == 0) {
						data.extensionsToEnable.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
						debugExt = extension.extensionName;
						break;
					}
				}

				if (debugExt) {
					break;
				}
			}
		}

		if (!debugExt) {
			log::error("Vk", "Required extension not found: ", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
	}

	return true;
}

bool FunctionTable::validateData(VulkanInstanceData &data, const VulkanInstanceInfo &info) const {
	bool completeExt = true;
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
		for (auto &extension : info.availableExtensions) {
			if (strcmp(it, extension.extensionName) == 0) {
				found = true;
				data.extensionsToEnable.emplace_back(it);
			}
		}
		if (!found) {
			log::error("Vk", "Required extension not found: ", it);
			completeExt = false;
		}
	}

	if (!completeExt) {
		log::error("Vk", "Not all required extensions found, fail to create VkInstance");
		return false;
	}

	return true;
}

Rc<Instance> FunctionTable::doCreateInstance(VulkanInstanceData &data, Dso &&vulkanModule, Instance::TerminateCallback &&cb) const {
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

	const char *debugExt = nullptr;
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

	VkInstance instance = VK_NULL_HANDLE;

	VkApplicationInfo appInfo{}; vk::sanitizeVkStruct(appInfo);
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = data.applicationName.data();
	appInfo.applicationVersion = XL_MAKE_API_VERSION(data.applicationVersion);
	appInfo.pEngineName = xenolith::platform::name();
	appInfo.engineVersion = xenolith::platform::version();
	appInfo.apiVersion = data.targetVulkanVersion;

	VkInstanceCreateInfo createInfo{}; vk::sanitizeVkStruct(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext = nullptr;
#if MACOS
	createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#else
	createInfo.flags = 0;
#endif
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
		createInfo.pNext = &debugCreateInfo;
	} else{
		createInfo.pNext = nullptr;
	}

	createInfo.enabledLayerCount = data.layersToEnable.size();
	createInfo.ppEnabledLayerNames = data.layersToEnable.data();
	ret = vkCreateInstance(&createInfo, nullptr, &instance);

	if (ret != VK_SUCCESS) {
		log::error("Vk", "Fail to create Vulkan instance");
		return nullptr;
	}

	auto vkInstance = Rc<vk::Instance>::alloc(instance, vkGetInstanceProcAddr, data.targetVulkanVersion, move(enabledOptionals),
			move(vulkanModule), move(cb), move(data.checkPresentationSupport), validationEnabled && (debugExt != nullptr), move(data.userdata));

	if constexpr (vk::s_printVkInfo) {
		StringStream out;
		out << "\n\tVulkan: " << getVersionDescription(s_InstanceVersion) << "\n\tLayers:\n";
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

		log::verbose("Vk-Info", out.str());
	}

	return vkInstance;
}

}
