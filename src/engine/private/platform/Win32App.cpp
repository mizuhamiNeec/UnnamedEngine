#include <pch.h>

#include <engine/public/platform/Win32App.h>
#include <engine/public/platform/Window.h>

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
			return false;
		}
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return true;
}
