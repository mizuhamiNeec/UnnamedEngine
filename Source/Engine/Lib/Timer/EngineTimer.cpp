#include "EngineTimer.h"

#include <ImGuiManager/Icons.h>
#include <SubSystem/Console/ConVarManager.h>
#include <Lib/Utils/StrUtils.h>
#include <time.h>

double EngineTimer::deltaTime_ = 0;
double EngineTimer::totalTime_ = 0;
uint64_t EngineTimer::frameCount_ = 0;

EngineTimer::EngineTimer() : startTime_(Clock::now()),
lastFrameTime_(Clock::now()) {
}

void EngineTimer::StartFrame() {
	const TimePoint currentTime = Clock::now();
	const std::chrono::duration<double> elapsed = currentTime - lastFrameTime_;
	deltaTime_ = elapsed.count();
	totalTime_ += deltaTime_;
	lastFrameTime_ = currentTime;

//#ifdef _DEBUG
//	ImGui::Begin((StrUtils::ConvertToUtf8(kIconTimer) + " EngineTimer").c_str());
//	ImGui::Text("%.2f FPS", 1.0 / deltaTime_);
//	ImGui::Text("%.2f ms", deltaTime_ * 1000.0);
//
//	const int totalMilliseconds = GetMillisecond(); // 0.01秒単位
//	const int days = GetDay();
//	const int hours = GetHour();
//	const int minutes = GetMinute();
//	const int secs = GetSecond();
//	const int centiseconds = totalMilliseconds % 100;
//
//	ImGui::Text(
//		"Uptime: %02d:%02d:%02d:%02d.%02d",
//		days,
//		hours,
//		minutes,
//		secs,
//		centiseconds
//	);
//	ImGui::End();
//#endif
}

void EngineTimer::EndFrame() const {
	const auto fpsMaxConVar = ConVarManager::GetConVar("cl_fpsmax");
	const double fpsLimit = fpsMaxConVar->GetValueAsDouble();
	if (fpsLimit > 0) {
		const double frameLimitDuration = 1.0 / fpsLimit;

		// 現在時刻を取得
		const TimePoint currentTime = Clock::now();
		const std::chrono::duration<double> elapsed = currentTime - lastFrameTime_;

		// 次フレームまでの残り時間を計算
		double remainingTime = frameLimitDuration - elapsed.count();
		if (remainingTime > 0.0) {
			// 残り時間の大部分をスリープ
			const double sleepTime = remainingTime * 0.9; // 残り時間の90%をスリープ
			if (sleepTime > 0.0) {
				std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
			}

			// スリープ後の補正（忙待ちで微調整）
			TimePoint afterSleepTime = Clock::now();
			while (std::chrono::duration<double>(afterSleepTime - lastFrameTime_).count() < frameLimitDuration) {
				afterSleepTime = Clock::now();
			}
		}
	}

	++frameCount_;
}

float EngineTimer::ScaledDelta() {
	const auto timeScaleConVar = ConVarManager::GetConVar("host_timescale");
	const double timeScale = timeScaleConVar->GetValueAsDouble();
	return static_cast<float>(deltaTime_ * timeScale);
}

void EngineTimer::SetTimeScale(const float& scale) {
	ConVarManager::GetConVar("host_timescale")->SetValueFromString(std::to_string(scale));
}

float EngineTimer::GetDeltaTime() {
	return static_cast<float>(deltaTime_);
}

float EngineTimer::GetScaledDeltaTime() {
	return static_cast<float>(deltaTime_) * GetTimeScale();
}

double EngineTimer::GetDeltaTimeDouble() {
	return deltaTime_;
}

double EngineTimer::GetScaledDeltaTimeDouble() {
	return deltaTime_ * GetTimeScale();
}

float EngineTimer::GetTotalTime() {
	return static_cast<float>(totalTime_);
}

float EngineTimer::GetTimeScale() {
	return ConVarManager::GetConVar("host_timescale")->GetValueAsFloat();
}

uint64_t EngineTimer::GetFrameCount() {
	return frameCount_;
}

int EngineTimer::GetDay() {
	return GetMillisecond() / (100 * 60 * 60 * 24);
}

int EngineTimer::GetHour() {
	return GetMillisecond() / (100 * 60 * 60) % 24;
}

int EngineTimer::GetMinute() {
	return GetMillisecond() / (100 * 60) % 60;
}

int EngineTimer::GetSecond() {
	return GetMillisecond() / 100 % 60;
}

int EngineTimer::GetMillisecond() {
	return static_cast<int>(totalTime_ * 100.0);
}

/// <summary>
/// エンジンが起動してからの経過時間を取得します
/// </summary>
DateTime EngineTimer::GetUpDateTime() {
	return {
		.day = GetDay(),
		.hour = GetHour(),
		.minute = GetMinute(),
		.second = GetSecond(),
		.millisecond = GetMillisecond()
	};
}

DateTime EngineTimer::GetNow() {
	const auto now = Clock::now();
	const auto nowTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	struct tm nowTm;
	localtime_s(&nowTm, &nowTime);
	return {
		.year = nowTm.tm_year + 1900,
		.month = nowTm.tm_mon + 1,
		.day = nowTm.tm_mday,
		.hour = nowTm.tm_hour,
		.minute = nowTm.tm_min,
		.second = nowTm.tm_sec,
		.millisecond = GetMillisecond()
	};
}
