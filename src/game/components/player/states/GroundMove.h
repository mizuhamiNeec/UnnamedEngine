#pragma once

#include "IPlayerMovementState.h"

#include "game/components/player/MovementComponent.h"

class GroundMove : public IPlayerMovementState {
public:
	void Enter(MovementComponent* context, MovementData& data) override;
	void Update(
		MovementComponent* context, MovementData& data, float dt
	) override;
	void Exit(MovementComponent* context, MovementData& data) override;

	[[nodiscard]] MOVEMENT_STATE GetStateID() const override {
		return MOVEMENT_STATE::GROUND;
	}

private:
	void Friction(MovementData& data, float amount, float dt);
	void Accelerate(
		MovementData& data, Vec3 dir, float speed, float accel, float dt
	);
};
