#pragma once

#include "../Engine/EntityComponentSystem/Entity/Base/BaseEntity.h"
#include "../Engine/EntityComponentSystem/Components/Transform/TransformComponent.h"

class Player : public BaseEntity {
public:
	Player() {
		transform_ = AddComponent<TransformComponent>();
	}

private:
	TransformComponent* transform_ = nullptr;
};
