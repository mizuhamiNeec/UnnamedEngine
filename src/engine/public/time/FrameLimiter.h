#pragma once
#include <chrono>

class GameTime;

//-----------------------------------------------------------------------------
// Purpose: フレームレートの制限をするためのクラス...
//			と思ったけど不正確...
// TODO: 修正しよう
//
//-----------------------------------------------------------------------------
class FrameLimiter {
public:
	explicit FrameLimiter(GameTime* gameTime);

	void SetTargetFPS(double targetFPS);

	void Limit();

private:
	void CheckConVarValue();

	using Clock     = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;

	GameTime* mGameTime = nullptr;

	Clock::duration mTargetFrameDuration;
};
