#pragma once

#include <chrono>

class GameTime {
public:
	GameTime();

	void StartFrame();
	void EndFrame();

	// Getter
	template <typename T = double>
	[[nodiscard]] T DeltaTime();

	template <typename T = double>
	[[nodiscard]] T ScaledDeltaTime();


	[[nodiscard]] double   TotalTime() const;
	[[nodiscard]] float    TimeScale();
	[[nodiscard]] uint64_t FrameCount() const;

	// Setter
	void SetTimeScale(const float& scale);

private:
	using Clock     = std::chrono::high_resolution_clock;
	using TimePoint = Clock::time_point;

	TimePoint mStartTime;
	TimePoint mLastFrameTime;
	TimePoint mFrameStartTime;

	double mDeltaTime; // 前回のフレームから経過した時間
	double mTotalTime; // エンジンの起動から経過した時間

	uint64_t mFrameCount; // フレーム数

	float mTimeScale = 1.0f; // ゲームの時間スケール
};
