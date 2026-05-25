#include "CoreMovementAbilities.h"

#include <algorithm>

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "game/core/components/character/GameMovementComponent.h"

namespace Unnamed {
	namespace {
		constexpr float kJumpDetachBiasHu = 1.0f;
	}

	void JumpMovementAbility::Init(ConsoleSystem* console) {
		mConsole = console;
	}

	MOVEMENT_ABILITY_SLOT JumpMovementAbility::GetSlot() const {
		return MOVEMENT_ABILITY_SLOT::JUMP;
	}

	std::string_view JumpMovementAbility::GetDebugName() const {
		return "jump";
	}

	bool JumpMovementAbility::IsForcedAbility() const {
		return false;
	}

	bool JumpMovementAbility::CanRunInMode(const MOVEMENT_MODE_ID modeId) const {
		return modeId == MOVEMENT_MODE_ID::GROUND;
	}

	bool JumpMovementAbility::CanStart(const MovementContext& context) const {
		return context.modeState.currentMode == MOVEMENT_MODE_ID::GROUND &&
			context.input.jumpPressed &&
			context.transform &&
			context.resolver;
	}

	bool JumpMovementAbility::Start(
		MovementContext& context,
		const float deltaTime
	) {
		const float detachBiasM = Math::HtoM(kJumpDetachBiasHu);
		context.transform->SetPosition(
			context.transform->GetPosition() + Vec3::up * detachBiasM
		);

		context.velocity.y += Math::HtoM(
			mConsole ? mConsole->GetConVarValueOr("sv_jumpvelocity", 420.0f) : 420.0f
		);

		context.jumpSnapDisableRemaining = std::max(
			context.jumpSnapDisableRemaining,
			mConsole ? mConsole->GetConVarValueOr("sv_jumpsnapdisabletime", 0.1f) : 0.1f
		);

		Vec3 jumpPos = context.transform->GetPosition();
		Vec3 jumpVel = context.velocity;
		context.resolver->SlideMove(jumpPos, jumpVel, deltaTime);
		context.transform->SetPosition(jumpPos);
		context.velocity = jumpVel;

		context.isGrounded = false;
		context.supportEntityGuid = 0;
		context.supportLinearVelocity = Vec3::zero;
		context.supportStepDelta = Vec3::zero;
		// 同tickのMode更新でジャンプ結果が上書きされないよう抑止します。
		context.modeTickSuppressed = true;
		context.SubmitTransition(
			MOVEMENT_MODE_ID::AIR,
			MOVEMENT_TRANSITION_PRIORITY::STANCE,
			"jump",
			"JumpMovementAbility"
		);
		return false;
	}

	bool JumpMovementAbility::Tick(MovementContext&, float) {
		return false;
	}

	void JumpMovementAbility::Stop(MovementContext&) {}

	void CrouchMovementAbility::Init(ConsoleSystem* console) {
		mConsole = console;
	}

	MOVEMENT_ABILITY_SLOT CrouchMovementAbility::GetSlot() const {
		return MOVEMENT_ABILITY_SLOT::CROUCH;
	}

	std::string_view CrouchMovementAbility::GetDebugName() const {
		return "crouch";
	}

	bool CrouchMovementAbility::IsForcedAbility() const {
		return false;
	}

	bool CrouchMovementAbility::CanRunInMode(MOVEMENT_MODE_ID) const {
		return true;
	}

	bool CrouchMovementAbility::CanStart(const MovementContext& context) const {
		return context.input.crouchPressed;
	}

	bool CrouchMovementAbility::Start(MovementContext&, float) {
		return true;
	}

	bool CrouchMovementAbility::Tick(MovementContext& context, float) {
		return context.input.crouchPressed;
	}

	void CrouchMovementAbility::Stop(MovementContext&) {}
}
