#include "UiLayoutComponents.h"

#include <algorithm>

#include "engine/gui/UiSerializationHelpers.h"
#include "engine/gui/UiWidget.h"

namespace Unnamed::Gui {
	void UiLinearLayoutComponent::SetPadding(const LayoutPadding& padding) {
		mPadding = padding;
	}

	const LayoutPadding& UiLinearLayoutComponent::GetPadding() const {
		return mPadding;
	}

	void UiLinearLayoutComponent::SetSpacing(const float spacing) {
		mSpacing = spacing;
	}

	float UiLinearLayoutComponent::GetSpacing() const {
		return mSpacing;
	}

	void UiLinearLayoutComponent::Serialize(JsonWriter& writer) const {
		writer.Key("padding");
		WritePadding(writer, mPadding);
		writer.Key("spacing");
		writer.Write(mSpacing);
	}

	bool UiLinearLayoutComponent::Deserialize(
		const JsonReader& reader, const UiDeserializeContext& context
	) {
		(void)context;
		if (reader.Has("padding")) {
			mPadding = ReadPadding(reader["padding"].GetArray());
		}
		if (reader.Has("spacing")) {
			mSpacing = reader["spacing"].GetFloat();
		}
		return true;
	}

	void UiLinearLayoutComponent::OnAfterLayout(UiWidget& owner) {
		ApplyLayout(owner);
	}

	void UiLinearLayoutComponent::ApplyLayout(UiWidget& owner) const {
		const Rect& selfRect = owner.GetGlobalRect();

		const float contentWidth = std::max(
			0.0f,
			selfRect.width - mPadding.left - mPadding.right
		);
		const float contentHeight = std::max(
			0.0f,
			selfRect.height - mPadding.top - mPadding.bottom
		);

		/// @brief ChildInfoは、layout対象widget、固定extent、余剰領域を受け取る指定をchild一件分保持します
		struct ChildInfo {
			UiWidget* widget      = nullptr;
			float     fixedExtent = 0.0f;
			bool      expand      = false;
		};

		std::vector<ChildInfo> infos;
		float                  totalFixedExtent = 0.0f;
		int                    expandCount      = 0;

		for (const auto& childPtr : owner.GetChildren()) {
			UiWidget* child = childPtr.get();
			if (!child || !child->IsVisible()) {
				continue;
			}

			const UiSizePolicy       policy      = child->GetSizePolicy();
			const UiSizeConstraints& constraints = child->GetSizeConstraints();
			const bool               isVertical  = IsVertical();
			const bool               expand      = isVertical ?
				                    policy.vertical ==
				                    UiSizePolicyAxis::EXPAND :
				                    policy.horizontal ==
				                    UiSizePolicyAxis::EXPAND;

			float preferred = isVertical ?
				                  child->GetPreferredHeight() :
				                  child->GetPreferredWidth();
			if (preferred <= 0.0f) {
				preferred = isVertical ? 32.0f : 64.0f;
			}

			const float minValue = isVertical ?
				                       constraints.minHeight :
				                       constraints.minWidth;
			const float maxValue = isVertical ?
				                       constraints.maxHeight :
				                       constraints.maxWidth;
			const float clamped = std::clamp(preferred, minValue, maxValue);

			ChildInfo info   = {};
			info.widget      = child;
			info.expand      = expand;
			info.fixedExtent = expand ? 0.0f : clamped;
			totalFixedExtent += info.fixedExtent;
			if (expand) {
				++expandCount;
			}
			infos.emplace_back(info);
		}

		if (infos.empty()) {
			return;
		}

		const float totalSpacing =
			mSpacing * static_cast<float>(std::max(
				0, static_cast<int>(infos.size()) - 1));
		const float contentExtent = IsVertical() ? contentHeight : contentWidth;
		const float remaining     = std::max(
			0.0f,
			contentExtent - totalFixedExtent - totalSpacing
		);
		const float expandExtent = expandCount > 0 ?
			                           remaining / static_cast<float>(
				                           expandCount) :
			                           0.0f;

		float cursor = IsVertical() ? mPadding.top : mPadding.left;
		for (const ChildInfo& info : infos) {
			if (!info.widget) {
				continue;
			}

			const UiSizeConstraints& constraints = info.widget->
				GetSizeConstraints();
			float extent = info.fixedExtent;
			if (info.expand) {
				extent = expandExtent;
				extent = std::clamp(
					extent,
					IsVertical() ? constraints.minHeight : constraints.minWidth,
					IsVertical() ? constraints.maxHeight : constraints.maxWidth
				);
			}

			Rect childLocal = info.widget->GetLocalRect();
			if (IsVertical()) {
				childLocal.x      = mPadding.left;
				childLocal.y      = cursor;
				childLocal.width  = contentWidth;
				childLocal.height = extent;
			} else {
				childLocal.x      = cursor;
				childLocal.y      = mPadding.top;
				childLocal.width  = extent;
				childLocal.height = contentHeight;
			}

			info.widget->SetAnchors(
				{.minX = 0.0f, .minY = 0.0f, .maxX = 0.0f, .maxY = 0.0f}
			);
			info.widget->SetPivot({.x = 0.0f, .y = 0.0f});
			info.widget->SetLocalRect(childLocal);

			cursor += extent + mSpacing;
		}
	}
}
