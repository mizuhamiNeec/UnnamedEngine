#pragma once

#include "../Components/gamemovement.h"
#include "../../Engine/EntityComponentSystem/Entity/Base/BaseEntity.h"

class TransformComponent;

class Player : public BaseEntity {
public:
	Player();

	void Initialize() const;
	void Update(const float& deltaTime) const;

private:
	TransformComponent* transform_ = nullptr;
	Gamemovement* gamemovement_ = nullptr;
};
