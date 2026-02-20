#include "SlideMove.h"
#include <engine/OldConsole/ConVarManager.h>
#include "../MovementComponent.h"
#include "PlayerMovementStateMachine.h"

void SlideMove::Enter(MovementComponent*  , MovementData&  ) {
}

void SlideMove::Update(MovementComponent*  , MovementData& data, float dt) {
	const float wishspeed = data.wishDirection.IsZero() ? 0.0f : data.currentSpeed * data.moveInputIntensity;

	if (!data.isGrounded) ApplyHalfGravity(data, dt);
	
	Friction(data, kSlideFriction, dt);

	if (!data.wishDirection.IsZero()) {
		Vec3 wishDir = data.wishDirection;
		wishDir.y    = 0;
		if (!wishDir.IsZero()) {
			wishDir.Normalize();
			Accelerate(data, wishDir, wishspeed, kSlideAcceleration, dt);
		}
	}

	if (!data.isGrounded) ApplyHalfGravity(data, dt);
}

void SlideMove::Exit(MovementComponent*  , MovementData&  ) {
}

void SlideMove::Friction(MovementData& data, float amount, float dt) {
	if (!data.isGrounded) return;

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

void SlideMove::Accelerate(MovementData& data, Vec3 dir, float speed, float accel, float dt) {
	if (dir.IsZero() || speed <= 0.0f || accel <= 0.0f) return;
	const float cur = Math::MtoH(data.velocity).Dot(dir);
	const float add = speed - cur;
	if (add <= 0.f) return;
	float acc      = std::min(accel * speed * dt, add);
	data.velocity += Math::HtoM(acc) * dir;
}

void SlideMove::ApplyHalfGravity(MovementData& data, float dt) {
	const float g = ConVarManager::GetConVar("sv_gravity")->GetValueAsFloat();
	data.velocity.y -= Math::HtoM(g) * 0.5f * dt;
}
