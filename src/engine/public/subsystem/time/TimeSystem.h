#pragma once
#include <memory>

#include <engine/public/subsystem/interface/ISubsystem.h>
#include <engine/public/subsystem/time/FrameLimiter.h>
#include <engine/public/subsystem/time/GameTime.h>
#include <engine/public/subsystem/time/SystemClock.h>

class TimeSystem : public ISubsystem {
public:
	~TimeSystem() override;

	bool Init() override;
	void Update(float deltaTime) override;
	void Render() override;

	void EndFrame();
	void Shutdown() override;

	[[nodiscard]] const std::string_view GetName() const override;

	GameTime*     GetGameTime() const;
	FrameLimiter* GetFrameLimiter() const;

private:
	std::unique_ptr<GameTime>     mGameTime;
	std::unique_ptr<FrameLimiter> mFrameLimiter;
	std::unique_ptr<SystemClock>  mSystemClock;
};
