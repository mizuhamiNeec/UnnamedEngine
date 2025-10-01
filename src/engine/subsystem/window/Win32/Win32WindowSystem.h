#pragma once
#include <unordered_map>

#include <engine/subsystem/window/interface/IWindowSystem.h>

#include <engine/subsystem/window/interface/IWindow.h>

namespace Unnamed {
	struct IPlatformEvents;
}

class Win32WindowSystem : public IWindowSystem {
public:
	~Win32WindowSystem() override;

	// IWindowSystem<-ISubsystem
	bool Init() override;
	void Update(float) override;
	void Shutdown() override;

	static void RegisterPlatformEvent(Unnamed::IPlatformEvents* events);

	[[nodiscard]] const std::string_view GetName() const override {
		return "Win32WindowSystem";
	}

	// Win32WindowSystem
	IWindow* CreateNewWindow(const IWindow::WindowInfo& windowInfo) override;
	[[nodiscard]] const std::vector<std::unique_ptr<IWindow>>&
	GetWindows() const override;
	[[nodiscard]] bool AllClosed() const override;

	[[nodiscard]] static bool IsInactiveWindow();

	static LRESULT CALLBACK WndProc(
		HWND   hWnd, UINT     msg,
		WPARAM wParam, LPARAM lParam
	);

private:
	void RegisterClassOnce();

private:
	HINSTANCE   mHInstance  = {};
	std::string mClassName  = "UnnamedWindowClass";
	bool        mRegistered = false;

	static std::vector<std::unique_ptr<IWindow>>        mWindows;
	static std::unordered_map<HWND, class Win32Window*> mHWndMap;

	static Unnamed::IPlatformEvents* mPlatformEvents;
};
