#include <pch.h>
#include <ranges>

#include <engine/public/platform/Window.h>
#include <engine/public/platform/WindowManager.h>

WindowManager::WindowManager(const HINSTANCE hInstance): mHInstance(hInstance) {
}

void WindowManager::Shutdown() {
	for (auto& window : mWindows | std::views::values) {
		window.Show(SW_HIDE);
	}
	mWindows.clear();
	mNextWindowId = 1;
}

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

void WindowManager::DestroyWindow(const uint32_t windowId) {
	mWindows.erase(windowId);
}

std::unordered_map<uint32_t, Window>& WindowManager::GetWindows() {
	return mWindows;
}

std::optional<std::reference_wrapper<Window>> WindowManager::Get(
	const uint32_t windowId
) {
	const auto it = mWindows.find(windowId);
	if (it == mWindows.end()) {
		return std::nullopt;
	}
	return it->second;
}
