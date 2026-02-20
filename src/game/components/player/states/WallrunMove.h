#pragma once

#include "IPlayerMovementState.h"

#include "game/components/player/MovementComponent.h"

class WallrunMove : public IPlayerMovementState {
public:
	void Enter(MovementComponent* context, MovementData& data) override;
	void Update(MovementComponent* context, MovementData& data, float dt) override;
	void Exit(MovementComponent* context, MovementData& data) override;

	[[nodiscard]] MOVEMENT_STATE GetStateID() const override {
		return MOVEMENT_STATE::WALL_RUN;
	}

private:
	static constexpr float kWallrunGravityScale = 0.1f; // 重力軽減
	static constexpr float kWallrunJumpForce    = 350.0f; // HU/s
};
