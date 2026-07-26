#pragma once

namespace Unnamed {
	/// @brief ITweenPlayableは、Tween実装が満たす操作契約とライフタイム境界を定義します
	class ITweenPlayable {
	public:
		virtual ~ITweenPlayable() = default;

		/// @brief Tweenを更新します。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		virtual void Update(float deltaTime) = 0;

		/// @brief Tweenの再生を一時停止します。
		virtual void Pause() = 0;

		/// @brief 一時停止中のTweenの再生を再開します。
		virtual void Resume() = 0;

		/// @brief Tweenを強制終了します。
		/// @param complete trueの場合、Tweenを完了状態にしてから終了します。falseの場合、Tweenを即座に終了します。
		virtual void Kill(bool complete) = 0;

		/// @brief Tweenが生きているか?
		[[nodiscard]] virtual bool IsAlive() const = 0;

		/// @brief Tweenが再生中か?
		[[nodiscard]] virtual bool IsPlaying() const = 0;

		/// @brief Tweenが完了しているか?
		[[nodiscard]] virtual bool IsComplete() const = 0;
	};
}
