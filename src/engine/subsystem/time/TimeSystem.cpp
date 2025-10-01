#include <engine/subsystem/time/TimeSystem.h>
#include <engine/subsystem/interface/ServiceLocator.h>

namespace Unnamed {
	TimeSystem::~TimeSystem() = default;

	bool TimeSystem::Init() {
		ServiceLocator::Register<TimeSystem>(this);

		mGameTime     = std::make_unique<GameTime>();
		mFrameLimiter = std::make_unique<FrameLimiter>(mGameTime.get());
		mSystemClock  = std::make_unique<SystemClock>();

		return mGameTime && mFrameLimiter;
	}

	void TimeSystem::BeginFrame() const {
		mFrameLimiter->BeginFrame();
	}

	void TimeSystem::EndFrame() const {
		mFrameLimiter->Limit();
		mGameTime->EndFrame();
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
}
