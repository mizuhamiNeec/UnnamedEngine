#pragma once
#include <Windows.h>
#include <cstdint>
#include <functional>
#include <string>

class BaseWindow {
public:
	using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;

	virtual ~BaseWindow() {}

	virtual bool Create(HINSTANCE hInstance, const std::string& className) = 0;
	virtual bool ProcessMessage() = 0;

	virtual HWND GetWindowHandle() const = 0;

	virtual uint32_t GetClientWidth() = 0;
	virtual uint32_t GetClientHeight() = 0;

	virtual void SetResizeCallback(ResizeCallback callback) = 0;
};
