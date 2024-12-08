#include "Window.h"

#include <dwmapi.h>
#include <utility>

#include "WindowsUtils.h"

#include "../Lib/Console/Console.h"

#pragma comment(lib, "winmm.lib")

#ifdef _DEBUG
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

int Window::deltaX_ = 0;
int Window::deltaY_ = 0;

Window::Window(
	std::wstring title,
	const uint32_t width,
	const uint32_t height,
	const DWORD style,
	const DWORD exStyle
) : title_(std::move(title)), width_(width), height_(height), style_(style), exStyle_(exStyle) {
	const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) {
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
	Console::Print("Creating Window...\n", kConsoleColorWait);

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

	if (!RegisterClassEx(&wc_)) {
		Console::Print("Failed to register window class. Error: " + std::to_string(GetLastError()) + "\n",
		               kConsoleColorError);
		return false;
	}

	Console::Print("Window class registered.\n");

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
	const HRESULT hr = DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
	if (FAILED(hr)) {
		const std::string errorMessage = WindowsUtils::GetHresultMessage(hr);
		Console::Print(errorMessage, kConsoleColorError);
	}
}

HINSTANCE Window::GetHInstance() const {
	return wc_.hInstance;
}

uint32_t Window::GetClientWidth() {
	RECT rect;
	if (GetClientRect(hWnd_, &rect)) {
		width_ = rect.right - rect.left;
	}
	return width_;
}

uint32_t Window::GetClientHeight() {
	RECT rect;
	if (GetClientRect(hWnd_, &rect)) {
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
				const bool darkMode = WindowsUtils::IsSystemDarkTheme(); // 現在のテーマを取得
				// 前回のテーマと異なる場合
				if (static_cast<bool>(sMode) != darkMode) {
					sMode = darkMode;
					SetUseImmersiveDarkMode(hWnd, darkMode); // ウィンドウのモードを設定
					Console::Print(std::format("Setting Window Mode to {}...\n", sMode ? "Dark" : "Light"));
				}
			}
		}
		break;
	case WM_CLOSE:
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default: return DefWindowProc(hWnd, msg, wParam, lParam);
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

	return false;
}

void Window::LockMouse() const {
	static POINT prevCursorPos = {}; // 前回のカーソル位置
	POINT currentCursorPos; // 現在のカーソル位置

	// 現在のマウスカーソル位置を取得
	GetCursorPos(&currentCursorPos);

	// マウスの移動量を計算
	int deltaX = currentCursorPos.x - prevCursorPos.x;
	int deltaY = currentCursorPos.y - prevCursorPos.y;

#ifdef _DEBUG
	// ImGuiのマウスデルタに反映
	ImGuiIO& io = ImGui::GetIO();
	io.MouseDelta.x = static_cast<float>(deltaX);
	io.MouseDelta.y = static_cast<float>(deltaY);
#endif

	// ウィンドウの中央座標を計算
	RECT clientRect;
	GetClientRect(hWnd_, &clientRect);

	POINT centerPos = {
		clientRect.left + (clientRect.right - clientRect.left) / 2,
		clientRect.top + (clientRect.bottom - clientRect.top) / 2
	};
	MapWindowPoints(hWnd_, nullptr, &centerPos, 1); // クライアント座標をスクリーン座標に変換

	// マウスを画面中央に移動
	SetCursorPos(centerPos.x, centerPos.y);

	// 前回位置を中央にリセット
	prevCursorPos = centerPos;
}

void Window::UnlockMouse() {
	ClipCursor(nullptr);
}

HWND Window::GetWindowHandle() const { return hWnd_; }
