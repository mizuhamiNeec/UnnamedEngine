#pragma once

#include <chrono>

#include <engine/public/structs/DateTime.h>

class EngineTimer {
public:
	EngineTimer();
	
	void StartFrame();
	void EndFrame();

	// Getter
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
