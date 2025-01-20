#pragma once

#include <d3d12.h>

class EntitySystem {
public:
	virtual ~EntitySystem() = default;
	virtual void Init() = 0;
	virtual void Update(float deltaTime) = 0;
	virtual void Render(ID3D12GraphicsCommandList* commandList) = 0;
};

