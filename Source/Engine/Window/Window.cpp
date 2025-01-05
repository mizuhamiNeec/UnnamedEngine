#include "Window.h"

#include <dwmapi.h>
#include <imgui.h>
#include <utility>

#include "WindowsUtils.h"
#include "../Input/InputSystem.h"
#include "Lib/Console/Console.h"

#pragma comment(lib, "winmm.lib")

#ifdef _DEBUG
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

Window::Window(
	std::wstring title,
	const uint32_t width,
	const uint32_t height,
	const DWORD style,
	const DWORD exStyle
) :
	title_(std::move(title)),
	style_(style),
	exStyle_(exStyle) {
	width_ = width;
	height_ = height;
	const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		Console::Print("Failed to initialize COM library");
	}
	timeBeginPeriod(1); // システムタイマーの分解能を上げる
}

Window::~Window() {
	CloseWindow(hWnd_);
	CoUninitialize();
	timeEndPeriod(1);
}

bool Window::Create(const HINSTANCE hInstance, [[maybe_unused]] const std::string& className, const WNDPROC wndProc) {
	wc_.cbSize = sizeof(WNDCLASSEX);
	wc_.style = CS_HREDRAW | CS_VREDRAW;
	wc_.lpfnWndProc = wndProc;
	wc_.hInstance = hInstance;
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc_.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
	wc_.lpszMenuName = nullptr;
	wc_.lpszClassName = StrUtils::ToString(className);
	wc_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wc_.hIconSm = LoadIcon(hInstance, IDI_APPLICATION);

	if (!RegisterClassEx(&wc_))
	{
		Console::Print(
			"Failed to register window class. Error: " + std::to_string(GetLastError()) + "\n",
			kConsoleColorError
		);
		return false;
	}

	RECT wrc{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};

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

	if (!hWnd_)
	{
		Console::Print("Failed to create window.\n", kConsoleColorError, Channel::kEngine);
		return false;
	}

	// テーマを設定
	SetUseImmersiveDarkMode(hWnd_, WindowsUtils::IsSystemDarkTheme());
	// ウィンドウを表示
	ShowWindow(hWnd_, SW_SHOW);
	UpdateWindow(hWnd_);
	// このウィンドウにフォーカス
	SetFocus(hWnd_);

	Console::Print("Complete create Window.\n", kConsoleColorCompleted, Channel::kEngine);

	return true;
}

void Window::SetUseImmersiveDarkMode(const HWND hWnd, const bool darkMode) {
	const BOOL value = darkMode;
	const HRESULT hr = DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
	if (FAILED(hr))
	{
		const std::string errorMessage = WindowsUtils::GetHresultMessage(hr);
		Console::Print(errorMessage, kConsoleColorError);
	}
}

HINSTANCE Window::GetHInstance() const {
	return wc_.hInstance;
}

uint32_t Window::GetClientWidth() {
	RECT rect;
	if (GetClientRect(hWnd_, &rect))
	{
		width_ = rect.right - rect.left;
	}
	return width_;
}

uint32_t Window::GetClientHeight() {
	RECT rect;
	if (GetClientRect(hWnd_, &rect))
	{
		height_ = rect.bottom - rect.top;
	}
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
	if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP)
	{
		return 0;
	}

	switch (msg)
	{
	case WM_SETTINGCHANGE: // Windowsの設定が変更された
		if (lParam)
		{
			auto immersiveColorSet = std::bit_cast<const wchar_t*>(lParam);
			// 変更された設定が "ImmersiveColorSet" か?
			if (immersiveColorSet && wcscmp(immersiveColorSet, L"ImmersiveColorSet") == 0)
			{
				static int sMode = 0;
				const bool darkMode = WindowsUtils::IsSystemDarkTheme(); // 現在のテーマを取得
				// 前回のテーマと異なる場合
				if (static_cast<bool>(sMode) != darkMode)
				{
					sMode = darkMode;
					SetUseImmersiveDarkMode(hWnd, darkMode); // ウィンドウのモードを設定
					Console::Print(
						std::format("Setting Window Mode to {}...\n", sMode ? "Dark" : "Light"), kConsoleColorWait,
						Channel::kEngine
					);
				}
			}
		}
		break;
	case WM_INPUT:
		{
			InputSystem::ProcessInput(lParam);
			break;
		}
	case WM_CLOSE:
	case WM_DESTROY: PostQuitMessage(0);
		return 0;
	default: return DefWindowProc(hWnd, msg, wParam, lParam);
	}

	return 0;
}

bool Window::ProcessMessage() {
	MSG msg = {};

	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			return true;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return false;
}

HWND Window::GetWindowHandle() {
	return hWnd_;
}

HWND Window::hWnd_ = nullptr;
uint32_t Window::width_ = 0;
uint32_t Window::height_ = 0;
