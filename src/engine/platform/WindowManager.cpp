#include <pch.h>
#include <ranges>

#include <engine/platform/Window.h>
#include <engine/platform/WindowManager.h>

/// @brief コンストラクタ
WindowManager::WindowManager(const HINSTANCE hInstance) : mHInstance(
	hInstance) {
}

/// @brief シャットダウン処理
void WindowManager::Shutdown() {
	for (auto& window : mWindows | std::views::values) {
		window.Show(SW_HIDE);
	}
	mWindows.clear();
	mNextWindowId = 1;
}

/// @brief 新しいウィンドウを作成します
/// @param desc ウィンドウの説明構造体
/// @return 作成されたウィンドウの参照
Window& WindowManager::CreateNewWindow(const WindowDesc& desc) {
	uint32_t windowId       = mNextWindowId++;
	auto     [it, inserted] = mWindows.try_emplace(
		windowId,
		desc,
		mHInstance,
		windowId
	);
	it->second.Show();
	return it->second;
}

/// @brief 指定したウィンドウを破棄します
/// @param windowId 破棄するウィンドウのID
void WindowManager::DestroyWindow(const uint32_t windowId) {
	mWindows.erase(windowId);
}

/// @brief 登録されている全てのウィンドウを取得します
/// @return ウィンドウIDとウィンドウのマップ
std::unordered_map<uint32_t, Window>& WindowManager::GetWindows() {
	return mWindows;
}

/// @brief 指定したIDのウィンドウを取得します
/// @param windowId ウィンドウID
/// @return ウィンドウの参照ラッパー、存在しない場合はstd::nullopt
std::optional<std::reference_wrapper<Window>> WindowManager::Get(
	const uint32_t windowId
) {
	const auto it = mWindows.find(windowId);
	if (it == mWindows.end()) {
		return std::nullopt;
	}
	return it->second;
}
