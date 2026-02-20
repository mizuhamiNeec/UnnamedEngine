#include "WallrunMove.h"
#include <engine/OldConsole/ConVarManager.h>
#include "../MovementComponent.h"
#include "PlayerMovementStateMachine.h"

void WallrunMove::Enter(MovementComponent*  , MovementData&  ) {
}

void WallrunMove::Update(MovementComponent*  , MovementData& data, float dt) {
	const float gravityValue = ConVarManager::GetConVar("sv_gravity")->GetValueAsFloat();
	data.velocity.y -= Math::HtoM(gravityValue) * kWallrunGravityScale * dt;

	const float wishspeed = data.wishDirection.IsZero() ? 0.0f : data.currentSpeed * data.moveInputIntensity;
	
	const Vec3 wishdir = data.wallRunDirection;
	
	if (data.vecMoveInput.y > 0) {
		const float currentSpeed = Math::MtoH(data.velocity.Dot(wishdir));
		const float addSpeed     = wishspeed * 1.2f - currentSpeed;

		if (addSpeed > 0) {
			const float accel = ConVarManager::GetConVar("sv_airaccelerate")->GetValueAsFloat() * 1.5f;
			const float accelspeed = std::min(accel * wishspeed * dt, addSpeed);
			data.velocity         += Math::HtoM(accelspeed) * wishdir;
		}
	} else {
		const float speedM = data.velocity.Length();
		const float speed  = Math::MtoH(speedM);
		if (speed > 0.1f) {
			const float fric = ConVarManager::GetConVar("sv_friction")->GetValueAsFloat();
			const float drop = speed * fric * dt * 0.5f; 
			const float news = std::max(0.0f, speed - drop);
			if (news != speed) { data.velocity *= (news / speed); }
		}
	}

	const float intoWall = data.velocity.Dot(-data.wallRunNormal);
	if (intoWall > 0) { data.velocity += data.wallRunNormal * intoWall; }

	const float pullForce = Math::HtoM(80.0f); 
	data.velocity        += -data.wallRunNormal * pullForce * dt;
}

void WallrunMove::Exit(MovementComponent*  , MovementData&  ) {
}
