#pragma once

#include <vector>

#include "engine/unnamed/ui/UnnamedUIDrawList.h"
#include "engine/unnamed/ui/UITheme.h"

#include "../base/BaseComponent.h"

#include "engine/unnamed/ui/UIContext.h"

namespace Unnamed {
	class NewUICanvas : public BaseComponent {
	public:
		//---------------------------------------------------------------------
		// BaseComponent
		//---------------------------------------------------------------------
		void OnAttached() override;
		void OnTick(float deltaTime) override;
		void OnFrameInputTick(float frameDeltaTime) override;
		void OnRenderTick(
			float renderDeltaTime, float interpolationAlpha
		) override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		[[nodiscard]] TICK_GROUP       GetTickGroup() const override;
		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;
		void                           Deserialize(const JsonReader& reader)
		override;
		void Serialize(JsonWriter& writer) const override;

		/// @brief 前回のフレーム入力更新で生成されたUI描画コマンドを返します。
		[[nodiscard]] const std::vector<UI::UIDrawCommand>&
		GetDrawCommands() const;

	private:
		UI::UIContext mContext;

		UI::UITheme                    mTheme = {};
		std::vector<UI::UIDrawCommand> mDrawCommands;

		bool  mShowDebug   = false;
		bool  mEnableBloom = true;
		float mVolume      = 0.5f;
		float mPlayerSpeed = 3.0f;
		float mEnemySpeed  = 6.0f;
	};
}
