#include <engine/unnamed/subsystem/time/GameTime.h>

#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

/// @brief コンストラクタ
GameTime::GameTime() :
	mStartTime(Clock::now()),
	mLastFrameTime(Clock::now()),
	mDeltaTime(1.0 / 60.0),
	mScaledDeltaTime(1.0 / 60.0),
	mTotalTime(0),
	mFrameCount(0) {
}

/// @brief ゲーム開始時の処理を行います。
void GameTime::StartGame() {
	mTotalTime      = 0.0;
	mFrameCount     = 0;
	mStartTime      = Clock::now();
	mFrameStartTime = mStartTime;
}

/// @brief フレーム開始時の処理を行います。
void GameTime::EndFrame(const bool advanceGameTime) {
	// フレーム終了時刻を記録
	const TimePoint frameEndTime = Clock::now();

	// デルタ計算
	mDeltaTime = std::chrono::duration<double>(
		frameEndTime - mFrameStartTime
	).count();

	if (!advanceGameTime) {
		// ロード用フレームはゲームの更新時間に含めず、次フレームの起点だけを更新する。
		mDeltaTime       = 0.0;
		mScaledDeltaTime = 0.0;
		mFrameStartTime  = frameEndTime;
		return;
	}

	// タイムスケールを取得
	mTimeScale = ServiceLocator::Get<Unnamed::ConsoleSystem>()->GetConVarAs<
		Unnamed::ConVar<float>>(
		"host_timescale"
	)->GetValue();

	// 各値を更新
	mScaledDeltaTime = mDeltaTime * mTimeScale;
	mTotalTime       += mScaledDeltaTime;
	++mFrameCount;

	// 次の開始時刻として記録
	mFrameStartTime = frameEndTime;
}

/// @brief 前フレームからの経過時間を取得します。
/// @tparam T 返却型（floatまたはdouble）
/// @return 前フレームからの経過時間（秒単位）
template <typename T>
T GameTime::DeltaTime() {
	return static_cast<T>(mDeltaTime);
}

/// @brief タイムスケールが適用された前フレームからの経過時間を取得します。
/// @tparam T 返却型（floatまたはdouble）
/// @return タイムスケールが適用された前フレームからの経過時間（秒単位）
template <typename T>
T GameTime::ScaledDeltaTime() {
	return static_cast<T>(mScaledDeltaTime);
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
float GameTime::TimeScale() const {
	return mTimeScale;
}

/// @brief 現在のフレーム数を取得します。
/// @return 現在のフレーム数
uint64_t GameTime::FrameCount() const {
	return mFrameCount;
}
