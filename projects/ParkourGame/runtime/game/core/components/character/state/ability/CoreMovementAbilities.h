#pragma once

#include "../interface/IMovementAbility.h"

namespace Unnamed {
	/// @brief 地上ジャンプを処理する基本Abilityです。
	class JumpMovementAbility final : public IMovementAbility {
	public:
		void Init(ConsoleSystem* console) override;
		[[nodiscard]] MOVEMENT_ABILITY_SLOT GetSlot() const override;
		[[nodiscard]] std::string_view GetDebugName() const override;
		[[nodiscard]] bool IsForcedAbility() const override;
		[[nodiscard]] bool CanRunInMode(MOVEMENT_MODE_ID modeId) const override;
		[[nodiscard]] bool CanStart(const MovementContext& context) const override;
		bool Start(MovementContext& context, float deltaTime) override;
		bool Tick(MovementContext& context, float deltaTime) override;
		void Stop(MovementContext& context) override;
	};

	/// @brief しゃがみ入力を管理する基本Abilityです。
	class CrouchMovementAbility final : public IMovementAbility {
	public:
		void Init(ConsoleSystem* console) override;
		[[nodiscard]] MOVEMENT_ABILITY_SLOT GetSlot() const override;
		[[nodiscard]] std::string_view GetDebugName() const override;
		[[nodiscard]] bool IsForcedAbility() const override;
		[[nodiscard]] bool CanRunInMode(MOVEMENT_MODE_ID modeId) const override;
		[[nodiscard]] bool CanStart(const MovementContext& context) const override;
		bool Start(MovementContext& context, float deltaTime) override;
		bool Tick(MovementContext& context, float deltaTime) override;
		void Stop(MovementContext& context) override;
	};
}
