#include <engine/public/subsystem/time/TimeSystem.h>

TimeSystem::~TimeSystem() {
}

bool TimeSystem::Init() {
	mGameTime     = std::make_unique<GameTime>();
	mFrameLimiter = std::make_unique<FrameLimiter>(mGameTime.get());
	mSystemClock  = std::make_unique<SystemClock>();

	return mGameTime && mFrameLimiter;
}

void TimeSystem::Update(float) {
}

void TimeSystem::Render() {
	ISubsystem::Render();
}

void TimeSystem::EndFrame() {
	mGameTime->EndFrame();
	mFrameLimiter->Limit();
}

void TimeSystem::Shutdown() {
}

const std::string_view TimeSystem::GetName() const {
	return "Time";
}

GameTime* TimeSystem::GetGameTime() const {
	return mGameTime.get();
}

FrameLimiter* TimeSystem::GetFrameLimiter() const {
	return mFrameLimiter.get();
}
