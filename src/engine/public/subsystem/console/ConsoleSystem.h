#pragma once
#include <engine/public/subsystem/console/interface/IConsole.h>
#include <engine/public/subsystem/interface/ISubsystem.h>

class ConsoleSystem final : public ISubsystem, public IConsole {
public:
	~ConsoleSystem() override;

	bool Init() override;
	void Update(float deltaTime) override;
	void Shutdown() override;

	[[nodiscard]] const std::string_view GetName() const override;

	void Msg(std::string_view message) override;

private:
};
