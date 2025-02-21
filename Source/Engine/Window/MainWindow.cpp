#include "MainWindow.h"

#include <dwmapi.h>
#include <ImGuiManager/ImGuiManager.h>
#include <Input/InputSystem.h>
#include <Lib/Utils/StrUtils.h>
#include <SubSystem/Console/Console.h>
#include <Window/WindowsUtils.h>

#pragma comment(lib, "winmm.lib")

#ifdef _DEBUG
extern IMGUI_IMPL_API LRESULT ImGuiImplWin32WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

MainWindow::~MainWindow() {
	if (hWnd_) {
		CloseWindow(hWnd_);
		SetWindowLongPtr(hWnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(nullptr));
		DestroyWindow(hWnd_);
		hWnd_ = nullptr;
	}
	CoUninitialize();
	timeEndPeriod(1);
}

bool MainWindow::Create(const WindowInfo info) {
	this->info_ = info;

	const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) {
		Console::Print("Failed to initialize COM library");
	}
	timeBeginPeriod(1); // システムタイマーの分解能を上げる

	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = StaticWindowProc;
	wc.hInstance = info_.hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = StrUtils::ToString(info_.className);
	wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hIconSm = LoadIcon(info_.hInstance, IDI_APPLICATION);

	if (!RegisterClassEx(&wc)) {
		Console::Print(
			"Failed to register window class. Error: " + std::to_string(GetLastError()) + "\n",
			kConTextColorError
		);
		return false;
	}

	RECT wrc{ 0, 0, static_cast<LONG>(info_.width), static_cast<LONG>(info_.height) };

	AdjustWindowRectEx(&wrc, info_.style, false, info_.exStyle);

	// アクティブなモニタを取得
	HMONITOR hMonitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi = { sizeof(mi) };
	if (!GetMonitorInfo(hMonitor, &mi)) {
		Console::Print("Failed to get monitor info.\n", kConTextColorError, Channel::Engine);
		return false;
	}

	// モニタの中央位置を計算
	const int32_t posX = mi.rcMonitor.left + (mi.rcMonitor.right - mi.rcMonitor.left - (wrc.right - wrc.left)) / 2;
	const int32_t posY = mi.rcMonitor.top + (mi.rcMonitor.bottom - mi.rcMonitor.top - (wrc.bottom - wrc.top)) / 2;

	hWnd_ = CreateWindowEx(
		info_.exStyle, // 拡張ウィンドウスタイル
		wc.lpszClassName,
		StrUtils::ToWString(info_.title).c_str(), // ウィンドウタイトル
		info_.style, // ウィンドウスタイル
		posX, posY, // ウィンドウの初期位置
		wrc.right - wrc.left, // ウィンドウの幅
		wrc.bottom - wrc.top, // ウィンドウの高さ
		nullptr, // このウィンドウの親
		nullptr, // メニュー
		wc.hInstance,
		this
	);

	if (!hWnd_) {
		Console::Print("Failed to create window.\n", kConTextColorError, Channel::Engine);
		return false;
	}

	// テーマを設定
	SetUseImmersiveDarkMode(hWnd_, WindowsUtils::IsSystemDarkTheme());
	// ウィンドウを表示
	ShowWindow(hWnd_, SW_SHOW);
	UpdateWindow(hWnd_);
	// このウィンドウにフォーカス
	SetFocus(hWnd_);

	Console::Print("Complete create MainWindow.\n", kConTextColorCompleted, Channel::Engine);

	return true;
}

void MainWindow::SetUseImmersiveDarkMode(const HWND hWnd, const bool darkMode) {
	const BOOL value = darkMode;
	const HRESULT hr = DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
	if (FAILED(hr)) {
		const std::string errorMessage = WindowsUtils::GetHresultMessage(hr);
		Console::Print(errorMessage, kConTextColorError);
	}
}

bool MainWindow::ProcessMessage() {
	MSG msg = {};

	// Windowタイトルをコンソールから取得して更新
	SetWindowTextA(hWnd_, info_.title.c_str());

	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT) {
			return true;
		}
	}

	return false;
}

void MainWindow::SetResizeCallback(ResizeCallback callback) {
	resizeCallback_ = std::move(callback);
}

LRESULT MainWindow::WindowProc([[maybe_unused]] HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
#ifdef _DEBUG
	if (ImGuiImplWin32WndProcHandler(hWnd_, msg, wParam, lParam)) {
		return true;
	}
#endif

	// ------------------------------------------------------------------------
	// ザ・ワールド対策 Alt || F10キー対策
	// ------------------------------------------------------------------------
	if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) {
		if (wParam == VK_F4 && (lParam & (1 << 29))) { // Alt + F4
			PostQuitMessage(0);
			return 0;
		}
		return 0;
	}

	switch (msg) {
	case WM_INPUT:
		// RawInputの処理
		InputSystem::ProcessInput(static_cast<long>(lParam));
		break;
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			// ウィンドウが非アクティブになったときリセットする
			InputSystem::ResetAllKeys();
		}
		break;
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED) {
			info_.width = LOWORD(lParam);
			info_.height = HIWORD(lParam);
			if (resizeCallback_) {
				resizeCallback_(info_.width, info_.height);
			}
		}
		break;
	case WM_CLOSE:
	case WM_DESTROY: PostQuitMessage(0);
		return 0;
	default: return DefWindowProc(hWnd_, msg, wParam, lParam);
	}

	return 0;
}
