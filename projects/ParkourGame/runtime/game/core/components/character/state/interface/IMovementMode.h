#pragma once

#include "../../movement/MovementTypes.h"

namespace Unnamed {
	class ConsoleSystem;

	/// @brief Locomotion Modeインターフェースです。
	class IMovementMode {
	public:
		virtual ~IMovementMode() = default;

		/// @brief Mode開始時に呼ばれます。
		/// @param console ConsoleSystemへのポインタ。必要に応じて保存して使用してください。
		virtual void Enter(ConsoleSystem* console) = 0;

		/// @brief Mode更新処理です。
		/// @param context キャラクターの移動に関するコンテキスト情報。必要に応じて読み書きしてください。
		/// @param deltaTime 前回の更新からの経過時間（秒単位）
		virtual void Tick(MovementContext& context, float deltaTime) = 0;

		/// @brief Mode終了時に呼ばれます。
		virtual void Exit() = 0;

		/// @brief Mode識別子を返します。
		/// @return このModeの識別子
		[[nodiscard]] virtual MOVEMENT_MODE_ID GetModeId() const = 0;

		/// @brief 表示用Mode名を返します。
		/// @return このModeの表示用名前。デバッグ表示などで使用されます。
		[[nodiscard]] virtual std::string_view GetModeName() const = 0;

		/// @brief カメラ方向を考慮した水平移動方向を返します。
		/// @param context 移動コンテキスト
		/// @return カメラ方向を考慮した水平移動方向のベクトル。ゼロベクトルの場合は移動入力なしとみなされます
		virtual Vec3 GetWishDirHoriz(MovementContext& context) const;

		/// @brief カメラ方向を考慮した3次元移動方向を返します。
		/// @param context 移動コンテキスト
		/// @return カメラ方向を考慮した3次元移動
		virtual Vec3 GetWishDir(MovementContext& context) const;

	protected:
		ConsoleSystem* mConsole = nullptr;
	};
}
