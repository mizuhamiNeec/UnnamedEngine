#pragma once
#include <cstdint>
#include <optional>
#include <string>

#include <Windows.h>

#include "engine/EngineConfig.h"

namespace Unnamed {
	struct WindowId {
		uint32_t value = 0;

		friend bool operator==(const WindowId a, const WindowId b) {
			return a.value == b.value;
		}

		friend bool operator!=(const WindowId a, const WindowId b) {
			return !(a == b);
		}
	};

	struct WindowDesc {
		std::string title     = "Unnamed Window";
		int32_t     width     = 1280;
		int32_t     height    = 720;
		WINDOW_MODE mode      = WINDOW_MODE::WINDOWED;
		bool        resizable = true;
		bool        visible   = true;
	};

	struct WindowResizeEvent {
		int32_t width        = 0;
		int32_t height       = 0;
		bool    isLiveResize = false;
	};

	class Window final {
	public:
		Window(WindowId id, WindowDesc desc, HWND hwnd);

		/// @brief ウィンドウのIDを取得します。
		[[nodiscard]] WindowId GetId() const;

		/// @brief ウィンドウのHWNDを取得します。
		[[nodiscard]] HWND GetHwnd() const;

		/// @brief ウィンドウの説明を取得します。
		[[nodiscard]] WindowDesc GetDesc() const;

		/// @brief 現在のウィンドウモードを取得します。
		[[nodiscard]] WINDOW_MODE GetMode() const;

		/// @brief ウィンドウが閉じるべきかどうかを返します。WindowManagerがこれを見て実際の破棄を行います。
		[[nodiscard]] bool ShouldClose() const;

		/// @brief ウィンドウが最小化されているかどうかを返します。
		[[nodiscard]] bool IsMinimized() const;

		/// @brief 保留中のリサイズイベントを消費します。リサイズイベントがない場合はstd::nulloptを返します。
		std::optional<WindowResizeEvent> ConsumeResizeEvent();

		/// @brief ウィンドウメッセージを処理します。WindowManagerから呼び出されます。
		LRESULT HandleMessage(
			HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
		);

		/// @brief 指定したウィンドウモードに変更します。
		void SetMode(WINDOW_MODE mode);

		/// @brief フルスクリーンとウィンドウモードを切り替えます。
		void ToggleFullscreen();

	private:
		/// @brief 現在のウィンドウ位置とスタイルを復帰用に保存します。
		void CaptureWindowedPlacement();

		/// @brief ウィンドウモードへ戻します。
		void ApplyWindowedMode();

		/// @brief モニタ全体を覆うボーダーレス表示へ変更します。
		void ApplyBorderlessMode(WINDOW_MODE mode);

		/// @brief ウィンドウを閉じるようにマークします。実際の破棄はWindowManagerが行います。
		void MarkCloseRequested();

		HWND              mHwnd          = nullptr;
		WindowDesc        mDesc          = {};
		WindowResizeEvent mPendingResize = {};
		WindowId          mId            = {};
		WINDOWPLACEMENT   mWindowedPlacement = {.length = sizeof(WINDOWPLACEMENT)};
		DWORD             mWindowedStyle     = 0;
		DWORD             mWindowedExStyle   = 0;

		bool mShouldClose      = false;
		bool mMinimized        = false;
		bool mHasPendingResize = false;
		bool mInLiveResize     = false;
		bool mHasWindowedPlacement = false;
	};
}
