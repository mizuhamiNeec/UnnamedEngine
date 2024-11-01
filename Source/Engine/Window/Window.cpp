#include "Window.h"

#ifdef _DEBUG
#include <imgui/imgui.h>
#endif

#include <dwmapi.h>

#include <utility>

#include "WindowsUtils.h"

#include "../Lib/Console/Console.h"

#pragma comment(lib, "winmm.lib")

#ifdef _DEBUG
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif	

Window::Window(std::wstring title, const uint32_t width, const uint32_t height, const DWORD style, const DWORD exStyle) : title_(std::move(title)), width_(width), height_(height), style_(style), exStyle_(exStyle) {
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	timeBeginPeriod(1); // システムタイマーの分解能を上げる
}

Window::~Window() {
	CloseWindow(hWnd_);
	CoUninitialize();
	timeEndPeriod(1);
}

bool Window::Create(const HINSTANCE hInstance, [[maybe_unused]] const std::string& className, const WNDPROC wndProc) {
	Console::Print("Creating Window...\n", kConsoleColorWait);

	wc_.cbSize = sizeof(WNDCLASSEX);
	wc_.style = CS_HREDRAW | CS_VREDRAW;
	wc_.lpfnWndProc = wndProc;
	wc_.hInstance = hInstance;
	wc_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc_.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
	wc_.lpszMenuName = nullptr;
	wc_.lpszClassName = ConvertString::ToString(className);
	wc_.hIconSm = LoadIcon(hInstance, IDI_APPLICATION);

	if (!RegisterClassEx(&wc_)) {
		Console::Print("Failed to register window class. Error: " + std::to_string(GetLastError()) + "\n", kConsoleColorError);
		return false;
	}

	Console::Print("Window class registered.\n");

	RECT wrc{ 0,0, static_cast<LONG>(width_), static_cast<LONG>(height_) };

	AdjustWindowRectEx(&wrc, style_, false, exStyle_);

	hWnd_ = CreateWindowEx(
		exStyle_, // 拡張ウィンドウスタイル
		wc_.lpszClassName,
		title_.c_str(), // ウィンドウタイトル
		style_, // ウィンドウスタイル
		CW_USEDEFAULT, CW_USEDEFAULT, // ウィンドウの初期位置
		wrc.right - wrc.left, // ウィンドウの幅
		wrc.bottom - wrc.top, // ウィンドウの高さ
		nullptr, // このウィンドウの親
		nullptr, // メニュー
		wc_.hInstance,
		nullptr
	);

	if (!hWnd_) {
		Console::Print("Failed to create window.\n", kConsoleColorError);
		return false;
	}

	// テーマを設定
	SetUseImmersiveDarkMode(hWnd_, WindowsUtils::IsSystemDarkTheme());
	// ウィンドウを表示
	ShowWindow(hWnd_, SW_SHOW);
	UpdateWindow(hWnd_);
	// このウィンドウにフォーカス
	SetFocus(hWnd_);

	Console::Print("Complete create Window.\n", kConsoleColorCompleted);

	return true;
}

void Window::SetUseImmersiveDarkMode(const HWND hWnd, const bool darkMode) {
	const BOOL value = darkMode;
	DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
}

HINSTANCE Window::GetHInstance() const { return wc_.hInstance; }

uint32_t Window::GetClientWidth() const {
	return width_;
}

uint32_t Window::GetClientHeight() const {
	return height_;
}

LRESULT Window::WindowProc(const HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
#ifdef _DEBUG
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
		return true;
	}
#endif

	// ------------------------------------------------------------------------
	// どちらかのキーを押すと時が止まる Alt || F10キー対策
	// ------------------------------------------------------------------------
	if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) {
		return 0;
	}

	switch (msg) {
	case WM_SETTINGCHANGE: // Windowsの設定が変更された
		if (lParam) {
			const wchar_t* immersiveColorSet = std::bit_cast<const wchar_t*>(lParam);
			// 変更された設定が "ImmersiveColorSet" か?
			if (immersiveColorSet && wcscmp(immersiveColorSet, L"ImmersiveColorSet") == 0) {
				static int sMode = 0;
				bool darkMode = WindowsUtils::IsSystemDarkTheme(); // 現在のテーマを取得
				// 前回のテーマと異なる場合
				if (static_cast<bool>(sMode) != darkMode) {
					sMode = darkMode;
					SetUseImmersiveDarkMode(hWnd, darkMode); // ウィンドウのモードを設定
				}
			}
		}
		break;
	case WM_CLOSE:
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool Window::ProcessMessage() {
	MSG msg = {};

	if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (msg.message == WM_QUIT) {
		return true;
	}

	RECT rect;
	if (GetClientRect(hWnd_, &rect)) {
		width_ = rect.right - rect.left;
		height_ = rect.bottom - rect.top;
	}

	return false;
}

HWND Window::GetWindowHandle() const { return hWnd_; }
