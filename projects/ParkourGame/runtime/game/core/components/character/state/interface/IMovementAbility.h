#pragma once

#include "../../movement/MovementTypes.h"

namespace Unnamed {
	class ConsoleSystem;

	/// @brief 移動Abilityインターフェースです。
	class IMovementAbility {
	public:
		virtual ~IMovementAbility() = default;

		/// @brief 初期化時に呼ばれます。
		virtual void Init(ConsoleSystem* console) = 0;

		/// @brief Abilityの識別スロットを返します。
		[[nodiscard]] virtual MOVEMENT_ABILITY_SLOT GetSlot() const = 0;

		/// @brief デバッグ表示名を返します。
		[[nodiscard]] virtual std::string_view GetDebugName() const = 0;

		/// @brief 本Abilityが強制優先度を利用するかを返します。
		[[nodiscard]] virtual bool IsForcedAbility() const = 0;

		/// @brief 現在Modeで実行可能かを返します。
		[[nodiscard]] virtual bool CanRunInMode(MOVEMENT_MODE_ID modeId) const = 0;

		/// @brief 開始可能かを判定します。
		[[nodiscard]] virtual bool CanStart(
			const MovementContext& context
		) const = 0;

		/// @brief 開始処理を実行します。
		/// @return 持続Abilityとしてアクティブ化する場合はtrue
		virtual bool Start(MovementContext& context, float deltaTime) = 0;

		/// @brief アクティブ中の更新処理です。
		/// @return 継続する場合はtrue、停止する場合はfalse
		virtual bool Tick(MovementContext& context, float deltaTime) = 0;

		/// @brief 停止処理です。
		virtual void Stop(MovementContext& context) = 0;

	protected:
		ConsoleSystem* mConsole = nullptr;
	};
}
