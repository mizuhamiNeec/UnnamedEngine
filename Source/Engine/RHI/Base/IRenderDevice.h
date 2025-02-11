#pragma once

#include <memory>

enum class APIType {
	DirectX12,
	Vulkan,
};

class IRenderDevice {
public:
	virtual ~IRenderDevice() = default;
	virtual bool Init() = 0;
	virtual bool Shutdown() = 0;
};
