#pragma once
#include <string>
#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

struct WindowDesc {
	std::string title;
	uint32_t    width  = 1280;
	uint32_t    height = 720;
	DWORD       style  = WS_OVERLAPPEDWINDOW;
};

class Window {
public:
	Window(const WindowDesc& desc, HINSTANCE hInstance, uint32_t wndId);
	~Window();

	[[nodiscard]] HWND     GetHWnd() const;
	[[nodiscard]] uint32_t GetID() const;

	void Show(int cmdShow = SW_SHOWDEFAULT) const;

private:
	static LRESULT CALLBACK  WndProc(HWND, UINT, WPARAM, LPARAM);
	static constexpr auto kClassName = L"WindowClass";

	HWND     mHWnd;
	uint32_t mWndId;
};
