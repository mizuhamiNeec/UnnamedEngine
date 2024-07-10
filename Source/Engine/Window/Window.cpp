#include "Window.h"

#include <dwmapi.h>
#include <uxtheme.h>
#include <imgui/imgui.h>

#include "../Renderer/DirectX12.h"
#include "../Utils/Logger.h"

#pragma comment(lib, "winmm.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Window::Window() {
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	timeBeginPeriod(1); // システムタイマーの分解能を上げる
}

Window::~Window() {
	timeEndPeriod(1);

	CloseWindow(hWnd_);
	CoUninitialize();
}

LRESULT Window::WindowProc(const HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
		return true;
	}

	// ------------------------------------------------------------------------
	// どちらかのキーを押すと時が止まる Alt || F10キー対策
	// ------------------------------------------------------------------------
	if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) {
		return 0;
	}

	switch (msg) {
	case WM_SIZE:
		// TODO : サイズ可変にしたい
		break;
	case WM_CLOSE:
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

/// <summary>
/// メインウィンドウを作成します
/// </summary>
void Window::CreateMainWindow(const WindowConfig& windowConfig) {
	windowConfig_ = windowConfig;

	// クライアント領域のアスペクト比を計算しておく
	aspectRatio_ = static_cast<float>(windowConfig_.clientWidth) / static_cast<float>(windowConfig_.clientHeight);

	// ウィンドウプロシージャ
	wc_.cbSize = sizeof(WNDCLASSEX);
	wc_.style = CS_HREDRAW || CS_VREDRAW;
	wc_.lpfnWndProc = static_cast<WNDPROC>(WindowProc);
	wc_.hInstance = GetModuleHandle(nullptr);
	wc_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc_.lpszClassName = windowConfig.windowClassName;

	if (!RegisterClassEx(&wc_)) {
		MessageBox(nullptr, L"Failed to register window class.", nullptr, MB_ICONERROR);
		Log("Failed to register window class.");
		return;
	}

	RECT wrc = {0, 0, static_cast<LONG>(windowConfig_.clientWidth), static_cast<LONG>(windowConfig_.clientHeight)};

	AdjustWindowRectEx(&wrc, windowConfig_.dwStyle, false, windowConfig_.dwExStyle);

	hWnd_ = CreateWindowEx(
		windowConfig_.dwExStyle, // 拡張ウィンドウスタイル
		windowConfig_.windowClassName,
		windowConfig_.windowTitle, // ウィンドウタイトル
		windowConfig_.dwStyle, // ウィンドウスタイル
		CW_USEDEFAULT, CW_USEDEFAULT, // ウィンドウの初期位置 X,Y
		wrc.right - wrc.left, // ウィンドウの幅
		wrc.bottom - wrc.top, // ウィンドウの高さ
		nullptr, // このウィンドウの親
		nullptr, // メニュー
		wc_.hInstance,
		nullptr
	);

	BOOL value = TRUE;
	DwmSetWindowAttribute(hWnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));

	if (hWnd_ == nullptr) {
		MessageBox(nullptr, L"Failed to create window.", nullptr, MB_ICONERROR);
		Log("Failed to create window.");
		return;
	}

	ShowWindow(hWnd_, windowConfig_.nCmdShow);
}

WindowConfig Window::GetWindowConfig() const {
	return windowConfig_;
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

	return false;
}
