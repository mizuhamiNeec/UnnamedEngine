#pragma once

#include <chrono>
#include <cstdint>

struct DateTime {
	int year;
	int month;
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
	void EndFrame();

	static float ScaledDelta();

	// Setter
	static void SetTimeScale(const float& scale);

	// Getter
	[[nodiscard]] static float    GetDeltaTime();
	[[nodiscard]] static float    GetScaledDeltaTime();
	[[nodiscard]] static double   GetDeltaTimeDouble();
	[[nodiscard]] static double   GetScaledDeltaTimeDouble();
	[[nodiscard]] static float    GetTotalTime();
	[[nodiscard]] static float    GetTimeScale();
	[[nodiscard]] static uint64_t GetFrameCount();
	[[nodiscard]] static DateTime GetUpDateTime();
	[[nodiscard]] static DateTime GetNow();

	[[nodiscard]] static int GetDay();
	[[nodiscard]] static int GetHour();
	[[nodiscard]] static int GetMinute();
	[[nodiscard]] static int GetSecond();
	[[nodiscard]] static int GetMillisecond();

private:
	using Clock     = std::chrono::high_resolution_clock;
	using TimePoint = Clock::time_point;

	TimePoint       mStartTime;
	TimePoint       mLastFrameTime;
	TimePoint       mFrameStartTime;
	static double   mDeltaTime;  // 前回のフレームから経過した時間
	static double   mTotalTime;  // エンジンの起動から経過した時間
	static uint64_t mFrameCount; // フレーム数
};
