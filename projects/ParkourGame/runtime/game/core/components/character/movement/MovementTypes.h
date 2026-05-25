#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "../base/BaseCharacterComponent.h"

namespace Unnamed {
	class GameMovementComponent;
	class BaseKinematicCollisionResolver;

	/// @brief Locomotion Mode の識別子です。
	enum class MOVEMENT_MODE_ID : uint8_t {
		GROUND = 0,
		AIR,
		NOCLIP,
		COUNT
	};

	/// @brief 移動Abilityのスロットです。
	enum class MOVEMENT_ABILITY_SLOT : uint8_t {
		JUMP = 0,
		CROUCH,
		SLIDE,
		WALL_RUN,
		DOUBLE_JUMP,
		SPEED_VAULT,
		BLINK,
		GRAPPLE,
		COUNT
	};

	/// @brief 状態遷移の優先度です。
	enum class MOVEMENT_TRANSITION_PRIORITY : uint8_t {
		OPTIONAL_ABILITY = 10,
		STANCE           = 20,
		MODE_SAFETY      = 30,
		FORCED_ABILITY   = 40,
		DEBUG_NOCLIP     = 50
	};

	/// @brief Ability有効/無効の設定です。
	struct MovementCapabilitySet {
		bool jump       = true;
		bool crouch     = true;
		bool slide      = false;
		bool wallRun    = false;
		bool doubleJump = false;
		bool speedVault = false;
		bool blink      = false;
		bool grapple    = false;
	};

	/// @brief 遷移要求1件を表します。
	struct MovementTransitionRequest {
		MOVEMENT_MODE_ID             toMode   = MOVEMENT_MODE_ID::AIR;
		MOVEMENT_TRANSITION_PRIORITY priority =
			MOVEMENT_TRANSITION_PRIORITY::OPTIONAL_ABILITY;
		std::string_view reason = {};
		std::string_view source = {};
	};

	/// @brief Modeの実行状態です。
	struct MovementModeState {
		MOVEMENT_MODE_ID currentMode     = MOVEMENT_MODE_ID::AIR;
		MOVEMENT_MODE_ID previousMode    = MOVEMENT_MODE_ID::AIR;
		bool             changedThisTick = false;
	};

	/// @brief Mode/Ability実行で共有するコンテキストです。
	struct MovementContext {
		static constexpr size_t kTransitionBufferSize = 16;

		MovementFrameInput              input = {};
		TransformComponent*             transform = nullptr;
		Vec3                            velocity = Vec3::zero;
		BaseKinematicCollisionResolver* resolver = nullptr;
		Vec3                            halfExtents = Vec3::zero;
		bool                            isGrounded = false;
		uint64_t                        supportEntityGuid = 0;
		Vec3                            supportLinearVelocity = Vec3::zero;
		Vec3                            supportStepDelta = Vec3::zero;
		float                           jumpSnapDisableRemaining = 0.0f;
		MOVEMENT_MODE_ID                defaultAirMode = MOVEMENT_MODE_ID::AIR;
		GameMovementComponent*          movementComponent = nullptr;
		const MovementCapabilitySet*    capabilitySet = nullptr;
		MovementModeState               modeState = {};
		uint64_t                        activeAbilityMask = 0;
		bool                            modeTickSuppressed = false;

		std::array<MovementTransitionRequest, kTransitionBufferSize>
		transitionBuffer;
		size_t transitionCount = 0;

		/// @brief 遷移要求バッファをリセットします。
		void ResetTransitions();

		/// @brief 遷移要求をバッファへ追加します。
		bool SubmitTransition(
			MOVEMENT_MODE_ID             toMode,
			MOVEMENT_TRANSITION_PRIORITY priority,
			std::string_view             reason,
			std::string_view             source
		);

		/// @brief 指定Abilityが有効設定かを返します。
		[[nodiscard]] bool IsCapabilityEnabled(
			MOVEMENT_ABILITY_SLOT slot
		) const;

		/// @brief 指定Abilityが現在アクティブかを返します。
		[[nodiscard]] bool IsAbilityActive(MOVEMENT_ABILITY_SLOT slot) const;
	};

	[[nodiscard]] uint64_t AbilitySlotToMask(MOVEMENT_ABILITY_SLOT slot);
	[[nodiscard]] std::string_view ToString(MOVEMENT_MODE_ID modeId);
	[[nodiscard]] std::string_view ToString(MOVEMENT_ABILITY_SLOT slot);
}
