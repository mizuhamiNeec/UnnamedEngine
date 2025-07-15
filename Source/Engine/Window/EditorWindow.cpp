#include "EditorWindow.h"

#include "WindowManager.h"
#include "Lib/Utils/StrUtil.h"
#include "SubSystem/Console/Console.h"

EditorWindow::EditorWindow() {}

EditorWindow::~EditorWindow() {
	CloseWindow(hWnd_);
}

bool EditorWindow::Create(const WindowInfo info) {
	this->info_ = info;

	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = StaticWindowProc;
	wc.hInstance = info_.hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = StrUtil::ToString(info_.className);
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

	hWnd_ = CreateWindowEx(
		info_.exStyle, // 拡張ウィンドウスタイル
		wc.lpszClassName,
		StrUtil::ToWString(info_.title).c_str(), // ウィンドウタイトル
		info_.style, // ウィンドウスタイル
		CW_USEDEFAULT, CW_USEDEFAULT, // ウィンドウの初期位置
		wrc.right - wrc.left, // ウィンドウの幅
		wrc.bottom - wrc.top, // ウィンドウの高さ
		WindowManager::GetMainWindow()->GetWindowHandle(), // このウィンドウの親
		nullptr, // メニュー
		wc.hInstance,
		this
	);

	if (!hWnd_) {
		Console::Print("Failed to create window.\n", kConTextColorError, Channel::Engine);
		return false;
	}

	// テーマを設定
	//SetUseImmersiveDarkMode(hWnd_, WindowsUtils::IsSystemDarkTheme());
	// ウィンドウを表示
	ShowWindow(hWnd_, SW_SHOW);
	UpdateWindow(hWnd_);
	// このウィンドウにフォーカス
	SetFocus(hWnd_);

	Console::Print("Complete create MainWindow.\n", kConTextColorCompleted, Channel::Engine);

	return true;
}

bool EditorWindow::ProcessMessage() {
	MSG msg = {};

	SetWindowTextA(hWnd_, info_.title.c_str());

	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return false;
}

void EditorWindow::SetResizeCallback(ResizeCallback callback) {
	resizeCallback_ = std::move(callback);
}

LRESULT EditorWindow::WindowProc(const HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	return DefWindowProc(hWnd, msg, wParam, lParam);
}