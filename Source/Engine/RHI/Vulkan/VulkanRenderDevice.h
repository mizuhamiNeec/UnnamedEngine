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
	static bool CheckValidationLayerSupport();
	static std::vector<const char*> GetRequiredExtensions();

	std::unique_ptr<VkInstance> instance_;
};
