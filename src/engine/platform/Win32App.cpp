#include <pch.h>

#include <engine/platform/Win32App.h>
#include <engine/platform/Window.h>

bool Win32App::Init() {
	WindowDesc desc = {};
	desc.title      = "Main Window";
	mWindowManager.CreateNewWindow(desc);
	return true;
}

void Win32App::Shutdown() {
	mWindowManager.Shutdown();
}

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
