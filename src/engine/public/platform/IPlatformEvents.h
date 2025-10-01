#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace Unnamed {
	struct IWin32MsgListener;

	struct IPlatformEvents {
		virtual bool AddListener(IWin32MsgListener* listener) = 0;
		virtual void RemoveListener(IWin32MsgListener* listener) = 0;
		virtual bool DispatchMessage(HWND, UINT, WPARAM, LPARAM) = 0;
	};
}
