#include "engine/public/platform/Window.h"

#include <pch.h>

Window::Window(
	const WindowDesc& desc, const HINSTANCE hInstance,
	const uint32_t    wndId
) : mWndId(wndId) {
	// 一回だけウィンドウを登録する
	static bool bRegistered = false;
	if (!bRegistered) {
		WNDCLASSEXW wc = {};

		wc.cbSize        = sizeof(WNDCLASSEXW);
		wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
		wc.hInstance     = hInstance;
		wc.lpszClassName = kClassName;
		wc.lpfnWndProc   = &Window::WndProc;
		wc.style         = CS_HREDRAW | CS_VREDRAW;
		RegisterClassExW(&wc);
		bRegistered = true;
	}

	RECT rect = {
		0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height)
	};
	AdjustWindowRect(&rect, desc.style, FALSE);

	mHWnd = CreateWindowExW(
		0,
		kClassName,
		StrUtil::ToWString(desc.title).c_str(),
		desc.style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left,
		rect.bottom - rect.top,
		nullptr,
		nullptr,
		hInstance, this
	);

	if (!mHWnd) {
		throw std::runtime_error("Failed to create window.");
	}
}

Window::~Window() {
	if (mHWnd) {
		DestroyWindow(mHWnd);
		mHWnd = nullptr;
		UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
	}
}

HWND Window::GetHWnd() const { return mHWnd; }

uint32_t Window::GetID() const { return mWndId; }

void Window::Show(const int cmdShow) const {
	ShowWindow(mHWnd, cmdShow);
	UpdateWindow(mHWnd);
}

LRESULT Window::WndProc(
	const HWND   hWnd, const UINT     msg,
	const WPARAM wParam, const LPARAM lParam
) {
	if (msg == WM_NCCREATE) {
		auto createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
		SetWindowLongPtrW(
			hWnd, GWLP_USERDATA,
			reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams)
		);
	}

	// これでウィンドウのポインタを取得できるらしい! やったね!
	// Window* self = reinterpret_cast<Window*>(
	// 	GetWindowLongPtrW(
	// 		hWnd, GWLP_USERDATA
	// 	)
	// );

	switch (msg) {
	case WM_CLOSE: {
		DestroyWindow(hWnd);
		return 0;
	}

	case WM_DESTROY: {
		PostQuitMessage(0);
		return 0;
	}

	case WM_PAINT: {
		InvalidateRect(hWnd, nullptr, FALSE);
		return 0;
	}
	case WM_SIZE: {
		InvalidateRect(hWnd, nullptr, FALSE);
		return 0;
	}

	// TODO: メッセージを他のサブシステムに横流しする
	default: ;
	}

	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

constexpr const wchar_t* Window::kClassName;
