#pragma once

#include <Renderer/AbstractionLayer/Base/BaseDevice.h>

class VulkanDevice : public BaseDevice {
public:
	~VulkanDevice() override;
	bool CreateBuffer() override;
	bool CreateTexture() override;
};

