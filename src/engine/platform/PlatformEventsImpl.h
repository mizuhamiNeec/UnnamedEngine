#pragma once
#include <vector>

#include "IPlatformEvents.h"

namespace Unnamed {
	class PlatformEventsImpl : public IPlatformEvents {
	public:
		bool AddListener(IWin32MsgListener* listener) override;

		void RemoveListener(IWin32MsgListener* listener) override;

		bool DispatchMessage(
			HWND   hWnd,
			UINT   uMsg,
			WPARAM wParam,
			LPARAM lParam
		) override;

	private:
		std::vector<IWin32MsgListener*> mListeners;
	};
}
