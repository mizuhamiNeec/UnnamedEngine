#pragma once

#include <optional>
#include <vector>

#include "engine/unnamed/ui/UnnamedUIDrawList.h"
#include "engine/unnamed/ui/UITheme.h"

#include "../base/BaseComponent.h"

#include "engine/unnamed/ui/UIContext.h"
#include "core/filesystem/Path.h"
#include "core/filesystem/VirtualPath.h"
#include "engine/content/AssetReferenceValidationPolicy.h"

namespace Unnamed::UI {
	class UIFontAtlas;
}

namespace Unnamed {
	/// @brief NewUICanvasは、エンジン要素のルート配置、入力、および描画提出を管理します
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

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		void DrawInspectorImGui() override;
#endif

		[[nodiscard]] TICK_GROUP       GetTickGroup() const override;
		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;
		void                           Deserialize(const JsonReader& reader)
		override;
		[[nodiscard]] bool Deserialize(
			const JsonReader& reader, const SceneDeserializeContext& context
		) override;
		void Serialize(JsonWriter& writer) const override;

		/// @brief 前回のフレーム入力更新で生成されたUI描画コマンドを返します。
		[[nodiscard]] const std::vector<UI::UIDrawCommand>&
		GetDrawCommands() const;
		/// @brief 現在設定のAtlasをcacheから取得し、使用中として更新します。
		[[nodiscard]] UI::UIFontAtlas* ResolveFontAtlas(
			AssetManager& assetManager
		);

	private:
		[[nodiscard]] bool InitializeFontAtlas(
			AssetManager& assetManager,
			AssetReferenceValidationPolicy validationPolicy
		);
		[[nodiscard]] bool TryInitializeFontAtlas(
			const VirtualPath& fontPath, AssetManager& assetManager
		);

		UI::UIContext mContext;

		UI::UITheme                    mTheme = {};
		std::vector<UI::UIDrawCommand> mDrawCommands;
		std::optional<VirtualPath> mFontPath =
			VirtualPath::ParseContentReference("fonts/JetBrainsMono.ttf");
		Path mResolvedFontPath;
		UI::UIFontAtlas* mFontAtlas = nullptr;

		bool  mShowDebug   = false;
		bool  mEnableBloom = true;
		float mVolume      = 0.5f;
		float mPlayerSpeed = 3.0f;
		float mEnemySpeed  = 6.0f;
	};
}
