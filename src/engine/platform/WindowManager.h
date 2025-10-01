#pragma once
#include <unordered_map>

#include <engine/platform/Window.h>

struct WindowDesc;

/// @class WindowManager
/// @brief 複数ウィンドウを管理します。
class WindowManager {
public:
	explicit WindowManager(HINSTANCE hInstance);

	void Shutdown();

	Window& CreateNewWindow(const WindowDesc& desc);
	void    DestroyWindow(uint32_t windowId);

	std::unordered_map<uint32_t, Window>&         GetWindows();
	std::optional<std::reference_wrapper<Window>> Get(uint32_t windowId);

private:
	HINSTANCE                            mHInstance;
	std::atomic<uint32_t>                mNextWindowId = { 1 };
	std::unordered_map<uint32_t, Window> mWindows;
};
