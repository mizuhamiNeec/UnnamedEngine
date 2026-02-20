#pragma once

#include "IPlayerMovementState.h"

#include "game/components/player/MovementComponent.h"

class AirMove : public IPlayerMovementState {
public:
	void Enter(MovementComponent* context, MovementData& data) override;
	void Update(MovementComponent* context, MovementData& data, float dt) override;
	void Exit(MovementComponent* context, MovementData& data) override;

	[[nodiscard]] MOVEMENT_STATE GetStateID() const override {
		return MOVEMENT_STATE::AIR;
	}

private:
	void          AirAccelerate(MovementData& data, Vec3 dir, float speed, float accel, float dt);
	void          ApplyGravity(MovementData& data, float dt);

	static constexpr float kAirSpeedCap = 30.0f;
};
