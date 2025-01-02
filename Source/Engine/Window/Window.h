#pragma once

#include <Windows.h>

#include <cstdint>

#include "../Lib/Utils/StrUtils.h"
#include "../Lib/Utils/ClientProperties.h"

class Window final {
public:
	Window(std::wstring title, uint32_t width, uint32_t height, DWORD style = WS_OVERLAPPEDWINDOW, DWORD exStyle = 0);
	~Window();

	bool Create(HINSTANCE hInstance, const std::string& className = kWindowClassName, WNDPROC wndProc = WindowProc);

	static void SetUseImmersiveDarkMode(HWND hWnd, bool darkMode);

	static HWND GetWindowHandle();
	HINSTANCE GetHInstance() const;

	static uint32_t GetClientWidth();
	static uint32_t GetClientHeight();

	static bool ProcessMessage();

private:
	static LRESULT WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	WNDCLASSEX wc_ = {};

	static HWND hWnd_;
	std::wstring title_;
	static uint32_t width_;
	static uint32_t height_;
	DWORD style_;
	DWORD exStyle_;
};
