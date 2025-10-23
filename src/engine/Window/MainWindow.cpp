#include <pch.h>
#include "engine/Window/MainWindow.h"

#include <dwmapi.h>

#include "engine/Input/InputSystem.h"
#include "engine/OldConsole/Console.h"
#include "engine/Window/WindowsUtils.h"

#ifdef _DEBUG
#include <imgui.h>
#endif

#pragma comment(lib, "winmm.lib")

#ifdef _DEBUG
/// @brief ImGuiのWndProcハンドラ
/// @param hWnd ウィンドウハンドル
/// @param msg メッセージ
/// @param wParam wParam
/// @param lParam lParam
/// @return 処理結果
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

/// @brief デストラクタ
MainWindow::~MainWindow() {
	if (mHWnd) {
		CloseWindow(mHWnd);
		SetWindowLongPtr(mHWnd, GWLP_USERDATA,
		                 reinterpret_cast<LONG_PTR>(nullptr));
		DestroyWindow(mHWnd);
		mHWnd = nullptr;
	}
	CoUninitialize();
	timeEndPeriod(1);
}

/// @brief ウィンドウを作成する
/// @param info ウィンドウ情報
/// @return 成功したらtrueを返す
bool MainWindow::Create(const WindowInfo info) {
	this->mInfo = info;

	const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) {
		Console::Print("Failed to initialize COM library");
	}
	timeBeginPeriod(1); // システムタイマーの分解能を上げる

	WNDCLASSEX wc           = {};
	wc.cbSize               = sizeof(WNDCLASSEX);
	wc.style                = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc          = StaticWindowProc;
	wc.hInstance            = mInfo.hInstance;
	wc.hCursor              = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground        = GetSysColorBrush(COLOR_BACKGROUND);
	wc.lpszMenuName         = nullptr;
	std::wstring classNameW = StrUtil::ToWString(mInfo.className);
	wc.lpszClassName        = classNameW.c_str();
	wc.hIcon                = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hIconSm              = LoadIcon(mInfo.hInstance, IDI_APPLICATION);

	if (!RegisterClassEx(&wc)) {
		Console::Print(
			"Failed to register window class. Error: " + std::to_string(
				GetLastError()) + "\n",
			kConTextColorError
		);
		return false;
	}

	RECT wrc{
		0, 0, static_cast<LONG>(mInfo.width), static_cast<LONG>(mInfo.height)
	};

	AdjustWindowRectEx(&wrc, mInfo.style, false, mInfo.exStyle);

	// アクティブなモニタを取得
	HMONITOR    hMonitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi       = {sizeof(mi)};
	if (!GetMonitorInfo(hMonitor, &mi)) {
		Console::Print("Failed to get monitor info.\n", kConTextColorError,
		               Channel::Engine);
		return false;
	}

	// モニタの中央位置を計算
	const int32_t posX = mi.rcMonitor.left + (mi.rcMonitor.right - mi.rcMonitor.
		left - (wrc.right - wrc.left)) / 2;
	const int32_t posY = mi.rcMonitor.top + (mi.rcMonitor.bottom - mi.rcMonitor.
		top - (wrc.bottom - wrc.top)) / 2;

	mHWnd = CreateWindowEx(
		mInfo.exStyle, // 拡張ウィンドウスタイル
		wc.lpszClassName,
		StrUtil::ToWString(mInfo.title).c_str(), // ウィンドウタイトル
		mInfo.style,                             // ウィンドウスタイル
		posX, posY,                              // ウィンドウの初期位置
		wrc.right - wrc.left,                    // ウィンドウの幅
		wrc.bottom - wrc.top,                    // ウィンドウの高さ
		nullptr,                                 // このウィンドウの親
		nullptr,                                 // メニュー
		wc.hInstance,
		this
	);

	if (!mHWnd) {
		Console::Print("Failed to create window.\n", kConTextColorError,
		               Channel::Engine);
		return false;
	}

	// テーマを設定
	SetUseImmersiveDarkMode(mHWnd, WindowsUtils::IsSystemDarkTheme());
	// ウィンドウを表示
	ShowWindow(mHWnd, SW_SHOW);
	UpdateWindow(mHWnd);
	// このウィンドウにフォーカス
	SetFocus(mHWnd);

	Console::Print("Complete create MainWindow.\n", kConTextColorCompleted,
	               Channel::Engine);

	return true;
}

/// @brief ウィンドウのダークモード設定を変更する
/// @param hWnd ウィンドウハンドル
/// @param darkMode ダークモードを使用するかどうか
void MainWindow::SetUseImmersiveDarkMode(const HWND hWnd, const bool darkMode) {
	const BOOL    value = darkMode;
	const HRESULT hr    = DwmSetWindowAttribute(
		hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
	if (FAILED(hr)) {
		const std::string errorMessage = WindowsUtils::GetHresultMessage(hr);
		Console::Print(errorMessage, kConTextColorError);
	}
}

/// @brief メッセージを処理する
/// @return 続行するならtrueを返す
bool MainWindow::ProcessMessage() {
	MSG msg = {};

	// Windowタイトルをコンソールから取得して更新
	SetWindowTextA(mHWnd, mInfo.title.c_str());

	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT) {
			return true;
		}
	}

	return false;
}

/// @brief リサイズコールバックを設定する
/// @param callback コールバック関数
void MainWindow::SetResizeCallback(ResizeCallback callback) {
	mResizeCallback = std::move(callback);
}

/// @brief ウィンドウプロシージャ
/// @param hWnd ウィンドウハンドル
/// @param msg メッセージ
/// @param wParam メッセージパラメータ1
/// @param lParam メッセージパラメータ2
/// @return 処理結果
LRESULT MainWindow::WindowProc([[maybe_unused]] HWND hWnd, const UINT msg,
                               const WPARAM wParam, const LPARAM lParam) {
#ifdef _DEBUG
	if (ImGui_ImplWin32_WndProcHandler(mHWnd, msg, wParam, lParam)) {
		return true;
	}
#endif

	// ------------------------------------------------------------------------
	// ザ・ワールド対策 Alt || F10キー対策
	// ------------------------------------------------------------------------
	if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) {
		if (wParam == VK_F4 && (lParam & (1 << 29))) {
			// Alt + F4
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
			mInfo.width  = LOWORD(lParam);
			mInfo.height = HIWORD(lParam);
			if (mResizeCallback) {
				mResizeCallback(mInfo.width, mInfo.height);
			}
		}
		break;
	case WM_CLOSE:
	case WM_DESTROY: PostQuitMessage(0);
		return 0;
	default: return DefWindowProc(mHWnd, msg, wParam, lParam);
	}

	return 0;
}
