#include <pch.h>

#include <engine/platform/Win32App.h>
#include <engine/platform/Window.h>

/// @brief Win32アプリケーションクラスを初期化します
/// @return 初期化に成功した場合はtrue、失敗した場合はfalse
bool Win32App::Init() {
	WindowDesc desc = {};
	desc.title      = "Main Window";
	mWindowManager.CreateNewWindow(desc);
	return true;
}

/// @brief Win32アプリケーションクラスをシャットダウンします
void Win32App::Shutdown() {
	mWindowManager.Shutdown();
}

/// @brief イベントをポーリングします
/// @return アプリケーションを終了する場合はtrue、続行する場合はfalse	
bool Win32App::PollEvents() {
	MSG msg = {};
	while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			return true; // アプリケーション終了
		}
		TranslateMessage(&msg);
		DispatchMessageW(&msg); // 各ウィンドウのWndProcに処理を委譲
	}
	return false; // アプリケーション続行
}

/// @brief ウィンドウマネージャーを取得します
/// @return ウィンドウマネージャーへの参照
WindowManager& Win32App::Windows() {
	return mWindowManager;
}
