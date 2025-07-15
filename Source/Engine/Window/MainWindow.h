#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Window/Base/BaseWindow.h>

class MainWindow final : public BaseWindow {
public:
	MainWindow() = default;
	~MainWindow() override;

	bool Create(WindowInfo info) override;

	static void SetUseImmersiveDarkMode(HWND hWnd, bool darkMode);

	bool ProcessMessage() override;

	void SetResizeCallback(ResizeCallback callback) override;

protected:
	LRESULT WindowProc(HWND   hWnd, UINT msg, WPARAM wParam,
	                   LPARAM lParam) override;
};
