#pragma once
#include <chrono>

struct DateTime {
	int day;
	int hour;
	int minute;
	int second;
	int millisecond;
};

class EngineTimer {
public:
	EngineTimer();

	void StartFrame();

	void EndFrame() const;

	static float ScaledDelta();

	// Setter
	static void SetTimeScale(const float& scale);

	// Getter
	[[nodiscard]] static float GetDeltaTime();
	[[nodiscard]] static float GetScaledDeltaTime();
	[[nodiscard]] static float GetTotalTime();
	[[nodiscard]] static float GetTimeScale();

	[[nodiscard]] static int GetDay();
	[[nodiscard]] static int GetHour();
	[[nodiscard]] static int GetMinute();
	[[nodiscard]] static int GetSecond();
	[[nodiscard]] static int GetMillisecond();

	[[nodiscard]] static DateTime GetUpDateTime();

private:
	using Clock = std::chrono::high_resolution_clock;
	using TimePoint = Clock::time_point;

	TimePoint startTime_;
	TimePoint lastFrameTime_;
	static double deltaTime_; // 前回のフレームから経過した時間
	static double totalTime_; // エンジンの起動から経過した時間
	double frameDuration_; // フレーム持続時間
};
