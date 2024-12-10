#pragma once

#include "../Base/BaseSystem.h"
#include "../EntityComponentSystem/Components/Transform/TransformComponent.h"

class TransformSystem : public BaseSystem {
public:
	void RegisterComponent(TransformComponent* component);
	void UnregisterComponent(TransformComponent* component);

	// インターフェース
	void Initialize() override;
	void Update(float deltaTime) override;
	void Terminate() override;

private:
	std::vector<TransformComponent*> components_;
};

