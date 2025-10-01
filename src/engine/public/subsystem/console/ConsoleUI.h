#pragma once
#include <runtime/core/math/Vec4.h>

namespace Unnamed {
	class ConsoleSystem;

	constexpr Vec4 kConTextColor        = {0.71f, 0.71f, 0.72f, 1.0f};
	constexpr Vec4 kConTextColorDev     = {0.25f, 0.71f, 0.25f, 1.0f};
	constexpr Vec4 kConTextColorWarn    = {0.93f, 0.79f, 0.09f, 1.0f};
	constexpr Vec4 kConTextColorError   = {0.71f, 0.25f, 0.25f, 1.0f};
	constexpr Vec4 kConTextColorFatal   = {0.71f, 0.0f, 0.0f, 1.0f};
	constexpr Vec4 kConTextColorExec    = {0.8f, 1.0f, 1.0f, 1.0f};
	constexpr Vec4 kConTextColorWait    = {0.93f, 0.79f, 0.09f, 1.0f};
	constexpr Vec4 kConTextColorSuccess = {0.48f, 0.76f, 0.26f, 1.0f};

	//-------------------------------------------------------------------------
	// Purpose: 新しいコンソールシステムを操作するUI。ImGuiで描画されます 
	//-------------------------------------------------------------------------
	class ConsoleUI {
	public:
		explicit ConsoleUI(ConsoleSystem* consoleSystem);

		void Show() const;

	private:
		static void PushLogTextColor(const struct ConsoleLogText& buffer);

		ConsoleSystem* mConsoleSystem;

		bool bIsImGuiInitialized = false;
	};
}
