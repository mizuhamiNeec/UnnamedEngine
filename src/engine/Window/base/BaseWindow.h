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

/// ウィンドウ情報構造体
struct WindowInfo {
	std::string title     = "Unnamed Window";
	uint32_t    width     = 300;
	uint32_t    height    = 400;
	DWORD       style     = WS_OVERLAPPEDWINDOW;
	DWORD       exStyle   = 0;
	HINSTANCE   hInstance = nullptr;
	std::string className = "Unnamed WindowClass";
};

/// @brief ベースウィンドウクラス
class BaseWindow {
public:
	using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;

	/// @brief デストラクタ
	virtual ~BaseWindow() {
		if (mHWnd) {
			SetWindowLongPtr(mHWnd, GWLP_USERDATA,
			                 reinterpret_cast<LONG_PTR>(nullptr));
			DestroyWindow(mHWnd);
			mHWnd = nullptr;
		}
	}

	/// @brief ウィンドウを作成する
	/// @param info ウィンドウ情報
	/// @return 成功したらtrueを返す
	virtual bool Create(WindowInfo info) = 0;

	/// @brief メッセージを処理する
	/// @return 続行するならtrueを返す
	virtual bool ProcessMessage() = 0;

	/// @brief リサイズコールバックを設定する
	/// @param callback コールバック関数
	virtual void SetResizeCallback(ResizeCallback callback) = 0;

	/// @brief ウィンドウハンドルを取得する
	/// @return ウィンドウハンドル
	[[nodiscard]] HWND GetWindowHandle() const {
		return mHWnd;
	}

	/// @brief クライアント幅を取得する
	/// @return クライアント幅
	[[nodiscard]] uint32_t GetClientWidth() const {
		return mInfo.width;
	}

	/// @brief クライアント高さを取得する
	/// @return クライアント高さ
	[[nodiscard]] uint32_t GetClientHeight() const {
		return mInfo.height;
	}

protected:
	static LRESULT StaticWindowProc(const HWND   hWnd, const UINT     msg,
	                                const WPARAM wParam, const LPARAM lParam) {
		BaseWindow* pThis;
		if (msg == WM_NCCREATE) {
			auto pCreate = reinterpret_cast<LPCREATESTRUCT>(lParam);
			pThis        = static_cast<BaseWindow*>(pCreate->lpCreateParams);
			SetWindowLongPtr(hWnd, GWLP_USERDATA,
			                 reinterpret_cast<LONG_PTR>(pThis));
			pThis->mHWnd = hWnd;
		} else {
			pThis = reinterpret_cast<BaseWindow*>(GetWindowLongPtr(
				hWnd, GWLP_USERDATA));
		}
		if (pThis) {
			return pThis->WindowProc(hWnd, msg, wParam, lParam);
		}
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}

	virtual LRESULT WindowProc(HWND   hWnd, UINT msg, WPARAM wParam,
	                           LPARAM lParam) = 0;

	HWND           mHWnd = nullptr;
	WindowInfo     mInfo;
	ResizeCallback mResizeCallback;
};
