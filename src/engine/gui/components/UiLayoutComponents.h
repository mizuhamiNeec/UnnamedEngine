#pragma once

#include "UiComponent.h"

namespace Unnamed::Gui {
	/// @brief LayoutPaddingは、layout内容領域の上下左右paddingをpixel単位で保持します
	struct LayoutPadding {
		float left   = 0.0f;
		float top    = 0.0f;
		float right  = 0.0f;
		float bottom = 0.0f;
	};

	/// @brief UiLinearLayoutComponentは、子UiWidgetを指定軸、間隔、alignmentに従って配置します
	class UiLinearLayoutComponent : public UiComponent {
	public:
		void SetPadding(const LayoutPadding& padding);
		[[nodiscard]] const LayoutPadding& GetPadding() const;

		void                SetSpacing(float spacing);
		[[nodiscard]] float GetSpacing() const;

		void Serialize(JsonWriter& writer) const override;
		[[nodiscard]] bool Deserialize(
			const JsonReader& reader, const UiDeserializeContext& context
		) override;
		void OnAfterLayout(UiWidget& owner) override;

	protected:
		UiLinearLayoutComponent() = default;
		virtual bool IsVertical() const = 0;

	private:
		void ApplyLayout(UiWidget& owner) const;

		LayoutPadding mPadding = {};
		float         mSpacing = 0.0f;
	};

	/// @brief UiVerticalLayoutComponentは、子UiWidgetを上から下へ間隔付きで配置します
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

	/// @brief UiHorizontalLayoutComponentは、子UiWidgetを左から右へ間隔付きで配置します
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
