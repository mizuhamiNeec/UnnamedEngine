#include "PlatformEventsImpl.h"

#include <engine/IWin32MsgListener.h>

namespace Unnamed {
	/// @brief リスナーを追加します
	/// @param listener 追加するリスナーのポインタ
	/// @return 追加に成功した場合はtrue、失敗した場合はfalse
	bool PlatformEventsImpl::AddListener(IWin32MsgListener* listener) {
		if (!listener) {
			return false;
		}
		mListeners.push_back(listener);
		return true;
	}

	/// @brief リスナーを削除します
	/// @param listener 削除するリスナーのポインタ
	void PlatformEventsImpl::RemoveListener(IWin32MsgListener* listener) {
		std::erase(mListeners, listener);
	}

	/// @brief Win32メッセージをディスパッチします
	/// @param hWnd ウィンドウハンドル
	/// @param uMsg メッセージID
	/// @param wParam メッセージのwParam
	/// @param lParam メッセージのlParam
	/// @return メッセージが処理された場合はtrue、そうでなければfalse
	bool PlatformEventsImpl::DispatchMessage(
		const HWND   hWnd,
		const UINT   uMsg,
		const WPARAM wParam,
		const LPARAM lParam
	) {
		for (auto* listener : mListeners) {
			if (listener->OnWin32Message(hWnd, uMsg, wParam, lParam)) {
				return true;
			}
		}
		return false;
	}
}
