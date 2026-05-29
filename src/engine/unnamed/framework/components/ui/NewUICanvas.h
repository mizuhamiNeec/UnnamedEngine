#pragma once

#include <vector>

#include "engine/unnamed/ui/UnnamedUIDrawList.h"
#include "engine/unnamed/ui/UITheme.h"

#include "../base/BaseComponent.h"

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
		UI::UITheme mTheme = {};
		std::vector<UI::UIDrawCommand> mDrawCommands;
	};
}
