#pragma once
#include <memory>

#include "UiRoot.h"
#include "UiScreen.h"

namespace Unnamed::Gui {
	/// @brief UiScreenStackは、画面の積み重ね順に入力対象と描画順を管理します
	class UiScreenStack {
	public:
		explicit UiScreenStack(UiRoot* uiRoot);

		void PushScreen(std::shared_ptr<UiScreen> screen);

		void PopScreen();
		void Clear();

		void Tick(float deltaTime);

		[[nodiscard]] const std::vector<std::shared_ptr<UiScreen>>&
		GetScreens() const {
			return mScreens;
		}

		[[nodiscard]] UiRoot* GetUiRoot() const {
			return mUiRoot;
		}

	private:
		UiRoot*                                mUiRoot = nullptr;
		std::vector<std::shared_ptr<UiScreen>> mScreens;
	};
}
