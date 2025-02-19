#pragma once

#include <vector>

#include <RHI/Base/IRenderDevice.h>

#include <vulkan/vulkan.h>

//-----------------------------------------------------------------------------
// TODO: やる気と時間ができたら実装しよう!!!
//-----------------------------------------------------------------------------

class VulkanRenderDevice : public IRenderDevice {
public:
	bool Init() override;
	bool Shutdown() override;

private:
	bool InitializeSurface();
	static bool CheckValidationLayerSupport();
	static std::vector<const char*> GetRequiredExtensions();
	bool SetupDebugMessenger();
	bool PickPhysicalDevice();
	static bool FindQueueFamilies(VkPhysicalDevice device, uint32_t& outQueueFamilyIndex);
	bool CreateLogicalDevice();

	VkInstance instance_ = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
	VkDevice device_ = VK_NULL_HANDLE;
	VkQueue graphicsQueue_ = VK_NULL_HANDLE;
	uint32_t graphicsQueueFamilyIndex_ = 0;
	VkSurfaceKHR surface_ = VK_NULL_HANDLE;
};
