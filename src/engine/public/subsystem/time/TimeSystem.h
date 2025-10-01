#pragma once
#include <memory>

#include <engine/public/subsystem/interface/ISubsystem.h>
#include <engine/public/subsystem/time/FrameLimiter.h>
#include <engine/public/subsystem/time/GameTime.h>
#include <engine/public/subsystem/time/SystemClock.h>

namespace Unnamed {
	class TimeSystem : public ISubsystem {
	public:
		~TimeSystem() override;

		bool Init() override;

		void BeginFrame() const;
		void EndFrame() const;

		[[nodiscard]] const std::string_view GetName() const override;

		[[nodiscard]] GameTime*     GetGameTime() const;
		[[nodiscard]] FrameLimiter* GetFrameLimiter() const;

	private:
		std::unique_ptr<GameTime>     mGameTime;
		std::unique_ptr<FrameLimiter> mFrameLimiter;
		std::unique_ptr<SystemClock>  mSystemClock;
	};
}
