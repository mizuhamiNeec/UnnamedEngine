#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace Unnamed {
	/// @brief Win32のメッセージを受け取るリスナーのインターフェース
	struct IWin32MsgListener {
		virtual bool OnWin32Message(HWND, UINT, WPARAM, LPARAM) = 0;
	};
}
