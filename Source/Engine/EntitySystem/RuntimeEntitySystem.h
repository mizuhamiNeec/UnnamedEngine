#pragma once

#include "Base/EntitySystem.h"

class RuntimeEntitySystem : public EntitySystem {
public:
	~RuntimeEntitySystem() override;
	void Init() override;
	void Update(float deltaTime) override;
	void Render(ID3D12GraphicsCommandList* commandList) override;
};

