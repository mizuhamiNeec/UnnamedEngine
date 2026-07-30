#pragma once
#include <memory>

#include "ITweenPlayable.h"

namespace Unnamed {
	/// @brief TweenHandleは、再生中Tweenへのweak_ptrを保持し、対象の破棄後は無効な非所有ハンドルとして動作します
	class TweenHandle {
	public:
		TweenHandle() = default;

		/// @brief コンストラクタ
		/// @param playable TweenPlayableオブジェクトへのWeakPtr。
		explicit TweenHandle(const std::weak_ptr<ITweenPlayable>& playable);

		/// @brief TweenPlayableオブジェクトが有効かどうかを判定します。
		/// @return 有効な場合はtrue、無効な場合はfalse。
		[[nodiscard]] bool IsValid() const;

		/// @brief TweenPlayableオブジェクトが再生中かどうかを判定します。
		/// @return 再生中の場合はtrue、そうでない場合はfalse
		[[nodiscard]] bool IsPlaying() const;

		/// @brief TweenPlayableオブジェクトが完了しているかどうかを判定します。
		/// @return 完了している場合はtrue、そうでない場合はfalse
		[[nodiscard]] bool IsComplete() const;

		/// @brief TweenPlayableオブジェクトを一時停止します。
		void Pause() const;

		/// @brief TweenPlayableオブジェクトを再開します。
		void Resume() const;

		/// @brief TweenPlayableオブジェクトを終了します。
		/// @param complete trueの場合、Tweenを完了させてから終了します。falseの場合、即座に終了します。
		void Kill(bool complete = false) const;

	private:
		std::weak_ptr<ITweenPlayable> mPlayable;
	};
}
