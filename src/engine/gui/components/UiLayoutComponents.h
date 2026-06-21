#pragma once

#include "UiComponent.h"

namespace Unnamed::Gui {
	struct LayoutPadding {
		float left   = 0.0f;
		float top    = 0.0f;
		float right  = 0.0f;
		float bottom = 0.0f;
	};

	class UiLinearLayoutComponent : public UiComponent {
	public:
		void SetPadding(const LayoutPadding& padding);
		[[nodiscard]] const LayoutPadding& GetPadding() const;

		void                SetSpacing(float spacing);
		[[nodiscard]] float GetSpacing() const;

		void Serialize(JsonWriter& writer) const override;
		void Deserialize(const JsonReader& reader) override;
		void OnAfterLayout(UiWidget& owner) override;

	protected:
		UiLinearLayoutComponent() = default;
		virtual bool IsVertical() const = 0;

	private:
		void ApplyLayout(UiWidget& owner) const;

		LayoutPadding mPadding = {};
		float         mSpacing = 0.0f;
	};

	class UiVerticalLayoutComponent final : public UiLinearLayoutComponent {
	public:
		[[nodiscard]] std::string_view GetTypeName() const override {
			return "VerticalLayout";
		}

	private:
		[[nodiscard]] bool IsVertical() const override {
			return true;
		}
	};

	class UiHorizontalLayoutComponent final : public UiLinearLayoutComponent {
	public:
		[[nodiscard]] std::string_view GetTypeName() const override {
			return "HorizontalLayout";
		}

	private:
		[[nodiscard]] bool IsVertical() const override {
			return false;
		}
	};
}
