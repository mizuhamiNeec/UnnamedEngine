#pragma once
#include <memory>

#include <Renderer/AbstractionLayer/Base/BaseRenderer.h>

#include "VulkanDevice.h"

class VulkanRenderer : public BaseRenderer {
public:
	~VulkanRenderer() override;
	bool Init(const RendererInitInfo& info) override;
	void Shutdown() override;
	BaseDevice* GetDevice() override;

private:
	std::unique_ptr<VulkanDevice> vulkanDevice_;
};

