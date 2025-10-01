#include <engine/OldConsole/ConVarManager.h>
#include <engine/subsystem/time/FrameLimiter.h>
#include <engine/subsystem/time/GameTime.h>
#include <runtime/core/Properties.h>

FrameLimiter::FrameLimiter(GameTime* gameTime) :
	mGameTime(gameTime) {
	SetTargetFPS(kDefaultFpsMax);
}

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

void FrameLimiter::BeginFrame() {
	mFrameStart = Clock::now();
}

void FrameLimiter::Limit() {
	CheckConVarValue();

	// 制限が設定されていない場合は何もしない
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

void FrameLimiter::CheckConVarValue() {
	double targetFPS = 1000; // TODO: コンソール変数に置き換え
	SetTargetFPS(targetFPS);
}
