#include <engine/public/subsystem/time/SystemClock.h>

void SystemClock::Init() {
	if (mStartTime.time_since_epoch().count() == 0) {
		mStartTime = SysClock::now();
	}
}

SystemClock::TimePoint SystemClock::StartTime() {
	return mStartTime;
}

SystemClock::TimePoint SystemClock::Now() {
	return SysClock::now();
}

double SystemClock::UpTime() {
	if (mStartTime.time_since_epoch().count() == 0) {
		return 0.0;
	}
	using namespace std::chrono;
	return duration<double>(SysClock::now() - mStartTime).count();
}

std::string SystemClock::ToString(const TimePoint& tp) {
	using namespace std::chrono;

	std::time_t tt = SysClock::to_time_t(tp);
	std::tm     tm = {};

	localtime_s(&tm, &tt);

	auto millis =
		duration_cast<milliseconds>(tp.time_since_epoch()).count() % 1000;

	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
		<< '.' << std::setfill('0') << std::setw(3) << millis;
	return oss.str();
}

static std::tm ToTm(SystemClock::TimePoint tp) {
	std::time_t tt = SystemClock::SysClock::to_time_t(tp);
	std::tm     tm = {};
	localtime_s(&tm, &tt);

	return tm;
}

int SystemClock::Year(const TimePoint& tp) {
	return ToTm(tp).tm_year + 1900;
}

int SystemClock::Month(const TimePoint& tp) {
	return ToTm(tp).tm_mon + 1;
}

int SystemClock::Day(const TimePoint& tp) {
	return ToTm(tp).tm_mday;
}

int SystemClock::Hour(const TimePoint& tp) {
	return ToTm(tp).tm_hour;
}

int SystemClock::Minute(const TimePoint& tp) {
	return ToTm(tp).tm_min;
}

int SystemClock::Second(const TimePoint& tp) {
	return ToTm(tp).tm_sec;
}

int SystemClock::MilliSecond(const TimePoint& tp) {
	using namespace std::chrono;
	return static_cast<int>(
		duration_cast<milliseconds>(tp.time_since_epoch()
		).count() % 1000);
}

DateTime SystemClock::GetDateTime(const TimePoint& tp) {
	return {
		.year = Year(tp),
		.month = Month(tp),
		.day = Day(tp),
		.hour = Hour(tp),
		.minute = Minute(tp),
		.second = Second(tp),
		.millisecond = MilliSecond(tp)
	};
}

std::chrono::time_point<std::chrono::system_clock> SystemClock::mStartTime;
