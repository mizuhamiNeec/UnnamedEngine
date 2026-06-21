#include "UiScreenStack.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Gui {
	static constexpr std::string_view kChannel = "UiScreen";

	UiScreenStack::UiScreenStack(UiRoot* uiRoot) : mUiRoot(uiRoot) {
	}

	void UiScreenStack::PushScreen(std::shared_ptr<UiScreen> screen) {
		if (!screen || !mUiRoot)
			return;

		const UiDocument* doc = screen->GetDocument();
		if (!doc)
			return;

		UiWidget* documentRoot = doc->GetRootWidget();
		if (!documentRoot) {
			Warning(kChannel, "Document has no root widget!");
			return;
		}

		mUiRoot->GetRootWidget()->AddChildReference(documentRoot);

		screen->OnShow();
		mScreens.emplace_back(std::move(screen));

		DevMsg(kChannel, "PushScreen: now {} screens\n", mScreens.size());
	}

	void UiScreenStack::PopScreen() {
		if (mScreens.empty()) {
			return;
		}

		const auto screen = mScreens.back();

		// ★ 追加: UiRootから参照を外す
		if (screen && mUiRoot && mUiRoot->GetRootWidget()) {
			if (const auto* doc = screen->GetDocument()) {
				if (const auto* docRoot = doc->GetRootWidget()) {
					mUiRoot->GetRootWidget()->RemoveChildReference(docRoot);
				}
			}
		}

		mScreens.pop_back();
		if (screen) {
			screen->OnHide();
		}

		DevMsg(kChannel, "PopScreen: now {} screens\n", mScreens.size());
	}

	void UiScreenStack::Clear() {
		// ★ 追加: すべてのスクリーンについて、UiRootから参照を外す
		if (mUiRoot && mUiRoot->GetRootWidget()) {
			for (auto& screen : mScreens) {
				if (screen) {
					if (const auto* doc = screen->GetDocument()) {
						if (const auto* docRoot = doc->GetRootWidget()) {
							mUiRoot->GetRootWidget()->RemoveChildReference(
								docRoot
							);
						}
					}
					screen->OnHide();
				}
			}

			// 注意: 元のコードにあった while(!GetChildren().empty()) ループは
			// 「所有している子」を消すものなので、スクリーンスタックの用途（参照使用）とは
			// 異なりますが、念のため残すか、参照のみなら削除しても構いません。
		}

		mScreens.clear();

		DevMsg(kChannel, "Clear: all screens removed\n");
	}

	void UiScreenStack::Tick(const float deltaTime) {
		for (auto& screen : mScreens) {
			if (screen) {
				screen->OnUpdate(deltaTime);
			}
		}
	}
}
