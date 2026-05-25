#include "AirMovementMode.h"

#include <algorithm>

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"

namespace Unnamed {
	namespace {
		constexpr float kGroundNormalMinY = 0.7f; // 地面と見なすための法線の最小Y成分

		// これ以上の上昇速度があるときは、地面に接していてもジャンプスナップを無効化する。
		constexpr float kNonJumpVelHu = 250.0f; // デフォは140
	}

	void AirMovementMode::Enter(ConsoleSystem* console) {
		if (!console) {
			Error("AirMovementMode", "ConsoleSystem is not available.");
			return;
		}
		mConsole = console;
	}

	void AirMovementMode::Tick(
		MovementContext& context, const float deltaTime
	) {
		if (mConsole->GetConVarValueOr("noclip", false)) {
			context.SubmitTransition(
				MOVEMENT_MODE_ID::NOCLIP,
				MOVEMENT_TRANSITION_PRIORITY::DEBUG_NOCLIP,
				"noclip enabled",
				"AirMovementMode"
			);
			return;
		}

		context.isGrounded               = false;
		context.supportEntityGuid        = 0;
		context.supportLinearVelocity    = Vec3::zero;
		context.supportStepDelta         = Vec3::zero;
		context.jumpSnapDisableRemaining = std::max(
			0.0f,
			context.jumpSnapDisableRemaining - deltaTime
		);

		Vec3 wishDir = GetWishDirHoriz(context);
		if (!wishDir.IsZero()) {
			wishDir.Normalize();
		}

		ApplyHalfGravity(context.velocity, deltaTime);
		AirAccelerate(
			context.velocity,
			wishDir,
			320.0f,
			mConsole->GetConVarValueOr("sv_airaccelerate", 10.0f),
			deltaTime
		);
		ApplyHalfGravity(context.velocity, deltaTime);

		KinematicMoveQuery query = {
			.position    = context.transform->GetPosition(),
			.velocity    = context.velocity,
			.halfExtents = context.halfExtents.IsZero() ?
				               Math::HtoM(Vec3(32.0f, 73.0f, 32.0f)) * 0.5f :
				               context.halfExtents,
			.deltaTime = deltaTime
		};

		context.resolver->SlideMove(query.position, query.velocity, deltaTime);
		context.transform->SetPosition(query.position);
		context.velocity = query.velocity;

		Physics::Hit groundHit{};
		if (
			context.jumpSnapDisableRemaining <= 0.0f && // ジャンプスナップ無効時間が経過している
			context.velocity.y < Math::HtoM(kNonJumpVelHu) && // しきい値以下の上昇速度
			IsGrounded(context.resolver, query.position, &groundHit) // 地面に接している
		) {
			context.velocity.y        = 0.0f;
			context.isGrounded        = true;
			context.supportEntityGuid = groundHit.hitEntityGuid;
			context.SubmitTransition(
				MOVEMENT_MODE_ID::GROUND,
				MOVEMENT_TRANSITION_PRIORITY::MODE_SAFETY,
				"landed",
				"AirMovementMode"
			);
		}
	}

	void AirMovementMode::Exit() {}

	MOVEMENT_MODE_ID AirMovementMode::GetModeId() const {
		return MOVEMENT_MODE_ID::AIR;
	}

	std::string_view AirMovementMode::GetModeName() const {
		return "AirMove";
	}

	void AirMovementMode::AirAccelerate(
		Vec3&       currentVel,
		const Vec3  wishDir,
		const float wishSpeed,
		const float accel,
		const float deltaTime
	) const {
		if (wishDir.IsZero() || wishSpeed <= 0.0f || accel <= 0.0f) {
			return;
		}

		const float wishSpd = std::min(
			wishSpeed,
			mConsole->GetConVarValueOr("sv_airspeedcap", 30.0f)
		);
		const float cur = Math::MtoH(currentVel).Dot(wishDir);
		const float add = wishSpd - cur;
		if (add <= 0.0f) {
			return;
		}

		const float acc = std::min(accel * wishSpeed * deltaTime, add);
		currentVel      += Math::HtoM(acc) * wishDir;
	}

	void AirMovementMode::ApplyHalfGravity(
		Vec3&       target,
		const float deltaTime
	) const {
		const float gravity = mConsole->GetConVarValueOr("sv_gravity", 800.0f);
		target.y            -= Math::HtoM(gravity) * 0.5f * deltaTime;
	}

	bool AirMovementMode::IsGrounded(
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
