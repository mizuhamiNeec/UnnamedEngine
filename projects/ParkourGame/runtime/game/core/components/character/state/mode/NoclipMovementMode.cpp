#include "NoclipMovementMode.h"

#include <algorithm>

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

namespace Unnamed {
	void NoclipMovementMode::Enter(ConsoleSystem* console) {
		if (!console) {
			Error("NoclipMovementMode", "ConsoleSystem is not available.");
			return;
		}
		mConsole = console;
	}

	void NoclipMovementMode::Tick(MovementContext& context, const float deltaTime) {
#ifdef _DEBUG
		if (!mConsole->GetConVarValueOr("noclip", false)) {
			context.SubmitTransition(
				context.defaultAirMode,
				MOVEMENT_TRANSITION_PRIORITY::DEBUG_NOCLIP,
				"noclip disabled",
				"NoclipMovementMode"
			);
			return;
		}

		context.isGrounded = false;
		const Vec3 wishDir = GetWishDir(context);
		const float maxSpeed =
			mConsole->GetConVarValueOr("sv_maxspeed", 320.0f) *
			mConsole->GetConVarValueOr("sv_noclipspeed", 5.0f);
		const float wishSpeed = wishDir.Length() * maxSpeed;

		ApplyFriction(
			context.velocity,
			mConsole->GetConVarValueOr("sv_friction", 5.2f),
			deltaTime
		);

		Accelerate(
			context.velocity,
			wishDir,
			wishSpeed,
			mConsole->GetConVarValueOr("sv_noclipaccelerate", 5.0f),
			deltaTime
		);

		context.transform->SetPosition(
			context.transform->GetPosition() + context.velocity * deltaTime
		);
#else
		(void)context;
		(void)deltaTime;
#endif
	}

	void NoclipMovementMode::Exit() {}

	MOVEMENT_MODE_ID NoclipMovementMode::GetModeId() const {
		return MOVEMENT_MODE_ID::NOCLIP;
	}

	std::string_view NoclipMovementMode::GetModeName() const {
		return "NoclipMove";
	}

	void NoclipMovementMode::ApplyFriction(
		Vec3& velocity,
		const float amount,
		const float deltaTime
	) const {
		const float speedM = velocity.Length();
		const float speed = Math::MtoH(speedM);
		if (speed < 0.1f) {
			return;
		}

		const float stop = mConsole->GetConVarValueOr("sv_stopspeed", speed);
		const float ctrl = speed < stop ? stop : speed;
		const float drop = ctrl * amount * deltaTime;
		float newSpeed = std::max(0.0f, speed - drop);
		if (newSpeed != speed) {
			newSpeed /= speed;
			velocity *= newSpeed;
		}
	}

	void NoclipMovementMode::Accelerate(
		Vec3& currentVel,
		const Vec3 wishDir,
		const float wishSpeed,
		const float accel,
		const float deltaTime
	) const {
		if (wishDir.IsZero() || wishSpeed <= 0.0f || accel <= 0.0f) {
			return;
		}

		const float maxNoclipSpeed =
			mConsole->GetConVarValueOr("sv_maxspeed", 320.0f) *
			mConsole->GetConVarValueOr("sv_noclipspeed", 5.0f);
		const float wishSpd = std::min(wishSpeed, maxNoclipSpeed);

		Vec3 currentHorizontal = Math::MtoH(currentVel);
		currentHorizontal.y = 0.0f;
		const float cur = currentHorizontal.Dot(wishDir);
		const float add = wishSpd - cur;
		if (add <= 0.0f) {
			return;
		}

		const float acc = std::min(accel * wishSpd * deltaTime, add);
		currentVel += Math::HtoM(acc) * wishDir;
	}
}
