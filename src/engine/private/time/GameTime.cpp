#include <algorithm>

#include <engine/public/OldConsole/ConVarManager.h>
#include <engine/public/time/GameTime.h>

GameTime::GameTime() :
	mStartTime(Clock::now()),
	mLastFrameTime(Clock::now()),
	mDeltaTime(1.0 / 60.0),
	mScaledDeltaTime(1.0 / 60.0),
	mTotalTime(0),
	mFrameCount(0) {
	// コンソール変数の登録
	ConVarManager::RegisterConVar<float>(
		"host_timescale",
		1.0f,
		"Prescale the clock by this amount."
	);
}

void GameTime::EndFrame() {
	// フレーム終了時刻を記録
	TimePoint frameEndTime = Clock::now();

	// デルタ計算
	mDeltaTime = std::chrono::duration<double>(
		frameEndTime - mFrameStartTime
	).count();

	// タイムスケールを取得
	mTimeScale = ConVarManager::GetConVar(
		"host_timescale"
	)->GetValueAsFloat();

	// 各値を更新
	mTotalTime += mDeltaTime;
	mScaledDeltaTime = mDeltaTime * mTimeScale;
	++mFrameCount;

	// 次の開始時刻として記録
	mFrameStartTime = frameEndTime;
}

template <typename T>
T GameTime::DeltaTime() {
	const double clamped = std::min(mDeltaTime, 1.0 / 60.0);
	return static_cast<T>(clamped);
}

template <typename T>
T GameTime::ScaledDeltaTime() {
	return static_cast<T>(mScaledDeltaTime * TimeScale());
}

template double GameTime::DeltaTime<double>();
template float  GameTime::DeltaTime<float>();
template double GameTime::ScaledDeltaTime<double>();
template float  GameTime::ScaledDeltaTime<float>();

/// @brief ゲームの起動から経過した時間を取得します。
/// @return ゲームの起動から経過した時間（秒単位）
double GameTime::TotalTime() const {
	return static_cast<float>(mTotalTime);
}

/// @brief ゲームの時間スケールを取得します。
/// @return ゲームの時間スケール
float GameTime::TimeScale() {
	return mTimeScale;
}

/// @brief 現在のフレーム数を取得します。
/// @return 現在のフレーム数
uint64_t GameTime::FrameCount() const {
	return mFrameCount;
}
