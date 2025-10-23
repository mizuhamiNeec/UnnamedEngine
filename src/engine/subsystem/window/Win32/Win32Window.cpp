#include <pch.h>

#include <dwmapi.h>

#include <engine/subsystem/window/Win32/Win32Window.h>
#include <engine/Window/WindowsUtils.h>

/// @brief 閉じるべきかどうかを取得します
/// @return 閉じるべきならtrue
bool Win32Window::ShouldClose() const {
	return mShouldClose;
}

/// @brief ウィンドウ情報を取得します
/// @return ウィンドウ情報構造体
IWindow::WindowInfo Win32Window::GetInfo() const {
	return mWindowInfo;
}

/// @brief ネイティブウィンドウハンドルを取得します
/// @return ネイティブウィンドウハンドル
void* Win32Window::GetNativeHandle() const {
	return mHWnd;
}

/// @brief ウィンドウを閉じるよう要求します
void Win32Window::RequestClose() {
	mShouldClose = true;
}

/// @brief ウィンドウタイトルを設定します
/// @param title ウィンドウタイトルの文字列ビュー
void Win32Window::SetTitle(const std::string_view title) {
	mWindowInfo.title = title;
	if (mHWnd) {
		const auto titleW = StrUtil::ToWString(std::string(title));
		SetWindowTextW(mHWnd, titleW.c_str());
	}
}

/// @brief ウィンドウのリサイズイベントを処理します
/// @param width 新しいクライアント幅
/// @param height 新しいクライアント高さ
void Win32Window::OnResize(uint32_t width, uint32_t height) {
	width;
	height;
}

/// @brief ウィンドウのアクティブ/非アクティブ状態を設定します
/// @param flag 非アクティブ状態ならtrue
void Win32Window::SetInactive(const bool flag) {
	mWindowInfo.bIsInactive = flag;
}

/// @brief デストラクタ
Win32Window::~Win32Window() {
	if (mHWnd) {
		DestroyWindow(mHWnd);
	}
}

/// @brief ウィンドウを作成します
/// @param hInstance インスタンスハンドル
/// @param className ウィンドウクラス名の文字列ビュー
/// @param hParent 親ウィンドウハンドル（親なしの場合はnullptr）
/// @return 作成成功ならtrue
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

/// @brief イマーシブダークモードを設定します
/// @param darkMode ダークモードを有効にする場合はtrue
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
