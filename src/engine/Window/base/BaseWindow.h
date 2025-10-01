#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cstdint>
#include <functional>
#include <string>

struct WindowInfo {
	std::string title = "Unnamed Window";
	uint32_t width = 300;
	uint32_t height = 400;
	DWORD style = WS_OVERLAPPEDWINDOW;
	DWORD exStyle = 0;
	HINSTANCE hInstance = nullptr;
	std::string className = "Unnamed WindowClass";
};


class BaseWindow {
public:
	using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;

	virtual ~BaseWindow() {
		if (hWnd_) {
			SetWindowLongPtr(hWnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(nullptr));
			DestroyWindow(hWnd_);
			hWnd_ = nullptr;
		}
	}

	virtual bool Create(WindowInfo info) = 0;
	virtual bool ProcessMessage() = 0;

	virtual void SetResizeCallback(ResizeCallback callback) = 0;

	HWND GetWindowHandle() const {
		return hWnd_;
	}

	uint32_t GetClientWidth() const {
		return info_.width;
	}

	uint32_t GetClientHeight() const {
		return info_.height;
	}

protected:
	static LRESULT StaticWindowProc(const HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
		BaseWindow* pThis = nullptr;
		if (msg == WM_NCCREATE) {
			auto pCreate = reinterpret_cast<LPCREATESTRUCT>(lParam);
			pThis = static_cast<BaseWindow*>(pCreate->lpCreateParams);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
			pThis->hWnd_ = hWnd;
		} else {
			pThis = reinterpret_cast<BaseWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
		}
		if (pThis) {
			return pThis->WindowProc(hWnd, msg, wParam, lParam);
		}
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	virtual LRESULT WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) = 0;

	HWND hWnd_ = nullptr;

	WindowInfo info_;

	ResizeCallback resizeCallback_;
};
