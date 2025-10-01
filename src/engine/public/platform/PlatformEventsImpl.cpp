#include "PlatformEventsImpl.h"

#include <engine/public/IWin32MsgListener.h>

namespace Unnamed {
	bool PlatformEventsImpl::AddListener(IWin32MsgListener* listener) {
		if (!listener) {
			return false;
		}
		mListeners.push_back(listener);
		return true;
	}

	void PlatformEventsImpl::RemoveListener(IWin32MsgListener* listener) {
		std::erase(mListeners, listener);
	}

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
