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

	HWND GetWindowHandle() const;
	HINSTANCE GetHInstance() const;

	uint32_t GetClientWidth() const;
	uint32_t GetClientHeight() const;

	bool ProcessMessage();

private:
	static LRESULT WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	WNDCLASSEX wc_ = {};

	HWND hWnd_ = nullptr;
	std::wstring title_;
	uint32_t width_;
	uint32_t height_;
	DWORD style_;
	DWORD exStyle_;

	static int deltaX_;
	static int deltaY_;
};
