#include "UiRoot.h"

#include <engine/unnamed/subsystem/console/Log.h>

#include <engine/gui/UiWidget.h>

namespace Unnamed::Gui {
	static constexpr std::string_view kChannel = "UiRoot";

	UiRoot::UiRoot() {
		mRootWidget = std::make_unique<UiWidget>();
		mRootRect   = {
			.x     = 0.0f, .y         = 0.0f,
			.width = 1920.0f, .height = 1080.0f
		};
		mRootWidget->SetLocalRect(
			{
				.x     = 0.0f, .y                 = 0.0f,
				.width = mRootRect.width, .height = mRootRect.height
			}
		);
	}

	void UiRoot::SetRootRect(const Rect& rect) {
		mRootRect = rect;
		mRootWidget->SetLocalRect(
			{
				.x     = 0.0f, .y                 = 0.0f,
				.width = mRootRect.width, .height = mRootRect.height
			}
		);
		mRootWidget->MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	void UiRoot::SetRootSize(const float width, const float height) {
		mRootRect.width  = width;
		mRootRect.height = height;
		mRootWidget->SetLocalRect(
			{
				.x     = 0.0f, .y                 = 0.0f,
				.width = mRootRect.width, .height = mRootRect.height
			}
		);
		mRootWidget->MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	void UiRoot::AddChild(std::unique_ptr<UiWidget> child) const {
		if (!child) {
			Warning(kChannel, "nullptrを追加しようとしました");
			return;
		}
		mRootWidget->AddChild(std::move(child));
	}

	std::unique_ptr<UiWidget> UiRoot::TakeChild(const UiWidget* child) const {
		return mRootWidget->TakeChild(child);
	}

	void UiRoot::Tick(const float deltaTime) const {
		mRootWidget->Tick(deltaTime);
	}

	void UiRoot::UpdateLayout() const {
		mRootWidget->UpdateLayoutRecursive(mRootRect);
	}

	void UiRoot::BuildDrawCommands(
		std::vector<UiDrawCommand>& out
	) const {
		mRootWidget->BuildDrawCommands(out);
	}

	UiWidget* UiRoot::GetRootWidget() const {
		return mRootWidget.get();
	}

	void UiRoot::ProcessMouse(
		const float mouseX, const float  mouseY,
		const bool  leftDown, const bool leftPressed, const bool leftReleased
	) {
		UiWidget* hitWidget = nullptr;
		if (mRootWidget) {
			hitWidget = mRootWidget->HitTest(mouseX, mouseY);
		}

		// Hover 状態の更新
		if (hitWidget != mHoveredWidget) {
			if (mHoveredWidget) {
				mHoveredWidget->OnMouseLeave();
			}
			mHoveredWidget = hitWidget;
			if (mHoveredWidget) {
				mHoveredWidget->OnMouseEnter();
			}
		}

		// マウス押下の開始
		if (leftPressed && hitWidget) {
			mPressedWidget = hitWidget;
			mPressedWidget->OnMouseDown();
		}

		// マウスボタンが離された
		if (leftReleased) {
			if (mPressedWidget) {
				mPressedWidget->OnMouseUp();

				// 押し始めたウィジェットの上で離されたら Click
				if (mPressedWidget == hitWidget) {
					mPressedWidget->OnClick();
				}
			}
			mPressedWidget = nullptr;
		}

		// もしドラッグ中などでボタンが押されっぱなしで
		// ウィジェットの外に出ても特別な処理がしたければここに追加
		(void)leftDown;
	}
}
