#include <engine/OldConsole/ConVarManager.h>
#include <engine/subsystem/time/FrameLimiter.h>
#include <engine/subsystem/time/GameTime.h>
#include <runtime/core/Properties.h>

/// @brief コンストラクタ
/// @param gameTime ゲームタイムクラスへのポインタ
FrameLimiter::FrameLimiter(GameTime* gameTime) :
	mGameTime(gameTime) {
	SetTargetFPS(kDefaultFpsMax);
}

/// @brief 目標FPSを設定します
/// @param targetFPS 目標FPS
void FrameLimiter::SetTargetFPS(const double targetFPS) {
	using namespace std::chrono;
	if (targetFPS > 0) {
		// 1/FPS 秒を duration に変換
		mTargetFrameDuration = duration_cast<Clock::duration>(
			duration<double>(1.0 / targetFPS));
	} else {
		mTargetFrameDuration = Clock::duration::zero();
	}
}

/// @brief フレームの開始を記録します
void FrameLimiter::BeginFrame() {
	mFrameStart = Clock::now();
}

/// @brief フレームレートを制限します
void FrameLimiter::Limit() {
	CheckConVarValue();

	if (mTargetFrameDuration == Clock::duration::zero()) {
		return;
	}

	using namespace std::chrono;

	auto now = Clock::now();

	auto elapsed = now - mFrameStart;

	if (elapsed >= mTargetFrameDuration) {
		return;
	}

	auto remaining = mTargetFrameDuration - elapsed;

	// 大まかにスリープする
	constexpr auto spinThreshold = milliseconds(10);
	if (remaining > spinThreshold) {
		std::this_thread::sleep_for(remaining - spinThreshold);
	}

	const auto endTime = mFrameStart + mTargetFrameDuration;
	while (Clock::now() < endTime) {
		std::this_thread::yield();
	}
}

/// @brief コンソール変数の値をチェックして目標FPSを更新します
void FrameLimiter::CheckConVarValue() {
	double targetFPS = 10000; // TODO: コンソール変数に置き換え
	SetTargetFPS(targetFPS);
}
