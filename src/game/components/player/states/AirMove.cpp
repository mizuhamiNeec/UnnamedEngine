#include "AirMove.h"
#include <engine/OldConsole/ConVarManager.h>
#include "../MovementComponent.h"
#include "PlayerMovementStateMachine.h"

void AirMove::Enter(MovementComponent*  , MovementData&  ) {
}

void AirMove::Update(MovementComponent*  , MovementData& data, float dt) {
	const float wishspeed = data.wishDirection.IsZero() ? 0.0f : data.currentSpeed * data.moveInputIntensity;

	ApplyGravity(data, dt);

	Vec3 wishdir = data.wishDirection;
	wishdir.y    = 0.0f;

	AirAccelerate(data, wishdir, wishspeed, ConVarManager::GetConVar("sv_airaccelerate")->GetValueAsFloat(), dt);

	ApplyGravity(data, dt);
}

void AirMove::Exit(MovementComponent*  , MovementData&  ) {
}

void AirMove::ApplyGravity(MovementData& data, float dt) {
	const float g = ConVarManager::GetConVar("sv_gravity")->GetValueAsFloat();
	data.velocity.y -= Math::HtoM(g) * 0.5f * dt;
}

void AirMove::AirAccelerate(MovementData& data, Vec3 dir, float speed, float accel, float dt) {
	if (dir.IsZero() || speed <= 0.0f || accel <= 0.0f) return;
	const float wishspd = std::min(speed, kAirSpeedCap);
	const float cur     = Math::MtoH(data.velocity).Dot(dir);
	const float add     = wishspd - cur;
	if (add <= 0.f) return;
	const float acc = std::min(accel * speed * dt, add);
	data.velocity  += Math::HtoM(acc) * dir;
}
