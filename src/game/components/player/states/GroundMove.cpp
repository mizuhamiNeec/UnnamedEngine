#include "GroundMove.h"
#include <engine/OldConsole/ConVarManager.h>
#include "../MovementComponent.h"
#include "PlayerMovementStateMachine.h"

void GroundMove::Enter(MovementComponent*  , MovementData&  ) {
	// 
}

void GroundMove::Update(MovementComponent*  , MovementData& data, float dt) {
	const float wishspeed = data.wishDirection.IsZero() ? 0.0f : data.currentSpeed * data.moveInputIntensity;

	data.velocity.y = 0.0f;

	const float groundFriction = ConVarManager::GetConVar("sv_friction")->GetValueAsFloat();
	Friction(data, groundFriction, dt);

	Vec3 wishdir = data.wishDirection;
	wishdir.y    = 0.0f;

	const float wishdirSqrLen = wishdir.SqrLength();
	if (wishdirSqrLen > 1e-8f && wishspeed > 0.0f) {
		wishdir *= 1.0f / std::sqrt(wishdirSqrLen);
		Accelerate(data, wishdir, wishspeed, ConVarManager::GetConVar("sv_accelerate")->GetValueAsFloat(), dt);
	}
}

void GroundMove::Exit(MovementComponent*  , MovementData&  ) {
	//
}

void GroundMove::Friction(MovementData& data, float amount, float dt) {
	Vec3 vel_horz      = data.velocity;
	vel_horz.y         = 0;
	const float speedM = vel_horz.Length();
	const float speed  = Math::MtoH(speedM);
	if (speed < 0.1f) return;

	const float stop = ConVarManager::GetConVar("sv_stopspeed")->GetValueAsFloat();
	const float ctrl = speed < stop ? stop : speed;

	const float drop = ctrl * amount * dt;

	float newspeed = std::max(0.0f, speed - drop);

	if (newspeed != speed) {
		newspeed       /= speed;
		data.velocity *= newspeed;
	}
}

void GroundMove::Accelerate(MovementData& data, Vec3 dir, float speed, float accel, float dt) {
	if (dir.IsZero() || speed <= 0.0f || accel <= 0.0f) return;
	const float cur = Math::MtoH(data.velocity).Dot(dir);
	const float add = speed - cur;
	if (add <= 0.f) return;
	float acc      = std::min(accel * speed * dt, add);
	data.velocity += Math::HtoM(acc) * dir;
}
