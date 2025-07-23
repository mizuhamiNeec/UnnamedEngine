#include <engine/public/Window/WindowManager.h>

OldWindowManager::OldWindowManager() {
};

OldWindowManager::~OldWindowManager() {
	ClearWindows();
}

void OldWindowManager::AddWindow(std::unique_ptr<BaseWindow> window) {
	windows_.emplace_back(std::move(window));
}

bool OldWindowManager::ProcessMessage() {
	MSG  msg  = {};
	bool quit = false;

	// 全体のメッセージループ
	while (PeekMessage(&msg, windows_[0]->GetWindowHandle(), 0, 0, PM_REMOVE)) {
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

BaseWindow* OldWindowManager::GetMainWindow() {
	return windows_.empty() ? nullptr : windows_[0].get();
}

void OldWindowManager::ClearWindows() {
	windows_.clear();
}

const std::vector<std::unique_ptr<BaseWindow>>& OldWindowManager::GetWindows() {
	return windows_;
}

std::vector<std::unique_ptr<BaseWindow>> OldWindowManager::windows_ = {};
