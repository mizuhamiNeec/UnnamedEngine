#pragma once

#include <chrono>

/// @brief ゲームの時間を管理するクラス
class GameTime {
public:
	GameTime();

	void StartGame();

	/// @brief フレームを終了し、必要に応じてゲーム時間を進めます。
	/// @param advanceGameTime false の場合は実時間だけを破棄し、次フレームの計測基準を更新します。
	void EndFrame(bool advanceGameTime = true);

	// Getter
	template <typename T = double>
	[[nodiscard]] T DeltaTime();

	template <typename T = double>
	[[nodiscard]] T ScaledDeltaTime();

	[[nodiscard]] double   TotalTime() const;
	[[nodiscard]] float    TimeScale() const;
	[[nodiscard]] uint64_t FrameCount() const;

private:
	using Clock     = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;

	TimePoint mStartTime;
	TimePoint mLastFrameTime;
	TimePoint mFrameStartTime;

	double mDeltaTime;
	double mScaledDeltaTime;
	double mTotalTime; // エンジンの起動から経過した時間

	uint64_t mFrameCount; // フレーム数

	float mTimeScale = 1.0f; // ゲームの時間スケール
};
