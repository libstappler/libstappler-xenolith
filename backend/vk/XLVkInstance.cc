/**
 Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#include "XLVkInstance.h"
#include "XLVk.h"
#include "XLVkInfo.h"
#include "XLVkLoop.h"
#include "XLCoreDevice.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

SPUNUSED static VkResult s_createDebugUtilsMessengerEXT(VkInstance instance,
		const PFN_vkGetInstanceProcAddr getInstanceProcAddr,
		const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)getInstanceProcAddr(instance,
			"vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

SPUNUSED static VKAPI_ATTR VkBool32 VKAPI_CALL s_debugMessageCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
	if (pCallbackData->pMessageIdName
			&& strcmp(pCallbackData->pMessageIdName,
					   "VUID-VkSwapchainCreateInfoKHR-imageExtent-01274")
					== 0) {
		// this is normal for multithreaded engine
		messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	}
	if (pCallbackData->pMessageIdName
			&& strcmp(pCallbackData->pMessageIdName, "Loader Message") == 0) {
		if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
			if (StringView(pCallbackData->pMessage).starts_with("Instance Extension: ")
					|| StringView(pCallbackData->pMessage).starts_with("Device Extension: ")) {
				return VK_FALSE;
			}
			log::verbose("Vk-Validation-Verbose", "[", pCallbackData->pMessageIdName, "] ",
					pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
			log::info("Vk-Validation-Info", "[", pCallbackData->pMessageIdName, "] ",
					pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			log::warn("Vk-Validation-Warning", "[", pCallbackData->pMessageIdName, "] ",
					pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			log::error("Vk-Validation-Error", "[", pCallbackData->pMessageIdName, "] ",
					pCallbackData->pMessage);
		}
		return VK_FALSE;
	} else {
		if (messageSeverity < XL_VK_MIN_MESSAGE_SEVERITY) {
			return VK_FALSE;
		}

		if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
			if (StringView(pCallbackData->pMessage).starts_with("Device Extension: ")) {
				return VK_FALSE;
			}
			log::verbose("Vk-Validation-Verbose", "[",
					pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "(null)", "] ",
					pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
			log::info("Vk-Validation-Info", "[",
					pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "(null)", "] ",
					pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			log::warn("Vk-Validation-Warning", "[",
					pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "(null)", "] ",
					pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			log::error("Vk-Validation-Error", "[",
					pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "(null)", "] ",
					pCallbackData->pMessage);
		}
		return VK_FALSE;
	}
}

Instance::Instance(VkInstance inst, const PFN_vkGetInstanceProcAddr getInstanceProcAddr,
		uint32_t targetVersion, OptVec &&optionals, Dso &&vulkanModule,
		TerminateCallback &&terminate, PresentSupportCallback &&present, bool validationEnabled,
		Rc<Ref> &&userdata)
: core::Instance(sp::move(vulkanModule), sp::move(terminate), sp::move(userdata))
, InstanceTable(getInstanceProcAddr, inst)
, _instance(inst)
, _version(targetVersion)
, _optionals(sp::move(optionals))
, _checkPresentSupport(sp::move(present)) {
	if (validationEnabled) {
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
		debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugCreateInfo.pNext = nullptr;
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

		debugCreateInfo.pfnUserCallback = s_debugMessageCallback;
		debugCreateInfo.pUserData = this;

		if (s_createDebugUtilsMessengerEXT(_instance, vkGetInstanceProcAddr, &debugCreateInfo,
					nullptr, &debugMessenger)
				!= VK_SUCCESS) {
			log::warn("Vk", "failed to set up debug messenger!");
		}
	}

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);

	if (deviceCount) {
		Vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

		for (auto &device : devices) {
			auto &it = _devices.emplace_back(getDeviceInfo(device));

			_availableDevices.emplace_back(core::DeviceProperties{
				String(it.properties.device10.properties.deviceName),
				it.properties.device10.properties.apiVersion,
				it.properties.device10.properties.driverVersion, it.supportsPresentation()});
		}
	} else {
		log::info("Vk", "No devices available on this instance");
	}
}

Instance::~Instance() {
	if constexpr (s_enableValidationLayers) {
		if (debugMessenger != VK_NULL_HANDLE) {
			vkDestroyDebugUtilsMessengerEXT(_instance, debugMessenger, nullptr);
		}
	}
	vkDestroyInstance(_instance, nullptr);
}

Rc<core::Loop> Instance::makeLoop(event::Looper *looper, core::LoopInfo &&info) const {
	return Rc<vk::Loop>::create(looper, Rc<Instance>(const_cast<Instance *>(this)), move(info));
}

Rc<Device> Instance::makeDevice(const core::LoopInfo &info) const {
	auto data = info.platformData.cast<LoopData>().get();
	if (!data) {
		log::error("vk::Instance", "Fail to create device: loop platform data is not defined");
		return nullptr;
	}

	auto isDeviceSupported = [&](const DeviceInfo &dev) {
		if (data->deviceSupportCallback) {
			if (!data->deviceSupportCallback(dev)) {
				return false;
			}
		} else {
			if (!dev.supportsPresentation()) {
				return false;
			}
		}
		return true;
	};

	auto getDeviceExtensions = [&](const DeviceInfo &dev) {
		Vector<StringView> requiredExtensions;
		if (data->deviceExtensionsCallback) {
			requiredExtensions = data->deviceExtensionsCallback(dev);
		}

		for (auto &ext : s_requiredDeviceExtensions) {
			if (ext
					&& !isPromotedExtension(dev.properties.device10.properties.apiVersion,
							StringView(ext))) {
				requiredExtensions.emplace_back(ext);
			}
		}

		for (auto &ext : dev.optionalExtensions) { requiredExtensions.emplace_back(ext); }

		for (auto &ext : dev.promotedExtensions) {
			if (!isPromotedExtension(dev.properties.device10.properties.apiVersion, ext)) {
				requiredExtensions.emplace_back(ext);
			}
		}

		return requiredExtensions;
	};

	auto isExtensionsSupported = [&](const DeviceInfo &dev,
										 const Vector<StringView> &requiredExtensions) {
		if (!requiredExtensions.empty()) {
			bool found = true;
			for (auto &req : requiredExtensions) {
				auto iit = std::find(dev.availableExtensions.begin(), dev.availableExtensions.end(),
						req);
				if (iit == dev.availableExtensions.end()) {
					found = false;
					break;
				}
			}
			if (!found) {
				return false;
			}
		}
		return true;
	};

	auto buildFeaturesList = [&](const DeviceInfo &dev, DeviceInfo::Features &features) {
		if (data->deviceFeaturesCallback) {
			features = data->deviceFeaturesCallback(dev);
		}

		features.enableFromFeatures(DeviceInfo::Features::getRequired());

		if (!dev.features.canEnable(features, dev.properties.device10.properties.apiVersion)) {
			return false;
		}

		features.enableFromFeatures(DeviceInfo::Features::getOptional());
		features.disableFromFeatures(dev.features);
		features.optionals = dev.features.optionals;
		return true;
	};

	if (info.deviceIdx == DefaultDevice) {
		for (auto &it : _devices) {
			if (!isDeviceSupported(it)) {
				log::warn("vk::Instance", "Device rejected: device is not supported");
				continue;
			}

			auto requiredExtensions = getDeviceExtensions(it);
			if (!isExtensionsSupported(it, requiredExtensions)) {
				log::warn("vk::Instance", "Device rejected: required extensions is not available");
				continue;
			}

			DeviceInfo::Features targetFeatures;
			if (!buildFeaturesList(it, targetFeatures)) {
				log::warn("vk::Instance", "Device rejected: required features is not available");
				continue;
			}

			if (it.features.canEnable(targetFeatures,
						it.properties.device10.properties.apiVersion)) {
				return Rc<vk::Device>::create(this, DeviceInfo(it), targetFeatures,
						requiredExtensions);
			}
		}
	} else if (info.deviceIdx < _devices.size()) {
		auto &dev = _devices[info.deviceIdx];
		if (!isDeviceSupported(dev)) {
			log::error("vk::Instance", "Fail to create device: device is not supported");
			return nullptr;
		}

		auto requiredExtensions = getDeviceExtensions(dev);
		if (!isExtensionsSupported(dev, requiredExtensions)) {
			log::error("vk::Instance",
					"Fail to create device: required extensions is not available");
			return nullptr;
		}

		DeviceInfo::Features targetFeatures;
		if (!buildFeaturesList(dev, targetFeatures)) {
			log::error("vk::Instance", "Fail to create device: required features is not available");
			return nullptr;
		}

		if (dev.features.canEnable(targetFeatures, dev.properties.device10.properties.apiVersion)) {
			return Rc<vk::Device>::create(this, DeviceInfo(dev), targetFeatures,
					requiredExtensions);
		}
	}

	log::error("vk::Instance", "Fail to create device: no acceptable devices found");
	return nullptr;
}

static core::PresentMode getGlPresentMode(VkPresentModeKHR presentMode) {
	switch (presentMode) {
	case VK_PRESENT_MODE_IMMEDIATE_KHR: return core::PresentMode::Immediate; break;
	case VK_PRESENT_MODE_MAILBOX_KHR: return core::PresentMode::Mailbox; break;
	case VK_PRESENT_MODE_FIFO_KHR: return core::PresentMode::Fifo; break;
	case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return core::PresentMode::FifoRelaxed; break;
	default: return core::PresentMode::Unsupported; break;
	}
}

core::SurfaceInfo Instance::getSurfaceOptions(VkSurfaceKHR surface, VkPhysicalDevice device) const {
	core::SurfaceInfo ret;

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (formatCount != 0) {
		Vector<VkSurfaceFormatKHR> formats;
		formats.resize(formatCount);

		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());

		ret.formats.reserve(formatCount);
		for (auto &it : formats) {
			ret.formats.emplace_back(core::ImageFormat(it.format), core::ColorSpace(it.colorSpace));
		}
	}

	if (presentModeCount != 0) {
		ret.presentModes.reserve(presentModeCount);
		Vector<VkPresentModeKHR> modes;
		modes.resize(presentModeCount);

		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, modes.data());

		for (auto &it : modes) { ret.presentModes.emplace_back(getGlPresentMode(it)); }

		std::sort(ret.presentModes.begin(), ret.presentModes.end(),
				[&](core::PresentMode l, core::PresentMode r) { return toInt(l) > toInt(r); });
	}

	VkSurfaceCapabilities2KHR caps;
	caps.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
	caps.pNext = nullptr;

	// index into s_optionalExtension
	if (_optionals[0]) {
		VkPhysicalDeviceSurfaceInfo2KHR info;
		info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
		info.surface = surface;
		info.pNext = nullptr;

		vkGetPhysicalDeviceSurfaceCapabilities2KHR(device, &info, &caps);
	} else {
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &caps.surfaceCapabilities);
	}


	ret.minImageCount = caps.surfaceCapabilities.minImageCount;
	ret.maxImageCount = caps.surfaceCapabilities.maxImageCount;
	ret.currentExtent = Extent2(caps.surfaceCapabilities.currentExtent.width,
			caps.surfaceCapabilities.currentExtent.height);
	ret.minImageExtent = Extent2(caps.surfaceCapabilities.minImageExtent.width,
			caps.surfaceCapabilities.minImageExtent.height);
	ret.maxImageExtent = Extent2(caps.surfaceCapabilities.maxImageExtent.width,
			caps.surfaceCapabilities.maxImageExtent.height);
	ret.maxImageArrayLayers = caps.surfaceCapabilities.maxImageArrayLayers;
	ret.supportedTransforms =
			core::SurfaceTransformFlags(caps.surfaceCapabilities.supportedTransforms);
	ret.currentTransform = core::SurfaceTransformFlags(caps.surfaceCapabilities.currentTransform);
	ret.supportedCompositeAlpha =
			core::CompositeAlphaFlags(caps.surfaceCapabilities.supportedCompositeAlpha);
	ret.supportedUsageFlags = core::ImageUsage(caps.surfaceCapabilities.supportedUsageFlags);
	return ret;
}

VkExtent2D Instance::getSurfaceExtent(VkSurfaceKHR surface, VkPhysicalDevice device) const {
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);
	return capabilities.currentExtent;
}

VkInstance Instance::getInstance() const { return _instance; }

void Instance::printDevicesInfo(std::ostream &out) const {
	out << "\n";

	auto getDeviceTypeString = [&](VkPhysicalDeviceType type) -> const char * {
		switch (type) {
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "Discrete GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "Virtual GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU"; break;
		default: return "Other"; break;
		}
		return "Other";
	};

	for (auto &device : _devices) {
		out << "\tDevice: " << device.device << " "
			<< getDeviceTypeString(device.properties.device10.properties.deviceType) << ": "
			<< device.properties.device10.properties.deviceName
			<< " (API: " << getVersionDescription(device.properties.device10.properties.apiVersion)
			<< ", Driver: "
			<< getVersionDescription(device.properties.device10.properties.driverVersion) << ")\n";

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device.device, &queueFamilyCount, nullptr);

		Vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device.device, &queueFamilyCount,
				queueFamilies.data());

		int i = 0;
		for (const VkQueueFamilyProperties &queueFamily : queueFamilies) {
			bool empty = true;
			out << "\t\t[" << i << "] Queue family; Flags: ";
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				if (!empty) {
					out << ", ";
				} else {
					empty = false;
				}
				out << "Graphics";
			}
			if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
				if (!empty) {
					out << ", ";
				} else {
					empty = false;
				}
				out << "Compute";
			}
			if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
				if (!empty) {
					out << ", ";
				} else {
					empty = false;
				}
				out << "Transfer";
			}
			if (queueFamily.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) {
				if (!empty) {
					out << ", ";
				} else {
					empty = false;
				}
				out << "SparseBinding";
			}
			if (queueFamily.queueFlags & VK_QUEUE_PROTECTED_BIT) {
				if (!empty) {
					out << ", ";
				} else {
					empty = false;
				}
				out << "Protected";
			}

			VkBool32 presentSupport =
					_checkPresentSupport ? _checkPresentSupport(this, device.device, i) : false;
			if (presentSupport) {
				if (!empty) {
					out << ", ";
				} else {
					empty = false;
				}
				out << "Present";
			}

			out << "; Count: " << queueFamily.queueCount << "\n";
			i++;
		}
		out << device.description();
	}
}

void Instance::getDeviceFeatures(const VkPhysicalDevice &device, DeviceInfo::Features &features,
		const DeviceInfo::OptVec &flags, uint32_t api) const {
	void *next = nullptr;
#ifdef VK_ENABLE_BETA_EXTENSIONS
	if ((flags & ExtensionFlags::Portability) != ExtensionFlags::None) {
		features.devicePortability.pNext = next;
		next = &features.devicePortability;
	}
#endif
	features.optionals = flags;
#if VK_VERSION_1_3
	if (api >= VK_API_VERSION_1_3) {
		features.device13.pNext = next;
		features.device12.pNext = &features.device13;
		features.device11.pNext = &features.device12;
		features.device10.pNext = &features.device11;

		if (vkGetPhysicalDeviceFeatures2) {
			vkGetPhysicalDeviceFeatures2(device, &features.device10);
		} else if (vkGetPhysicalDeviceFeatures2KHR) {
			vkGetPhysicalDeviceFeatures2KHR(device, &features.device10);
		} else {
			vkGetPhysicalDeviceFeatures(device, &features.device10.features);
		}

		features.updateFrom13();
	} else
#endif
			if (api >= VK_API_VERSION_1_2) {
		features.device12.pNext = next;
		features.device11.pNext = &features.device12;
		features.device10.pNext = &features.device11;

		if (vkGetPhysicalDeviceFeatures2) {
			vkGetPhysicalDeviceFeatures2(device, &features.device10);
		} else if (vkGetPhysicalDeviceFeatures2KHR) {
			vkGetPhysicalDeviceFeatures2KHR(device, &features.device10);
		} else {
			vkGetPhysicalDeviceFeatures(device, &features.device10.features);
		}

		features.updateFrom12();
	} else {
		if (flags[toInt(OptionalDeviceExtension::Storage16Bit)]) {
			features.device16bitStorage.pNext = next;
			next = &features.device16bitStorage;
		}
		if (flags[toInt(OptionalDeviceExtension::Storage8Bit)]) {
			features.device8bitStorage.pNext = next;
			next = &features.device8bitStorage;
		}
		if (flags[toInt(OptionalDeviceExtension::ShaderFloat16Int8)]) {
			features.deviceShaderFloat16Int8.pNext = next;
			next = &features.deviceShaderFloat16Int8;
		}
		if (flags[toInt(OptionalDeviceExtension::DescriptorIndexing)]) {
			features.deviceDescriptorIndexing.pNext = next;
			next = &features.deviceDescriptorIndexing;
		}
		if (flags[toInt(OptionalDeviceExtension::DeviceAddress)]) {
			features.deviceBufferDeviceAddress.pNext = next;
			next = &features.deviceBufferDeviceAddress;
		}
		features.device10.pNext = next;

		if (vkGetPhysicalDeviceFeatures2) {
			vkGetPhysicalDeviceFeatures2(device, &features.device10);
		} else if (vkGetPhysicalDeviceFeatures2KHR) {
			vkGetPhysicalDeviceFeatures2KHR(device, &features.device10);
		} else {
			vkGetPhysicalDeviceFeatures(device, &features.device10.features);
		}

		features.updateTo12(true);
	}

	VkPhysicalDeviceExternalFenceInfo fenceInfo;
	fenceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO;
	fenceInfo.pNext = nullptr;
	fenceInfo.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;

	vkGetPhysicalDeviceExternalFenceProperties(device, &fenceInfo, &features.fenceSyncFd);
}

void Instance::getDeviceProperties(const VkPhysicalDevice &device,
		DeviceInfo::Properties &properties, const DeviceInfo::OptVec &flags, uint32_t api) const {
	void *next = nullptr;
#ifdef VK_ENABLE_BETA_EXTENSIONS
	if ((flags & ExtensionFlags::Portability) != ExtensionFlags::None) {
		properties.devicePortability.pNext = next;
		next = &properties.devicePortability;
	}
#endif
	if (flags[toInt(OptionalDeviceExtension::Maintenance3)]) {
		properties.deviceMaintenance3.pNext = next;
		next = &properties.deviceMaintenance3;
	}
	if (flags[toInt(OptionalDeviceExtension::DescriptorIndexing)]) {
		properties.deviceDescriptorIndexing.pNext = next;
		next = &properties.deviceDescriptorIndexing;
	}

	properties.device10.pNext = next;

	if (vkGetPhysicalDeviceProperties2) {
		vkGetPhysicalDeviceProperties2(device, &properties.device10);
	} else if (vkGetPhysicalDeviceProperties2KHR) {
		vkGetPhysicalDeviceProperties2KHR(device, &properties.device10);
	} else {
		vkGetPhysicalDeviceProperties(device, &properties.device10.properties);
	}
}

DeviceInfo Instance::getDeviceInfo(VkPhysicalDevice device) const {
	DeviceInfo ret;
	uint32_t graphicsFamily = maxOf<uint32_t>();
	uint32_t presentFamily = maxOf<uint32_t>();
	uint32_t transferFamily = maxOf<uint32_t>();
	uint32_t computeFamily = maxOf<uint32_t>();

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	Vector<DeviceInfo::QueueFamilyInfo> queueInfo(queueFamilyCount);
	Vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);

	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const VkQueueFamilyProperties &queueFamily : queueFamilies) {
		auto presentSupport = _checkPresentSupport ? _checkPresentSupport(this, device, i) : false;

		queueInfo[i].index = i;
		queueInfo[i].flags = getQueueFlags(queueFamily.queueFlags, presentSupport);
		queueInfo[i].count = queueFamily.queueCount;
		queueInfo[i].used = 0;
		queueInfo[i].timestampValidBits = queueFamily.timestampValidBits;
		queueInfo[i].minImageTransferGranularity =
				Extent3(queueFamily.minImageTransferGranularity.width,
						queueFamily.minImageTransferGranularity.height,
						queueFamily.minImageTransferGranularity.depth);
		queueInfo[i].presentSurfaceMask = presentSupport;

		if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				&& graphicsFamily == maxOf<uint32_t>()) {
			graphicsFamily = i;
		}

		if ((queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
				&& transferFamily == maxOf<uint32_t>()) {
			transferFamily = i;
		}

		if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) && computeFamily == maxOf<uint32_t>()) {
			computeFamily = i;
		}

		if (presentSupport != 0 && presentFamily == maxOf<uint32_t>()) {
			presentFamily = i;
		}

		i++;
	}

	// try to select different families for transfer and compute (for more concurrency)
	if (computeFamily == graphicsFamily) {
		for (auto &it : queueInfo) {
			if (it.index != graphicsFamily
					&& ((it.flags & core::QueueFlags::Compute) != core::QueueFlags::None)) {
				computeFamily = it.index;
			}
		}
	}

	if (transferFamily == computeFamily || transferFamily == graphicsFamily) {
		for (auto &it : queueInfo) {
			if (it.index != graphicsFamily && it.index != computeFamily
					&& ((it.flags & core::QueueFlags::Transfer) != core::QueueFlags::None)) {
				transferFamily = it.index;
				break;
			}
		}
		if (transferFamily == computeFamily || transferFamily == graphicsFamily) {
			if (queueInfo[computeFamily].count >= queueInfo[graphicsFamily].count) {
				transferFamily = computeFamily;
			} else {
				transferFamily = graphicsFamily;
			}
		}
	}

	// try to map present with graphics
	if (presentFamily != graphicsFamily) {
		if ((queueInfo[graphicsFamily].flags & core::QueueFlags::Present)
				!= core::QueueFlags::None) {
			presentFamily = graphicsFamily;
		}
	}

	// fallback when Transfer or Compute is not defined
	if (transferFamily == maxOf<uint32_t>()) {
		transferFamily = graphicsFamily;
		queueInfo[transferFamily].flags |= core::QueueFlags::Transfer;
	}

	if (computeFamily == maxOf<uint32_t>()) {
		computeFamily = graphicsFamily;
	}

	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	Vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
			availableExtensions.data());

	// we need only API version for now
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	// find required device extensions
	bool notFound = false;
	for (auto &extensionName : s_requiredDeviceExtensions) {
		if (!extensionName) {
			break;
		}

		if (isPromotedExtension(deviceProperties.apiVersion, extensionName)) {
			continue;
		}

		bool found = false;
		for (auto &extension : availableExtensions) {
			if (strcmp(extensionName, extension.extensionName) == 0) {
				found = true;
				break;
			}
		}

		if (!found) {
			if constexpr (s_printVkInfo) {
				log::verbose("Vk-Info", "Required device extension not found: %s", extensionName);
			}
			notFound = true;
			break;
		}
	}

	if (notFound) {
		ret.requiredExtensionsExists = false;
	} else {
		ret.requiredExtensionsExists = true;
	}

	// check for optionals
	DeviceInfo::OptVec extensionFlags;
	Vector<StringView> enabledOptionals;
	Vector<StringView> promotedOptionals;
	for (auto &extensionName : s_optionalDeviceExtensions) {
		if (!extensionName) {
			break;
		}

		checkIfExtensionAvailable(deviceProperties.apiVersion, extensionName, availableExtensions,
				enabledOptionals, promotedOptionals, extensionFlags);
	}

	ret.device = device;
	ret.graphicsFamily = queueInfo[graphicsFamily];
	ret.presentFamily = (presentFamily == maxOf<uint32_t>()) ? DeviceInfo::QueueFamilyInfo()
															 : queueInfo[presentFamily];
	ret.transferFamily = queueInfo[transferFamily];
	ret.computeFamily = queueInfo[computeFamily];
	ret.optionalExtensions = sp::move(enabledOptionals);
	ret.promotedExtensions = sp::move(promotedOptionals);

	ret.availableExtensions.reserve(availableExtensions.size());
	for (auto &it : availableExtensions) { ret.availableExtensions.emplace_back(it.extensionName); }

	getDeviceProperties(device, ret.properties, extensionFlags, deviceProperties.apiVersion);
	getDeviceFeatures(device, ret.features, extensionFlags, deviceProperties.apiVersion);

	auto requiredFeatures = DeviceInfo::Features::getRequired();
	ret.requiredFeaturesExists =
			ret.features.canEnable(requiredFeatures, deviceProperties.apiVersion);

	return ret;
}

} // namespace stappler::xenolith::vk
