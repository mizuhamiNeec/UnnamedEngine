#pragma once
#include <RHI/Base/IRenderDevice.h>

#include <vulkan/vulkan.h>

class VulkanRenderDevice : public IRenderDevice {
public:
	bool Init() override;
	bool Shutdown() override;

private:
};
