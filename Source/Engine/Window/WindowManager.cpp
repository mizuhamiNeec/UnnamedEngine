#include "WindowManager.h"

WindowManager::WindowManager() = default;

WindowManager::~WindowManager() {
	ClearWindows();
}

void WindowManager::AddWindow(std::unique_ptr<BaseWindow> window) {
	windows_.emplace_back(std::move(window));
}

bool WindowManager::ProcessMessage() {
	MSG msg = {};
	bool quit = false;

	// 全体のメッセージループ
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT) {
			quit = true;
			break;
		}
	}
	// 各ウィンドウごとの処理
	for (const auto& win : windows_) {
		if (win->ProcessMessage()) {
			quit = true;
			break;
		}
	}

	return quit;
}

BaseWindow* WindowManager::GetMainWindow() {
	return windows_.empty() ? nullptr : windows_[0].get();
}

void WindowManager::ClearWindows() {
	windows_.clear();
}

const std::vector<std::unique_ptr<BaseWindow>>& WindowManager::GetWindows() {
	return windows_;
}

std::vector<std::unique_ptr<BaseWindow>> WindowManager::windows_ = {};