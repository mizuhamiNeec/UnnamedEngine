#include <engine/subsystem/time/SystemClock.h>

/// @brief 初期化
void SystemClock::Init() {
	if (mStartTime.time_since_epoch().count() == 0) {
		mStartTime = SysClock::now();
	}
}

/// @brief システム起動時刻を取得します
/// @return システム起動時刻
SystemClock::TimePoint SystemClock::StartTime() {
	return mStartTime;
}

/// @brief 現在時刻を取得します
/// @return 現在時刻
SystemClock::TimePoint SystemClock::Now() {
	return SysClock::now();
}

/// @brief システム起動からの経過時間を秒単位で取得します
/// @return システム起動からの経過時間（秒）
double SystemClock::UpTime() {
	if (mStartTime.time_since_epoch().count() == 0) {
		return 0.0;
	}
	using namespace std::chrono;
	return duration<double>(SysClock::now() - mStartTime).count();
}

/// @brief 指定したTimePointを文字列に変換します
/// @param tp 変換するTimePoint
/// @return 変換された文字列
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

/// @brief TimePointをstd::tm構造体に変換します
/// @param tp 変換するTimePoint
/// @return 変換されたstd::tm構造体
static std::tm ToTm(SystemClock::TimePoint tp) {
	std::time_t tt = SystemClock::SysClock::to_time_t(tp);
	std::tm     tm = {};
	localtime_s(&tm, &tt);

	return tm;
}

/// @brief 指定したTimePointから年を取得します
/// @param tp 取得するTimePoint
/// @return 年
int SystemClock::Year(const TimePoint& tp) {
	return ToTm(tp).tm_year + 1900;
}

/// @brief 指定したTimePointから月を取得します
/// @param tp 取得するTimePoint
/// @return 月
int SystemClock::Month(const TimePoint& tp) {
	return ToTm(tp).tm_mon + 1;
}

/// @brief 指定したTimePointから日を取得します
/// @param tp 取得するTimePoint
/// @return 日
int SystemClock::Day(const TimePoint& tp) {
	return ToTm(tp).tm_mday;
}

/// @brief 指定したTimePointから時を取得します
/// @param tp 取得するTimePoint
/// @return 時
int SystemClock::Hour(const TimePoint& tp) {
	return ToTm(tp).tm_hour;
}

/// @brief 指定したTimePointから分を取得します
/// @param tp 取得するTimePoint
/// @return 分
int SystemClock::Minute(const TimePoint& tp) {
	return ToTm(tp).tm_min;
}

/// @brief 指定したTimePointから秒を取得します
/// @param tp 取得するTimePoint
/// @return 秒
int SystemClock::Second(const TimePoint& tp) {
	return ToTm(tp).tm_sec;
}

/// @brief 指定したTimePointからミリ秒を取得します
/// @param tp 取得するTimePoint
/// @return ミリ秒
int SystemClock::MilliSecond(const TimePoint& tp) {
	using namespace std::chrono;
	return static_cast<int>(
		duration_cast<milliseconds>(tp.time_since_epoch()
		).count() % 1000);
}

/// @brief 指定したTimePointからDateTime構造体を取得します
/// @param tp 取得するTimePoint
/// @return DateTime構造体
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
