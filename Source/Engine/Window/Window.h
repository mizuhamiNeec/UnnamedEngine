#pragma once

#include <cstdint>
#include <Windows.h>

struct WindowConfig {
	LPCWSTR windowTitle;
	uint32_t clientWidth;
	uint32_t clientHeight;
	LPCWSTR windowClassName;
	DWORD dwStyle;
	DWORD dwExStyle;
	int nCmdShow;
};

struct CaptionButton {
	UINT uCmd; // Command to send when clicked (WM_COMMAND)
	int nRightBorder; // Pixels between this button and buttons to the right
	HBITMAP hBmp; // Bitmap to display
	BOOL fPressed; // Is the button pressed in or out?
};

constexpr int maxTitleButtons = 2;

struct CustomCaption {
	CaptionButton buttons[maxTitleButtons];
	int nNumButtons;
	BOOL fMouseDown; // is the mouse button being clicked?
	WNDPROC wpOldProc; // old window procedure
	int iActiveButton; // the button index being clicked.
};

class Window final {
public:
	Window();
	~Window();

	static LRESULT WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void CreateMainWindow(const WindowConfig& windowConfig);
	WindowConfig GetWindowConfig() const;

	static bool ProcessMessage();

	HWND GetHWND() const { return hWnd_; }

	HINSTANCE GetHInstance() const {
		return wc_.hInstance;
	}

private:
	HWND hWnd_ = nullptr;
	WNDCLASSEX wc_ = {};
	WindowConfig windowConfig_ = {};
	float aspectRatio_ = 0;
};
