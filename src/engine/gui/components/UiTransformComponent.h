#pragma once

#include "UiComponent.h"

#include "engine/gui/Rect.h"
#include "engine/gui/UiWidget.h"

namespace Unnamed::Gui {
	class UiTransformComponent final : public UiComponent {
	public:
		[[nodiscard]] std::string_view GetTypeName() const override {
			return "Transform";
		}

		void OnAttached(UiWidget& owner) override;
		void OnBeforeLayout(UiWidget& owner) override;

		void Serialize(JsonWriter& writer) const override;
		[[nodiscard]] bool Deserialize(
			const JsonReader& reader, const UiDeserializeContext& context
		) override;

		void SyncFromWidget(const UiWidget& owner);

		void SetRect(const Rect& rect);
		void SetAnchors(const Anchors& anchors);
		void SetMargins(const Margins& margins);
		void SetPivot(const Pivot& pivot);
		void SetSizePolicy(UiSizePolicy sizePolicy);
		void SetSizeConstraints(UiSizeConstraints constraints);

		[[nodiscard]] const Rect&              GetRect() const;
		[[nodiscard]] const Anchors&           GetAnchors() const;
		[[nodiscard]] const Margins&           GetMargins() const;
		[[nodiscard]] const Pivot&             GetPivot() const;
		[[nodiscard]] UiSizePolicy             GetSizePolicy() const;
		[[nodiscard]] const UiSizeConstraints& GetSizeConstraints() const;

	private:
		Rect              mRect            = {};
		Anchors           mAnchors         = {};
		Margins           mMargins         = {};
		Pivot             mPivot           = {};
		UiSizePolicy      mSizePolicy      = {};
		UiSizeConstraints mSizeConstraints = {};
		bool              mNeedsApply      = true;
	};
}
