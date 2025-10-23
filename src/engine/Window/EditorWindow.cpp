#include <pch.h>

//-----------------------------------------------------------------------------

#include "engine/Window/EditorWindow.h"

#include "engine/OldConsole/Console.h"
#include "engine/Window/WindowManager.h"
#include "engine/Window/base/BaseWindow.h"

/// @brief コンストラクタ
EditorWindow::EditorWindow() = default;

/// @brief デストラクタ
EditorWindow::~EditorWindow() {
	CloseWindow(mHWnd);
}

/// @brief ウィンドウを作成する
/// @param info ウィンドウ情報
/// @return 成功したらtrueを返す
bool EditorWindow::Create(const WindowInfo info) {
	this->mInfo = info;

	WNDCLASSEX wc    = {};
	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = StaticWindowProc;
	wc.hInstance     = mInfo.hInstance;
	wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
	wc.lpszMenuName  = nullptr;
	wc.lpszClassName = StrUtil::ToWString(mInfo.className).c_str();
	wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hIconSm       = LoadIcon(mInfo.hInstance, IDI_APPLICATION);

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

	mHWnd = CreateWindowEx(
		mInfo.exStyle, // 拡張ウィンドウスタイル
		wc.lpszClassName,
		StrUtil::ToWString(mInfo.title).c_str(),              // ウィンドウタイトル
		mInfo.style,                                          // ウィンドウスタイル
		CW_USEDEFAULT, CW_USEDEFAULT,                         // ウィンドウの初期位置
		wrc.right - wrc.left,                                 // ウィンドウの幅
		wrc.bottom - wrc.top,                                 // ウィンドウの高さ
		OldWindowManager::GetMainWindow()->GetWindowHandle(), // このウィンドウの親
		nullptr,                                              // メニュー
		wc.hInstance,
		this
	);

	if (!mHWnd) {
		Console::Print("Failed to create window.\n", kConTextColorError,
		               Channel::Engine);
		return false;
	}

	// ウィンドウを表示
	ShowWindow(mHWnd, SW_SHOW);
	UpdateWindow(mHWnd);
	// このウィンドウにフォーカス
	SetFocus(mHWnd);

	Console::Print("Complete create MainWindow.\n", kConTextColorCompleted,
	               Channel::Engine);

	return true;
}

/// @brief メッセージを処理する
/// @return 続行するならtrueを返す
bool EditorWindow::ProcessMessage() {
	MSG msg = {};

	SetWindowTextA(mHWnd, mInfo.title.c_str());

	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return false;
}

/// @brief リサイズコールバックを設定する
/// @param callback コールバック関数
void EditorWindow::SetResizeCallback(ResizeCallback callback) {
	mResizeCallback = std::move(callback);
}

/// @brief ウィンドウプロシージャ
/// @param hWnd ウィンドウハンドル
/// @param msg メッセージ
/// @param wParam メッセージパラメータ1
/// @param lParam メッセージパラメータ2
/// @return 処理結果
LRESULT EditorWindow::WindowProc(
	const HWND   hWnd,
	const UINT   msg,
	const WPARAM wParam,
	const LPARAM lParam
) {
	return DefWindowProc(hWnd, msg, wParam, lParam);
}
