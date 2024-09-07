/**
Copyright (c) 2020 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_BACKEND_VK_XLVKINSTANCE_H_
#define XENOLITH_BACKEND_VK_XLVKINSTANCE_H_

#include "XLCoreInstance.h"
#include "XLVkInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class Device;

struct SP_PUBLIC LoopData : Ref {
	using DeviceSupportCallback = Function<bool(const DeviceInfo &)>;
	using DeviceExtensionsCallback = Function<Vector<StringView>(const DeviceInfo &)>;
	using DeviceFeaturesCallback = Function<DeviceInfo::Features(const DeviceInfo &)>;

	DeviceSupportCallback deviceSupportCallback;
	DeviceExtensionsCallback deviceExtensionsCallback;
	DeviceFeaturesCallback deviceFeaturesCallback;
};

class SP_PUBLIC Instance : public core::Instance, public InstanceTable {
public:
	using PresentSupportCallback = Function<uint32_t(const Instance *, VkPhysicalDevice device, uint32_t familyIdx)>;

	Instance(VkInstance, const PFN_vkGetInstanceProcAddr getInstanceProcAddr, uint32_t targetVersion,
			Vector<StringView> &&optionals, Dso &&vulkanModule, TerminateCallback &&terminate, PresentSupportCallback &&, bool validationEnabled, Rc<Ref> &&);
	virtual ~Instance();

	virtual Rc<core::Loop> makeLoop(core::LoopInfo &&) const override;

	Rc<Device> makeDevice(const core::LoopInfo &) const;

	core::SurfaceInfo getSurfaceOptions(VkSurfaceKHR, VkPhysicalDevice) const;
	VkExtent2D getSurfaceExtent(VkSurfaceKHR, VkPhysicalDevice) const;

	VkInstance getInstance() const;

	void printDevicesInfo(std::ostream &stream) const;

	uint32_t getVersion() const { return _version; }

private:
	void getDeviceFeatures(const VkPhysicalDevice &device, DeviceInfo::Features &, ExtensionFlags, uint32_t) const;
	void getDeviceProperties(const VkPhysicalDevice &device, DeviceInfo::Properties &, ExtensionFlags, uint32_t) const;

	DeviceInfo getDeviceInfo(VkPhysicalDevice device) const;

	friend class VirtualDevice;
	friend class DrawDevice;
	friend class PresentationDevice;
	friend class TransferDevice;
	friend class Allocator;
	friend class ViewImpl;

	VkInstance _instance;
	uint32_t _version = 0;
	Vector<StringView> _optionals;
	Vector<DeviceInfo> _devices;
	PresentSupportCallback _checkPresentSupport;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
};

}

#endif // XENOLITH_BACKEND_VK_XLVKINSTANCE_H_
