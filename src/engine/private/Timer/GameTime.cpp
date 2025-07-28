#include <algorithm>

#include <engine/public/OldConsole/ConVarManager.h>
#include <engine/public/Timer/GameTime.h>

GameTime::GameTime() :
	mStartTime(Clock::now()),
	mLastFrameTime(Clock::now()),
	mDeltaTime(0),
	mTotalTime(0),
	mFrameCount(0) {
	// コンソール変数の登録
	ConVarManager::RegisterConVar<float>(
		"host_timescale",
		1.0f,
		"Prescale the clock by this amount."
	);
}

void GameTime::StartFrame() {
	// フレーム開始時刻を記録
	mFrameStartTime = Clock::now();
}

void GameTime::EndFrame() {
	// フレーム終了時刻を記録し、次回のdeltaTime_を更新
	TimePoint frameEndTime = Clock::now();
	mDeltaTime = std::chrono::duration<double>(frameEndTime - mFrameStartTime).
		count();
	mLastFrameTime = frameEndTime;

	++mFrameCount;
}

template <typename T>
T GameTime::DeltaTime() {
	const double clamped = std::min(mDeltaTime, 1.0 / 60.0);
	return static_cast<T>(clamped);
}

template <typename T>
T GameTime::ScaledDeltaTime() {
	return static_cast<T>(mDeltaTime * TimeScale());
}

template double GameTime::DeltaTime<double>();
template float  GameTime::DeltaTime<float>();
template double GameTime::ScaledDeltaTime<double>();
template float  GameTime::ScaledDeltaTime<float>();

double GameTime::TotalTime() const {
	return static_cast<float>(mTotalTime);
}

float GameTime::TimeScale() {
	return ConVarManager::GetConVar(
		"host_timescale"
	)->GetValueAsFloat();
}

uint64_t GameTime::FrameCount() const {
	return mFrameCount;
}

void GameTime::SetTimeScale(const float& scale) {
	ConVarManager::GetConVar(
		"host_timescale"
	)->SetValueFromString(
		std::to_string(scale)
	);
}
