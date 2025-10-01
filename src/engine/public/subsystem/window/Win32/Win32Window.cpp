#include <pch.h>

#include <dwmapi.h>

#include <engine/public/subsystem/window/Win32/Win32Window.h>
#include <engine/public/Window/WindowsUtils.h>

bool Win32Window::ShouldClose() const {
	return mShouldClose;
}

IWindow::WindowInfo Win32Window::GetInfo() const {
	return mWindowInfo;
}

void* Win32Window::GetNativeHandle() const {
	return mHWnd;
}

void Win32Window::RequestClose() {
	mShouldClose = true;
}

void Win32Window::SetTitle(const std::string_view title) {
	mWindowInfo.title = title;
	if (mHWnd) {
		const auto titleW = StrUtil::ToWString(std::string(title));
		SetWindowTextW(mHWnd, titleW.c_str());
	}
}

void Win32Window::OnResize(uint32_t width, uint32_t height) {
	width;
	height;
}

void Win32Window::SetInactive(const bool flag) {
	mWindowInfo.bIsInactive = flag;
}

Win32Window::~Win32Window() {
	if (mHWnd) {
		DestroyWindow(mHWnd);
	}
}

bool Win32Window::Create(
	HINSTANCE               hInstance,
	const std::string_view& className,
	HWND                    hParent
) {
	DWORD style = WS_OVERLAPPEDWINDOW;
	if (!mWindowInfo.bIsResizable) {
		style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
	}

	RECT rect = {
		0, 0,
		static_cast<LONG>(mWindowInfo.clWidth),
		static_cast<LONG>(mWindowInfo.clHeight)
	};

	AdjustWindowRect(&rect, style, FALSE);

	const auto winW = rect.right - rect.left;
	const auto winH = rect.bottom - rect.top;
	auto       x    = CW_USEDEFAULT;
	auto       y    = CW_USEDEFAULT;
	if (mWindowInfo.bCreateAtCenter) {
		RECT desktop;
		GetClientRect(GetDesktopWindow(), &desktop);
		x = (desktop.right - winW) / 2;
		y = (desktop.bottom - winH) / 2;
	}

	const auto titleW     = StrUtil::ToWString(mWindowInfo.title);
	const auto classNameW = StrUtil::ToWString(std::string(className));

	mHWnd = CreateWindowEx(
		0,
		classNameW.c_str(),
		titleW.c_str(),
		style,
		x, y,
		winW, winH,
		hParent,
		nullptr,
		hInstance,
		this
	);

	if (!mHWnd) {
		return false;
	}
	SetImmersiveDarkMode(WindowsUtils::IsSystemDarkTheme());
	ShowWindow(mHWnd, SW_SHOW);
	UpdateWindow(mHWnd);
	return true;
}

void Win32Window::SetImmersiveDarkMode(const bool darkMode) const {
	const BOOL    value = darkMode;
	const HRESULT hr    = DwmSetWindowAttribute(
		mHWnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
		&value, sizeof(value)
	);
	if (FAILED(hr)) {
		const std::string errorMessage = WindowsUtils::GetHresultMessage(hr);
		Error(
			"Win32Window",
			"Failed to set immersive dark mode. Error: {}",
			errorMessage
		);
	}
}
