#pragma once
#include <engine/platform/WindowManager.h>

class Win32App {
public:
	explicit Win32App(const HINSTANCE hInstance)
		: mHInstance(hInstance), mWindowManager(hInstance) {
	}

	bool Init();
	void Shutdown();

	static bool PollEvents();

	WindowManager& Windows() {
		return mWindowManager;
	}

private:
	HINSTANCE     mHInstance = {};
	WindowManager mWindowManager;
};
