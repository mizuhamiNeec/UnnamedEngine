#pragma once

#include <array>
#include <memory>
#include <string_view>
#include <vector>

#include "interface/IMovementAbility.h"
#include "interface/IMovementMode.h"

namespace Unnamed {
	class ConsoleSystem;

	/// @brief Locomotion ModeとAbilityを統合管理する状態機械です。
	class GameMovementStateMachine {
	public:
		GameMovementStateMachine();
		~GameMovementStateMachine();

		/// @brief 状態機械で利用するサービス参照を初期化します。
		/// @param console Mode/Ability に渡す ConsoleSystem 参照
		void Init(ConsoleSystem* console);

		/// @brief 現在状態を1フレーム更新します。
		void Tick(MovementContext& context, float deltaTime);

		/// @brief Modeを登録します。
		void AddMode(const std::shared_ptr<IMovementMode>& mode);

		/// @brief Abilityを登録します。
		void AddAbility(const std::shared_ptr<IMovementAbility>& ability);

		/// @brief 初期Modeを設定します。
		void SetInitialMode(MOVEMENT_MODE_ID modeId);

		[[nodiscard]] MOVEMENT_MODE_ID GetCurrentModeId() const;
		[[nodiscard]] std::string_view GetCurrentModeName() const;
		[[nodiscard]] uint64_t GetActiveAbilityMask() const;
		[[nodiscard]] bool IsAbilityActive(MOVEMENT_ABILITY_SLOT slot) const;

	private:
		struct AbilityEntry {
			std::shared_ptr<IMovementAbility> ability = nullptr;
			bool active = false;
		};

		void ApplyModeTransition(
			MovementContext& context,
			MOVEMENT_MODE_ID toMode,
			std::string_view reason,
			std::string_view source
		);

		void StopAbilitiesInvalidInMode(
			MovementContext& context,
			MOVEMENT_MODE_ID modeId
		);

		[[nodiscard]] uint64_t RebuildAbilityMask() const;
		[[nodiscard]] IMovementMode* GetMode(MOVEMENT_MODE_ID modeId) const;

		ConsoleSystem* mConsole = nullptr;
		bool mHasReportedMissingConsole = false;

		std::array<std::shared_ptr<IMovementMode>, static_cast<size_t>(
			MOVEMENT_MODE_ID::COUNT
		)> mModes = {};
		std::vector<AbilityEntry> mAbilities = {};
		MOVEMENT_MODE_ID mCurrentModeId = MOVEMENT_MODE_ID::AIR;
		uint64_t mActiveAbilityMask = 0;
	};
}
