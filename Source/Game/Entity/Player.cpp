#include "Player.h"

#include "../EntityComponentSystem/Components/Camera/CameraComponent.h"

Player::Player() {
	transform_ = AddComponent<TransformComponent>();
	transform_->Initialize();
	gamemovement_ = AddComponent<Gamemovement>();
	gamemovement_->Initialize();
}

void Player::Initialize() const {
	gamemovement_->Initialize();
}

void Player::Update(const float& deltaTime) const {
	gamemovement_->Update(deltaTime);
}
