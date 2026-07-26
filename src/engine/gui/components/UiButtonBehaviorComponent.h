#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "UiComponent.h"

#include "engine/gui/UiDrawCommand.h"

namespace Unnamed::Gui {
	/// @brief UiButtonBehaviorComponentは、UiWidgetのhover・press・click状態を入力から更新します
	class UiButtonBehaviorComponent final : public UiComponent {
	public:
		[[nodiscard]] std::string_view GetTypeName() const override {
			return "ButtonBehavior";
		}

		void                           SetText(const std::string_view& text);
		[[nodiscard]] std::string_view GetText() const;

		void SetOnClick(const std::function<void()>& callback);
		void SetColors(
			const Color& normal, const Color& hovered, const Color& pressed
		);
		[[nodiscard]] const Color& GetNormalColor() const;
		[[nodiscard]] const Color& GetHoveredColor() const;
		[[nodiscard]] const Color& GetPressedColor() const;
		void                       SetBorderColor(const Color& color);
		[[nodiscard]] const Color& GetBorderColor() const;
		void                       SetTextColor(const Color& color);
		[[nodiscard]] const Color& GetTextColor() const;
		void                       SetCornerRadius(float radius);
		[[nodiscard]] float        GetCornerRadius() const;
		void                       SetFontSize(float size);
		[[nodiscard]] float        GetFontSize() const;

		void BuildDrawCommands(
			const UiWidget& owner, std::vector<UiDrawCommand>& out
		) const override;
		void OnClick(UiWidget& owner) override;
		void Serialize(JsonWriter& writer) const override;
		[[nodiscard]] bool Deserialize(
			const JsonReader& reader, const UiDeserializeContext& context
		) override;

	private:
		std::string           mText;
		std::function<void()> mOnClick;

		Color mColorNormal  = {.r = 0.25f, .g = 0.25f, .b = 0.30f, .a = 1.0f};
		Color mColorHovered = {.r = 0.32f, .g = 0.32f, .b = 0.38f, .a = 1.0f};
		Color mColorPressed = {.r = 0.18f, .g = 0.18f, .b = 0.25f, .a = 1.0f};
		Color mBorderColor  = {.r = 0.05f, .g = 0.05f, .b = 0.07f, .a = 1.0f};
		Color mTextColor    = {.r = 1.00f, .g = 1.00f, .b = 1.00f, .a = 1.0f};

		float mCornerRadius = 4.0f;
		float mFontSize     = 16.0f;
	};
}
