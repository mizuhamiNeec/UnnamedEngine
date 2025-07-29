#pragma once

#include <chrono>

#include "engine/public/structs/DateTime.h"

class SystemClock {
public:
	using SysClock  = std::chrono::system_clock;
	using TimePoint = SysClock::time_point;

	static void Init();

	[[nodiscard]] static TimePoint StartTime();
	[[nodiscard]] static TimePoint Now();

	[[nodiscard]] static double UpTime();

	[[nodiscard]] static std::string ToString(const TimePoint& tp);

	[[nodiscard]] static int Year(const TimePoint& tp);
	[[nodiscard]] static int Month(const TimePoint& tp);
	[[nodiscard]] static int Day(const TimePoint& tp);
	[[nodiscard]] static int Hour(const TimePoint& tp);
	[[nodiscard]] static int Minute(const TimePoint& tp);
	[[nodiscard]] static int Second(const TimePoint& tp);
	[[nodiscard]] static int MilliSecond(const TimePoint& tp);

	[[nodiscard]] static DateTime GetDateTime(const TimePoint& tp);

private:
	static TimePoint sStartTime;
};
