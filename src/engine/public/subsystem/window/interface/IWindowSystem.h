#pragma once

#include <engine/public/Components/Animation/AnimationComponent.h>
#include <engine/public/subsystem/interface/ISubsystem.h>
#include <engine/public/subsystem/window/interface/IWindow.h>

class IWindowSystem : public ISubsystem {
public:
	// ISubsystem
	bool                                 Init() override = 0;
	void                                 Shutdown() override = 0;
	[[nodiscard]] const std::string_view GetName() const override = 0;

	// IWindowSystem
	virtual IWindow* CreateNewWindow(const IWindow::WindowInfo& windowInfo) = 0;

	[[nodiscard]]
	virtual const std::vector<std::unique_ptr<IWindow>>& GetWindows() const = 0;
	[[nodiscard]]
	virtual bool AllClosed() const = 0;
};
