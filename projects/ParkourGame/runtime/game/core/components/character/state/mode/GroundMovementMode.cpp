#include "GroundMovementMode.h"

#include <algorithm>

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/world/World.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "game/core/components/character/GameMovementComponent.h"

namespace Unnamed {
	namespace {
		constexpr float kGroundNormalMinY = 0.7f;
	}

	void GroundMovementMode::Enter(ConsoleSystem* console) {
		if (!console) {
			Error("GroundMovementMode", "ConsoleSystem is not available.");
			return;
		}
		mConsole = console;
	}

	void GroundMovementMode::Tick(
		MovementContext& context,
		const float      deltaTime
	) {
		if (mConsole->GetConVarValueOr("noclip", false)) {
			context.SubmitTransition(
				MOVEMENT_MODE_ID::NOCLIP,
				MOVEMENT_TRANSITION_PRIORITY::DEBUG_NOCLIP,
				"noclip enabled",
				"GroundMovementMode"
			);
			return;
		}

		context.isGrounded = false;

		Vec3 wishDir = GetWishDirHoriz(context);
		if (!wishDir.IsZero()) {
			wishDir.Normalize();
		}

		context.transform->GetWorld()->GetDebugDraw().DrawRay(
			context.transform->GetPosition(),
			wishDir,
			Vec4::white
		);

		context.velocity.y = 0.0f;
		if (!context.input.jumpPressed) {
			ApplyFriction(
				context.velocity,
				mConsole->GetConVarValueOr("sv_friction", 5.2f),
				deltaTime
			);
		}

		float currentMaxSpeed = mConsole->GetConVarValueOr(
			"sv_walkspeed", 150.0f
		);
		if (context.movementComponent &&
		    context.movementComponent->UseSprintSpeedAsDefaultGroundSpeed()) {
			currentMaxSpeed = mConsole->GetConVarValueOr(
				"sv_sprintspeed", 320.0f
			);
		} else if (context.input.sprintPressed) {
			currentMaxSpeed = mConsole->GetConVarValueOr(
				"sv_sprintspeed", 320.0f
			);
		}
		if (context.IsAbilityActive(MOVEMENT_ABILITY_SLOT::CROUCH)) {
			currentMaxSpeed = mConsole->GetConVarValueOr("sv_duckspeed", 63.3f);
		}

		Accelerate(
			context.velocity,
			wishDir,
			currentMaxSpeed,
			mConsole->GetConVarValueOr("sv_accelerate", 10.0f),
			deltaTime
		);

		KinematicMoveQuery query = {
			.position    = context.transform->GetPosition(),
			.velocity    = context.velocity,
			.halfExtents = context.halfExtents,
			.deltaTime   = deltaTime
		};
		context.resolver->StepMove(
			query.position,
			query.velocity,
			mConsole->GetConVarValueOr("sv_stepheight", 18.0f),
			deltaTime
		);

		context.transform->SetPosition(query.position);
		context.velocity = query.velocity;

		Physics::Hit groundHit{};
		if (!IsGrounded(context.resolver, query.position, &groundHit)) {
			context.supportEntityGuid      = 0;
			context.supportLinearVelocity  = Vec3::zero;
			context.supportStepDelta       = Vec3::zero;
			context.SubmitTransition(
				MOVEMENT_MODE_ID::AIR,
				MOVEMENT_TRANSITION_PRIORITY::MODE_SAFETY,
				"ground lost",
				"GroundMovementMode"
			);
			return;
		}

		context.isGrounded        = true;
		context.supportEntityGuid = groundHit.hitEntityGuid;
		context.velocity.y        = 0.0f;
	}

	void GroundMovementMode::Exit() {}

	MOVEMENT_MODE_ID GroundMovementMode::GetModeId() const {
		return MOVEMENT_MODE_ID::GROUND;
	}

	std::string_view GroundMovementMode::GetModeName() const {
		return "GroundMove";
	}

	void GroundMovementMode::ApplyFriction(
		Vec3&       velocity,
		const float amount,
		const float deltaTime
	) const {
		Vec3 velHorz       = velocity;
		velHorz.y          = 0.0f;
		const float speedM = velHorz.Length();
		const float speed  = Math::MtoH(speedM);
		if (speed < 0.1f) {
			return;
		}

		const float stop = mConsole->GetConVarValueOr("sv_stopspeed", speed);
		const float ctrl = speed < stop ? stop : speed;
		const float drop = ctrl * amount * deltaTime;
		float       newSpeed = std::max(0.0f, speed - drop);
		if (newSpeed != speed) {
			newSpeed /= speed;
			velocity *= newSpeed;
		}
	}

	void GroundMovementMode::Accelerate(
		Vec3&       currentVel,
		Vec3        wishDir,
		const float wishSpeed,
		const float accel,
		const float deltaTime
	) const {
		if (wishDir.IsZero() || wishSpeed <= 0.0f || accel <= 0.0f) {
			return;
		}

		const float maxGroundSpeed = mConsole->GetConVarValueOr(
			"sv_maxspeed", 320.0f
		);
		const float wishSpd = std::min(wishSpeed, maxGroundSpeed);

		Vec3 currentHorizontal = Math::MtoH(currentVel);
		currentHorizontal.y    = 0.0f;
		const float cur        = currentHorizontal.Dot(wishDir);
		const float add        = wishSpd - cur;
		if (add <= 0.0f) {
			return;
		}

		const float acc = std::min(accel * wishSpd * deltaTime, add);
		currentVel      += Math::HtoM(acc) * wishDir;
	}

	bool GroundMovementMode::IsGrounded(
		const BaseKinematicCollisionResolver* resolver,
		const Vec3&                           position,
		Physics::Hit*                         outHit
	) const {
		if (!resolver) {
			return false;
		}

		const float probeDistanceHu = mConsole ?
			                              mConsole->GetConVarValueOr(
				                              "sv_groundprobe_distance_hu", 1.0f
			                              ) :
			                              1.0f;

		Physics::Hit hit{};
		if (!resolver->ProbeGround(
			position,
			Math::HtoM(std::max(0.0f, probeDistanceHu)),
			&hit
		)) {
			return false;
		}

		if (outHit) {
			*outHit = hit;
		}

		return hit.startSolid || hit.allsolid || hit.normal.y >
		       kGroundNormalMinY;
	}
}
