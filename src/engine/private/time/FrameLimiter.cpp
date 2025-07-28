#include <engine/public/OldConsole/ConVarManager.h>
#include <engine/public/time/FrameLimiter.h>
#include <engine/public/time/GameTime.h>
#include <engine/public/utils/Properties.h>

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

void FrameLimiter::Limit() {
	CheckConVarValue();
	
	// 制限が設定されていない場合は何もしない
	if (mTargetFrameDuration == Clock::duration::zero()) {
		return;
	}

	using namespace std::chrono;

	const double frameSecond  = mGameTime->DeltaTime<double>();
	const double targetSecond = duration<double>(mTargetFrameDuration).count();
	const double waitSecond   = targetSecond - frameSecond;

	// フレーム時間が目標時間を超えている場合は待機しない
	if (waitSecond <= 0.0) {
		return;
	}

	const auto waitDuration = duration<double>(waitSecond);

	// 大まかにスリープする
	constexpr auto spinThreshold = milliseconds(2);
	if (waitDuration > spinThreshold) {
		std::this_thread::sleep_for(waitDuration - spinThreshold);
	}

	const auto endTime = Clock::now() + waitDuration;
	while (Clock::now() < endTime);
}

void FrameLimiter::CheckConVarValue() {
	double targetFPS = ConVarManager::GetConVar("fps_max")->GetValueAsDouble();
	SetTargetFPS(targetFPS);
}
