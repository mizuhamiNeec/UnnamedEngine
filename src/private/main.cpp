#include <pch.h>
#include <Windows.h>

namespace {
	bool gWishClose = false;

	LRESULT CALLBACK WndProc(
		const HWND   hWnd,
		const UINT   uMsg,
		const WPARAM wParam,
		const LPARAM lParam
	) {
		switch (uMsg) {
		case WM_CLOSE:
			gWishClose = true;
			return 0;

		case WM_PAINT: {
			if (gWishClose) {
				// Shutdown Engine
				DestroyWindow(hWnd);
				gWishClose = false;
			}
			return 0;
		}

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		default: ;
		}

		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}

constexpr uint32_t kWindowWidth  = 1280;
constexpr uint32_t kWindowHeight = 720;

int WINAPI wWinMain(
	const HINSTANCE hInstance,
	HINSTANCE,
	const PWSTR,
	const int) {
	LeakChecker;

	WNDCLASS wc      = {};
	wc.lpfnWndProc   = WndProc;
	wc.hInstance     = hInstance;
	wc.lpszClassName = L"EngineWindowClass";
	RegisterClass(&wc);

	RECT wrc = {
		0, 0, static_cast<LONG>(kWindowWidth), static_cast<LONG>(kWindowHeight)
	};

	AdjustWindowRectEx(&wrc, WS_OVERLAPPEDWINDOW, FALSE, 0);

	const HWND hWnd = CreateWindowEx(
		0,
		wc.lpszClassName,
		L"Unnamed Window",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	MSG msg = {};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	UnregisterClassW(wc.lpszClassName, hInstance);

	return EXIT_SUCCESS;
}
