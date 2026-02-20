#pragma once

#include "IPlayerMovementState.h"

#include "game/components/player/MovementComponent.h"

class SlideMove : public IPlayerMovementState {
public:
	void Enter(MovementComponent* context, MovementData& data) override;
	void Update(MovementComponent* context, MovementData& data, float dt) override;
	void Exit(MovementComponent* context, MovementData& data) override;

	[[nodiscard]] MOVEMENT_STATE GetStateID() const override {
		return MOVEMENT_STATE::SLIDE;
	}

private:
	void Friction(MovementData& data, float amount, float dt);
	void Accelerate(MovementData& data, Vec3 dir, float speed, float accel, float dt);
	void ApplyHalfGravity(MovementData& data, float dt);

	static constexpr float kSlideAcceleration = 4.0f; // HU/s^2 - スライド加速度
	static constexpr float kSlideFriction     = 0.75f; // HU/s^2 - スライディング摩擦
};
