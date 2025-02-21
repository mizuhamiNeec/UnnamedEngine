#pragma once

#include "Renderer/AbstractionLayer/Base/BaseDevice.h"

class D3D12Device : public BaseDevice {
public:
	D3D12Device();
	~D3D12Device() override;
	bool CreateBuffer() override;
	bool CreateTexture() override;
};

