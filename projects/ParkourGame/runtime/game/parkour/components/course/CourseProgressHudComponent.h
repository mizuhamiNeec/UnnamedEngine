#pragma once

#include <string>
#include <string_view>

#include "core/math/Vec2.h"

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;

	namespace Gui {
		class UiWidget;
		class UiTextureComponent;
		class UiTransformComponent;
	}

	class CourseProgressComponent;
	class UiCanvasComponent;

	/// @brief CourseProgress のスナップショットを UiCanvas に反映する HUD コンポーネントです。
	class CourseProgressHudComponent final : public BaseComponent {
	public:
		/// @brief コンポーネントを初期化します。
		void OnAttached() override;
		/// @brief プレイ中の HUD を更新します。
		void OnTick(float deltaTime) override;
		/// @brief エディタ中の HUD プレビューを更新します。
		void OnEditorTick(float deltaTime) override;

		/// @brief コンポーネントの stableName を返します。
		[[nodiscard]] std::string_view GetStableName() const override;
		/// @brief コンポーネントの表示名を返します。
		[[nodiscard]] std::string_view GetComponentName() const override;
		[[nodiscard]] uint32_t GetIcon() const override;

#ifdef _DEBUG
		/// @brief インスペクタ UI を描画します。
		void DrawInspectorImGui() override;
#endif

		/// @brief JSON から設定を読み込みます。
		void Deserialize(const JsonReader& reader) override;
		/// @brief 設定を JSON に書き出します。
		void Serialize(JsonWriter& writer) const override;

	private:
		/// @brief HUD 反映処理を実行します。
		void TickHud();
		/// @brief CourseProgress と UiCanvas の参照を解決します。
		void ResolveBindings();
		/// @brief 名前でウィジェットを再帰検索します。
		[[nodiscard]] static Gui::UiWidget* FindWidgetByNameRecursive(
			Gui::UiWidget* root,
			std::string_view widgetName
		);
		/// @brief 画面サイズを決定します。
		[[nodiscard]] bool ResolveViewportSize(Vec2& outViewportSizePx) const;
		/// @brief 追従スプライトの描画状態を更新します。
		void UpdateGuideWidget(
			Gui::UiWidget*            widget,
			Gui::UiTransformComponent* transform,
			Gui::UiTextureComponent*  texture,
			const Vec2&               centerPx,
			float                     sizePx,
			float                     alpha,
			float                     rotationRad,
			bool                      visible
		) const;

		std::string mCourseId = "default";
		bool        mRequireCourseHudEnabled = true;
		float       mScreenClampMarginPx = 100.0f;
		float       mPinSizePx           = 42.0f;
		float       mArrowSizePx         = 34.0f;
		std::string mPinWidgetName       = "NextCheckpointPin";
		std::string mArrowWidgetName     = "NextCheckpointArrow";
		std::string mPinTexturePath      = "textures/ping.png";
		std::string mArrowTexturePath    = "textures/arrow.png";

		// 所有しない参照です。毎Tick再解決するため raw pointer を使用します。
		CourseProgressComponent* mCourseProgress = nullptr;
		UiCanvasComponent*       mUiCanvas       = nullptr;
	};
}
