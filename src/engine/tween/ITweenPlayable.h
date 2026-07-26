#pragma once

namespace Unnamed {
	/// @brief ITweenPlayableは、Tween実装が満たす操作契約とライフタイム境界を定義します
	class ITweenPlayable {
	public:
		virtual ~ITweenPlayable() = default;

		virtual void Update(float deltaTime) = 0;
		virtual void Pause() = 0;
		virtual void Resume() = 0;
		virtual void Kill(bool complete) = 0;

		[[nodiscard]] virtual bool IsAlive() const = 0;
		[[nodiscard]] virtual bool IsPlaying() const = 0;
		[[nodiscard]] virtual bool IsComplete() const = 0;
	};
}
