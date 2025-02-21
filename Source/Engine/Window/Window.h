#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <Windows.h>
#include <Lib/Utils/ClientProperties.h>

#include <Window/Base/BaseWindow.h>

class Window final : public BaseWindow {
public:
	Window(std::wstring title, uint32_t width, uint32_t height, DWORD style = WS_OVERLAPPEDWINDOW, DWORD exStyle = 0);
	~Window() override;

	bool Create(HINSTANCE hInstance, const std::string& className = kWindowClassName) override;

	static void SetUseImmersiveDarkMode(HWND hWnd, bool darkMode);

	HWND GetWindowHandle() const override;
	[[nodiscard]] HINSTANCE GetHInstance() const;

	uint32_t GetClientWidth() override;
	uint32_t GetClientHeight() override;

	bool ProcessMessage() override;

	void SetResizeCallback(ResizeCallback callback) override;

private:
	// すべてのインスタンスで共有するウィンドウプロシージャ
	static LRESULT StaticWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	// 各インスタンス用
	LRESULT WindowProc(UINT msg, WPARAM wParam, LPARAM lParam);

private:
	WNDCLASSEX wc_ = {};

	static HWND hWnd_;
	std::wstring title_;
	static uint32_t width_;
	static uint32_t height_;
	DWORD style_;
	DWORD exStyle_;

	static ResizeCallback resizeCallback_;
};
