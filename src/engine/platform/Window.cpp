#include "Window.h"

namespace Unnamed {
	Window::Window(const WindowId id, WindowDesc desc, const HWND hwnd) :
		mHwnd(hwnd),
		mDesc(std::move(desc)),
		mId(id) {
	}

	WindowId Window::GetId() const {
		return mId;
	}

	HWND Window::GetHwnd() const {
		return mHwnd;
	}

	WindowDesc Window::GetDesc() const {
		return mDesc;
	}

	WINDOW_MODE Window::GetMode() const {
		return mDesc.mode;
	}

	bool Window::ShouldClose() const {
		return mShouldClose;
	}

	bool Window::IsMinimized() const {
		return mMinimized;
	}

	std::optional<WindowResizeEvent> Window::ConsumeResizeEvent() {
		if (!mHasPendingResize) {
			return std::nullopt;
		}

		// 複数の WM_SIZE をまとめ、Engine 側では一度だけスワップチェーンを更新する
		mDesc.width  = mPendingResize.width;
		mDesc.height = mPendingResize.height;

		mHasPendingResize = false;
		return mPendingResize;
	}

	LRESULT Window::HandleMessage(
		const HWND   hwnd, const UINT     msg,
		const WPARAM wParam, const LPARAM lParam
	) {
		switch (msg) {
			case WM_CLOSE: {
				MarkCloseRequested();
				return 0;
			}

			case WM_DESTROY: {
				MarkCloseRequested();
				return 0;
			}

			case WM_SIZE: {
				const int32_t width  = LOWORD(lParam);
				const int32_t height = HIWORD(lParam);

				if (wParam == SIZE_MINIMIZED) {
					mMinimized = true;
				} else {
					mMinimized                  = false;
					mHasPendingResize           = true;
					mPendingResize.width        = width;
					mPendingResize.height       = height;
					mPendingResize.isLiveResize = mInLiveResize;
				}
				return 0;
			}
			case WM_ENTERSIZEMOVE: {
				mInLiveResize = true;
				return 0;
			}
			case WM_EXITSIZEMOVE: {
				mInLiveResize = false;
				return 0;
			}

			default: break;
		}

		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	void Window::SetMode(const WINDOW_MODE mode) {
		if (!mHwnd || mode == mDesc.mode) {
			return;
		}

		if (mode == WINDOW_MODE::WINDOWED) {
			ApplyWindowedMode();
		} else {
			ApplyBorderlessMode(mode);
		}
	}

	void Window::ToggleFullscreen() {
		SetMode(
			mDesc.mode == WINDOW_MODE::WINDOWED ?
				WINDOW_MODE::FULLSCREEN :
				WINDOW_MODE::WINDOWED
		);
	}

	void Window::CaptureWindowedPlacement() {
		if (mDesc.mode != WINDOW_MODE::WINDOWED || !mHwnd) {
			return;
		}

		mWindowedPlacement.length = sizeof(WINDOWPLACEMENT);
		if (!GetWindowPlacement(mHwnd, &mWindowedPlacement)) {
			return;
		}
		if (mWindowedPlacement.showCmd == SW_HIDE) {
			mWindowedPlacement.showCmd = SW_SHOWNORMAL;
		}

		mWindowedStyle   = static_cast<DWORD>(GetWindowLong(mHwnd, GWL_STYLE));
		mWindowedExStyle = static_cast<DWORD>(
			GetWindowLong(mHwnd, GWL_EXSTYLE));
		mHasWindowedPlacement = true;
	}

	void Window::ApplyWindowedMode() {
		DWORD style = mWindowedStyle != 0 ?
			              mWindowedStyle :
			              WS_OVERLAPPEDWINDOW;
		if (!mDesc.resizable) {
			style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
		}

		SetWindowLong(mHwnd, GWL_STYLE, static_cast<LONG>(style));
		SetWindowLong(mHwnd, GWL_EXSTYLE, static_cast<LONG>(mWindowedExStyle));

		if (mHasWindowedPlacement) {
			mWindowedPlacement.length = sizeof(WINDOWPLACEMENT);
			SetWindowPlacement(mHwnd, &mWindowedPlacement);
			SetWindowPos(
				mHwnd,
				HWND_NOTOPMOST,
				0,
				0,
				0,
				0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_FRAMECHANGED
			);
		} else {
			RECT rect{
				.left   = 0, .top = 0, .right = mDesc.width,
				.bottom = mDesc.height
			};
			AdjustWindowRectEx(&rect, style, FALSE, mWindowedExStyle);

			const HMONITOR hMonitor = MonitorFromWindow(
				mHwnd, MONITOR_DEFAULTTONEAREST
			);
			MONITORINFO mi = {.cbSize = sizeof(mi)};
			if (!GetMonitorInfoW(hMonitor, &mi)) {
				return;
			}

			const int32_t width  = rect.right - rect.left;
			const int32_t height = rect.bottom - rect.top;
			const int32_t posX   = mi.rcMonitor.left + (
				                     mi.rcMonitor.right - mi.rcMonitor.left -
				                     width
			                     ) / 2;
			const int32_t posY = mi.rcMonitor.top + (
				                     mi.rcMonitor.bottom - mi.rcMonitor.top -
				                     height
			                     ) / 2;

			SetWindowPos(
				mHwnd,
				HWND_NOTOPMOST,
				posX,
				posY,
				width,
				height,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED
			);
		}

		mDesc.mode = WINDOW_MODE::WINDOWED;
	}

	void Window::ApplyBorderlessMode(const WINDOW_MODE mode) {
		// 復帰時にユーザーの位置・サイズを戻せるよう、枠なし化の前に保存する
		CaptureWindowedPlacement();

		MONITORINFO mi = {.cbSize = sizeof(mi)};
		if (!GetMonitorInfoW(MonitorFromWindow(mHwnd, MONITOR_DEFAULTTONEAREST),
		                     &mi)) {
			return;
		}

		const DWORD style = static_cast<DWORD>(
			GetWindowLong(mHwnd, GWL_STYLE)
		);
		const DWORD exStyle = static_cast<DWORD>(
			GetWindowLong(mHwnd, GWL_EXSTYLE)
		);
		const DWORD borderlessStyle   = style & ~WS_OVERLAPPEDWINDOW | WS_POPUP;
		const DWORD borderlessExStyle = exStyle & ~(
			                                WS_EX_DLGMODALFRAME |
			                                WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE
			                                |
			                                WS_EX_STATICEDGE
		                                );

		SetWindowLong(mHwnd, GWL_STYLE, static_cast<LONG>(borderlessStyle));
		SetWindowLong(mHwnd, GWL_EXSTYLE, static_cast<LONG>(borderlessExStyle));
		SetWindowPos(
			mHwnd,
			HWND_TOP,
			mi.rcMonitor.left,
			mi.rcMonitor.top,
			mi.rcMonitor.right - mi.rcMonitor.left,
			mi.rcMonitor.bottom - mi.rcMonitor.top,
			SWP_NOOWNERZORDER | SWP_FRAMECHANGED
		);

		mDesc.mode = mode;
	}

	void Window::MarkCloseRequested() {
		mShouldClose = true;
	}
}
