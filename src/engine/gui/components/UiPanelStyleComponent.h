#pragma once

#include "UiComponent.h"

#include "engine/gui/UiDrawCommand.h"

namespace Unnamed::Gui {
	class UiPanelStyleComponent final : public UiComponent {
	public:
		[[nodiscard]] std::string_view GetTypeName() const override {
			return "PanelStyle";
		}

		void                       SetBackgroundColor(const Color& color);
		[[nodiscard]] const Color& GetBackgroundColor() const;

		void                SetCornerRadius(float radius);
		[[nodiscard]] float GetCornerRadius() const;

		void                SetBorderThickness(float thickness);
		[[nodiscard]] float GetBorderThickness() const;

		void                       SetBorderColor(const Color& color);
		[[nodiscard]] const Color& GetBorderColor() const;

		void BuildDrawCommands(
			const UiWidget& owner, std::vector<UiDrawCommand>& out
		) const override;
		void Serialize(JsonWriter& writer) const override;
		void Deserialize(const JsonReader& reader) override;

	private:
		Color mBackgroundColor = {
			.r = 0.15f, .g = 0.15f, .b = 0.18f, .a = 1.0f
		};
		float mCornerRadius    = 4.0f;
		float mBorderThickness = 0.0f;
		Color mBorderColor     = {.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f};
	};
}
