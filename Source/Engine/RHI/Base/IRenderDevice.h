#pragma once

enum class APIType {
	D3D12,
	Vulkan,
};

class IRenderDevice {
public:
	virtual ~IRenderDevice() = default;
	virtual bool Init() = 0;
	virtual bool Shutdown() = 0;
};
