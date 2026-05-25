#include "UiWidget.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include <engine/unnamed/subsystem/console/Log.h>

#include <core/math/Math.h>

#include "UiSerializationHelpers.h"
#include "components/UiButtonBehaviorComponent.h"
#include "components/UiDigitStripComponent.h"
#include "components/UiLayoutComponents.h"
#include "components/UiPanelStyleComponent.h"
#include "components/UiTextureComponent.h"
#include "components/UiTransformComponent.h"

namespace Unnamed::Gui {
	namespace {
		static constexpr std::string_view kChannel = "UiWidget";
	}

	DIRTY_FLAGS operator|(DIRTY_FLAGS a, DIRTY_FLAGS b) {
		return static_cast<DIRTY_FLAGS>(
			static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
		);
	}

	DIRTY_FLAGS operator|=(DIRTY_FLAGS& a, const DIRTY_FLAGS b) {
		return a = a | b;
	}

	bool HasFlag(const DIRTY_FLAGS flags, const DIRTY_FLAGS test) {
		return
			(static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
	}

	const char* ToString(const DIRTY_FLAGS e) {
		switch (e) {
			case DIRTY_FLAGS::NONE: return "NONE";
			case DIRTY_FLAGS::LAYOUT: return "LAYOUT";
			case DIRTY_FLAGS::STYLE: return "STYLE";
			case DIRTY_FLAGS::DRAW: return "DRAW";
			case DIRTY_FLAGS::ALL: return "ALL";
			default: return "unknown";
		}
	}

	UiWidget::UiWidget() {
		AddComponent(std::make_unique<UiTransformComponent>());
	}

	UiWidget::~UiWidget() {
		for (auto& component : mComponents) {
			if (component) {
				component->OnDetached(*this);
			}
		}
	}

	void UiWidget::AddComponent(std::unique_ptr<UiComponent> component) {
		if (!component) {
			return;
		}
		component->OnAttached(*this);
		mComponents.emplace_back(std::move(component));
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	UiComponent* UiWidget::GetComponentByTypeName(const std::string_view typeName
	) {
		for (auto& component : mComponents) {
			if (component && component->GetTypeName() == typeName) {
				return component.get();
			}
		}
		return nullptr;
	}

	const std::vector<std::unique_ptr<UiComponent>>& UiWidget::GetComponents(
	) const {
		return mComponents;
	}

	std::vector<std::unique_ptr<UiComponent>>& UiWidget::GetComponents() {
		return mComponents;
	}

	bool UiWidget::RemoveComponentAt(const size_t index) {
		if (index >= mComponents.size()) {
			return false;
		}
		const auto* component = mComponents[index].get();
		if (!component || component->GetTypeName() == "Transform") {
			return false;
		}

		mComponents[index]->OnDetached(*this);
		mComponents.erase(
			mComponents.begin() + static_cast<std::ptrdiff_t>(index)
		);
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
		return true;
	}

	bool UiWidget::MoveComponent(size_t fromIndex, size_t toIndex) {
		if (
			fromIndex >= mComponents.size() ||
			toIndex >= mComponents.size() ||
			fromIndex == toIndex
		) {
			return false;
		}

		const auto* component = mComponents[fromIndex].get();
		if (!component || component->GetTypeName() == "Transform") {
			return false;
		}
		if (toIndex == 0 && !mComponents.empty() &&
		    mComponents[0] &&
		    mComponents[0]->GetTypeName() == "Transform") {
			toIndex = 1;
		}

		auto moving = std::move(mComponents[fromIndex]);
		mComponents.erase(
			mComponents.begin() + static_cast<std::ptrdiff_t>(fromIndex)
		);
		if (fromIndex < toIndex) {
			--toIndex;
		}
		mComponents.insert(
			mComponents.begin() + static_cast<std::ptrdiff_t>(toIndex),
			std::move(moving)
		);
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
		return true;
	}

	std::unique_ptr<UiComponent> UiWidget::CreateComponentByTypeName(
		const std::string_view typeName
	) {
		if (typeName == "Transform") {
			return std::make_unique<UiTransformComponent>();
		}
		if (typeName == "VerticalLayout") {
			return std::make_unique<UiVerticalLayoutComponent>();
		}
		if (typeName == "HorizontalLayout") {
			return std::make_unique<UiHorizontalLayoutComponent>();
		}
		if (typeName == "PanelStyle") {
			return std::make_unique<UiPanelStyleComponent>();
		}
		if (typeName == "ButtonBehavior") {
			return std::make_unique<UiButtonBehaviorComponent>();
		}
		if (typeName == "Texture") {
			return std::make_unique<UiTextureComponent>();
		}
		if (typeName == "DigitStrip") {
			return std::make_unique<UiDigitStripComponent>();
		}
		return nullptr;
	}

	std::vector<std::string_view> UiWidget::GetRegisteredComponentTypeNames() {
		return {
			"Transform",
			"VerticalLayout",
			"HorizontalLayout",
			"PanelStyle",
			"ButtonBehavior",
			"Texture",
			"DigitStrip",
		};
	}

	void UiWidget::AddChild(std::unique_ptr<UiWidget> child) {
		if (!child) {
			Warning(kChannel, "nullptrを追加しようとしました");
			return;
		}

		child->mParent = this;
		mChildren.emplace_back(std::move(child));

		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	void UiWidget::RemoveChild(const UiWidget* child) {
		if (!child) {
			Warning(kChannel, "nullptrを削除しようとしました");
			return;
		}

		for (auto it = mChildren.begin(); it != mChildren.end(); ++it) {
			if (it->get() == child) {
				(*it)->mParent = nullptr;
				mChildren.erase(it);
				MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
				return;
			}
		}
	}

	std::unique_ptr<UiWidget> UiWidget::TakeChild(const UiWidget* child) {
		if (!child) {
			Warning(kChannel, "nullptrを取り出そうとしました");
			return nullptr;
		}

		for (auto it = mChildren.begin(); it != mChildren.end(); ++it) {
			if (it->get() == child) {
				(*it)->mParent = nullptr;
				auto result    = std::move(*it);
				mChildren.erase(it);
				MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
				return result;
			}
		}
		return nullptr;
	}

	void UiWidget::AddChildReference(UiWidget* child) {
		if (!child) {
			return;
		}
		child->mParent = this;
		mReferenceChildren.push_back(child);
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	void UiWidget::RemoveChildReference(const UiWidget* child) {
		if (!child) {
			return;
		}

		for (auto it = mReferenceChildren.begin(); it != mReferenceChildren.
		     end(); ++it) {
			if (*it == child) {
				if ((*it)->mParent == this) {
					(*it)->mParent = nullptr;
				}
				mReferenceChildren.erase(it);
				MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
				return;
			}
		}
	}

	const std::vector<UiWidget*>& UiWidget::GetReferenceChildren() const {
		return mReferenceChildren;
	}

	const std::vector<std::unique_ptr<UiWidget>>& UiWidget::GetChildren() const {
		return mChildren;
	}

	UiWidget* UiWidget::GetParent() const {
		return mParent;
	}

	void UiWidget::SetName(const std::string_view& name) {
		mName = name;
	}

	std::string_view UiWidget::GetName() const {
		return mName;
	}

	void UiWidget::SetLocalRect(const Rect& rect) {
		mLocalRect = rect;
		if (auto* transform = GetComponent<UiTransformComponent>()) {
			transform->SetRect(rect);
		}
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	const Rect& UiWidget::GetLocalRect() const {
		return mLocalRect;
	}

	const Rect& UiWidget::GetGlobalRect() const {
		return mGlobalRect;
	}

	void UiWidget::SetAnchors(const Anchors& anchors) {
		mAnchors = anchors;
		if (auto* transform = GetComponent<UiTransformComponent>()) {
			transform->SetAnchors(anchors);
		}
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	const Anchors& UiWidget::GetAnchors() const {
		return mAnchors;
	}

	void UiWidget::SetMargins(const Margins& margins) {
		mMargins = margins;
		if (auto* transform = GetComponent<UiTransformComponent>()) {
			transform->SetMargins(margins);
		}
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	const Margins& UiWidget::GetMargins() const {
		return mMargins;
	}

	void UiWidget::SetPivot(const Pivot& pivot) {
		mPivot = pivot;
		if (auto* transform = GetComponent<UiTransformComponent>()) {
			transform->SetPivot(pivot);
		}
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	const Pivot& UiWidget::GetPivot() const {
		return mPivot;
	}

	void UiWidget::SetVisible(const bool visible) {
		if (mVisible == visible) {
			return;
		}
		mVisible = visible;
		MarkDirty(DIRTY_FLAGS::DRAW);
	}

	bool UiWidget::IsVisible() const {
		return mVisible;
	}

	void UiWidget::SetEnabled(const bool enabled) {
		if (mEnabled == enabled) {
			return;
		}
		mEnabled = enabled;
	}

	bool UiWidget::IsEnabled() const {
		return mEnabled;
	}

	void UiWidget::MarkDirty(const DIRTY_FLAGS flags) {
		const DIRTY_FLAGS oldFlags = mDirtyFlags;
		mDirtyFlags                |= flags;
		if (mDirtyFlags != oldFlags) {
			OnDirtyFlagAdded(flags);
		}
	}

	DIRTY_FLAGS UiWidget::GetDirtyFlags() const {
		return mDirtyFlags;
	}

	void UiWidget::ClearDirtyFlags(const DIRTY_FLAGS flags) {
		uint32_t       raw   = static_cast<uint32_t>(mDirtyFlags);
		const uint32_t clear = static_cast<uint32_t>(flags);
		raw                  &= ~clear;
		mDirtyFlags          = static_cast<DIRTY_FLAGS>(raw);
	}

	void UiWidget::SetSizePolicy(
		const UiSizePolicyAxis horizontal,
		const UiSizePolicyAxis vertical
	) {
		mSizePolicy.horizontal = horizontal;
		mSizePolicy.vertical   = vertical;
		if (auto* transform = GetComponent<UiTransformComponent>()) {
			transform->SetSizePolicy(mSizePolicy);
		}
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	UiSizePolicy UiWidget::GetSizePolicy() const {
		return mSizePolicy;
	}

	void UiWidget::SetSizeConstraints(const UiSizeConstraints& constraints) {
		mSizeConstraints = constraints;
		if (auto* transform = GetComponent<UiTransformComponent>()) {
			transform->SetSizeConstraints(constraints);
		}
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	const UiSizeConstraints& UiWidget::GetSizeConstraints() const {
		return mSizeConstraints;
	}

	float UiWidget::GetPreferredWidth() const {
		return mLocalRect.width;
	}

	float UiWidget::GetPreferredHeight() const {
		return mLocalRect.height;
	}

	void UiWidget::Tick(const float deltaTime) {
		for (auto& component : mComponents) {
			if (component) {
				component->OnTick(*this, deltaTime);
			}
		}
	}

	void UiWidget::UpdateLayoutRecursive(const Rect& parentGlobalRect) {
		for (auto& component : mComponents) {
			if (component) {
				component->OnBeforeLayout(*this);
			}
		}

		if (HasFlag(mDirtyFlags, DIRTY_FLAGS::LAYOUT) ||
		    HasFlag(mDirtyFlags, DIRTY_FLAGS::ALL)) {
			UpdateLayout(parentGlobalRect);
			ClearDirtyFlags(DIRTY_FLAGS::LAYOUT);
		}

		for (auto& component : mComponents) {
			if (component) {
				component->OnAfterLayout(*this);
			}
		}

		for (auto& child : mChildren) {
			if (child) {
				child->UpdateLayoutRecursive(mGlobalRect);
			}
		}

		for (auto* refChild : mReferenceChildren) {
			if (refChild) {
				refChild->UpdateLayoutRecursive(mGlobalRect);
			}
		}
	}

	void UiWidget::BuildDrawCommands(std::vector<UiDrawCommand>& out) const {
		if (!IsVisible()) {
			return;
		}

		for (const auto& component : mComponents) {
			if (component) {
				component->BuildDrawCommands(*this, out);
			}
		}

		for (const auto& child : mChildren) {
			if (child) {
				child->BuildDrawCommands(out);
			}
		}

		for (const auto* refChild : mReferenceChildren) {
			if (refChild) {
				refChild->BuildDrawCommands(out);
			}
		}
	}

	void UiWidget::DebugDrawUi(const UiWidget* w) {
		if (!w) {
			return;
		}
#ifdef UNNAMED_WITH_EDITOR
		const auto& r  = w->GetGlobalRect();
		auto*       dl = ImGui::GetForegroundDrawList();

		dl->AddRect(
			ImVec2(+r.x, +r.y),
			ImVec2(+r.x + r.width, +r.y + r.height),
			IM_COL32(255, 0, 0, 255)
		);

		if (const auto* parent = w->GetParent()) {
			const auto& pr = parent->GetGlobalRect();
			dl->AddLine(
				ImVec2(+r.x + r.width * 0.5f, +r.y + r.height * 0.5f),
				ImVec2(+pr.x, +pr.y),
				IM_COL32(0, 255, 0, 255)
			);
		}

		for (const auto& child : w->GetChildren()) {
			if (child) {
				DebugDrawUi(child.get());
			}
		}

		for (const auto* refChild : w->mReferenceChildren) {
			if (refChild) {
				DebugDrawUi(refChild);
			}
		}
#endif
	}

	UiWidget* UiWidget::HitTest(const float x, const float y) {
		if (!mVisible || !mEnabled) {
			return nullptr;
		}

		for (auto it = mChildren.rbegin(); it != mChildren.rend(); ++it) {
			if (*it) {
				if (UiWidget* hit = (*it)->HitTest(x, y)) {
					return hit;
				}
			}
		}

		for (auto it = mReferenceChildren.rbegin(); it != mReferenceChildren.
		     rend(); ++it) {
			UiWidget* child = *it;
			if (child) {
				if (UiWidget* hit = child->HitTest(x, y)) {
					return hit;
				}
			}
		}

		const Rect& r      = mGlobalRect;
		const bool  inside =
			x >= r.x && x <= r.x + r.width &&
			y >= r.y && y <= r.y + r.height;

		return inside ? this : nullptr;
	}

	bool UiWidget::IsHovered() const {
		return mHovered;
	}

	bool UiWidget::IsPressed() const {
		return mPressed;
	}

	void UiWidget::OnMouseEnter() {
		mHovered = true;
		for (auto& component : mComponents) {
			if (component) {
				component->OnMouseEnter(*this);
			}
		}
	}

	void UiWidget::OnMouseLeave() {
		mHovered = false;
		for (auto& component : mComponents) {
			if (component) {
				component->OnMouseLeave(*this);
			}
		}
	}

	void UiWidget::OnMouseDown() {
		mPressed = true;
		for (auto& component : mComponents) {
			if (component) {
				component->OnMouseDown(*this);
			}
		}
	}

	void UiWidget::OnMouseUp() {
		mPressed = false;
		for (auto& component : mComponents) {
			if (component) {
				component->OnMouseUp(*this);
			}
		}
	}

	void UiWidget::OnClick() {
		for (auto& component : mComponents) {
			if (component) {
				component->OnClick(*this);
			}
		}
	}

	const char* UiWidget::GetTypeName() const {
		return "Widget";
	}

	void UiWidget::SaveToJson(JsonWriter& writer) const {
		writer.BeginObject();
		writer.Key("name");
		writer.Write(std::string(GetName()));
		writer.Key("visible");
		writer.Write(IsVisible());
		writer.Key("enabled");
		writer.Write(IsEnabled());

		writer.Key("components");
		writer.BeginArray();
		for (const auto& component : mComponents) {
			if (!component) {
				continue;
			}
			writer.BeginObject();
			writer.Key("type");
			writer.Write(std::string(component->GetTypeName()));
			writer.Key("data");
			writer.BeginObject();
			component->Serialize(writer);
			writer.EndObject();
			writer.EndObject();
		}
		writer.EndArray();

		OnSerialize(writer);

		writer.Key("children");
		writer.BeginArray();
		for (const auto& child : GetChildren()) {
			if (child) {
				child->SaveToJson(writer);
			}
		}
		writer.EndArray();

		writer.EndObject();
	}

	void UiWidget::LoadFromJson(const JsonReader& reader) {
		if (!reader.Valid()) {
			return;
		}

		if (reader.Has("name")) {
			SetName(reader["name"].GetString());
		}
		if (reader.Has("visible")) {
			SetVisible(reader["visible"].GetBool());
		}
		if (reader.Has("enabled")) {
			SetEnabled(reader["enabled"].GetBool());
		}

		if (reader.Has("components")) {
			const JsonReader components = reader["components"].GetArray();
			for (size_t i = 0; i < components.Size(); ++i) {
				const JsonReader componentNode = components[i];
				if (!componentNode.Valid()) {
					continue;
				}
				const JsonReader typeNode = componentNode["type"];
				if (!typeNode.Valid()) {
					continue;
				}
				const std::string typeName = typeNode.GetString();
				const JsonReader  dataNode = componentNode["data"];

				if (UiComponent* existing = GetComponentByTypeName(typeName)) {
					existing->Deserialize(dataNode);
					continue;
				}

				std::unique_ptr<UiComponent> component =
					CreateComponentByTypeName(typeName);
				if (!component) {
					Warning(
						kChannel,
						"Unknown UI component type '{}' in widget '{}'.",
						typeName,
						GetName()
					);
					continue;
				}
				component->Deserialize(dataNode);
				AddComponent(std::move(component));
			}
		}

		OnDeserialize(reader);
	}

	std::unique_ptr<UiWidget> UiWidget::CreateFromJson(const JsonReader& reader) {
		if (!reader.Valid()) {
			return nullptr;
		}

		auto widget = std::make_unique<UiWidget>();
		widget->LoadFromJson(reader);

		if (reader.Has("children")) {
			const JsonReader children = reader["children"].GetArray();
			for (size_t i = 0; i < children.Size(); ++i) {
				JsonReader childNode = children[i];
				auto       childWidget = CreateFromJson(childNode);
				if (childWidget) {
					widget->AddChild(std::move(childWidget));
				}
			}
		}

		return widget;
	}

	void UiWidget::UpdateLayout(const Rect& parentGlobalRect) {
		const float parentLeft   = parentGlobalRect.x;
		const float parentTop    = parentGlobalRect.y;
		const float parentRight  = parentGlobalRect.x + parentGlobalRect.width;
		const float parentBottom = parentGlobalRect.y + parentGlobalRect.height;

		const float anchorLeft = Math::Lerp(
			parentLeft, parentRight, mAnchors.minX
		);
		const float anchorTop = Math::Lerp(
			parentTop, parentBottom, mAnchors.minY
		);
		const float anchorRight = Math::Lerp(
			parentLeft, parentRight, mAnchors.maxX
		);
		const float anchorBottom = Math::Lerp(
			parentTop, parentBottom, mAnchors.maxY
		);

		float x;
		float y;
		float w;
		float h;

		if (mAnchors.minX == mAnchors.maxX) {
			x = anchorLeft + mLocalRect.x;
			w = mLocalRect.width;
		} else {
			const float available = anchorRight - anchorLeft;
			x                     = anchorLeft + mMargins.left;
			w                     = available - mMargins.left - mMargins.right;
		}

		if (mAnchors.minY == mAnchors.maxY) {
			y = anchorTop + mLocalRect.y;
			h = mLocalRect.height;
		} else {
			const float available = anchorBottom - anchorTop;
			y                     = anchorTop + mMargins.top;
			h                     = available - mMargins.top - mMargins.bottom;
		}

		float finalX = x;
		float finalY = y;

		if (mAnchors.minX == mAnchors.maxX) {
			finalX = x - w * mPivot.x;
		}
		if (mAnchors.minY == mAnchors.maxY) {
			finalY = y - h * mPivot.y;
		}

		mGlobalRect.x      = finalX;
		mGlobalRect.y      = finalY;
		mGlobalRect.width  = w;
		mGlobalRect.height = h;
	}

	void UiWidget::OnDirtyFlagAdded(const DIRTY_FLAGS flags) {
		if (HasFlag(flags, DIRTY_FLAGS::LAYOUT)) {
			for (auto& child : mChildren) {
				if (child) {
					child->MarkDirty(DIRTY_FLAGS::LAYOUT);
				}
			}
			for (auto* refChild : mReferenceChildren) {
				if (refChild) {
					refChild->MarkDirty(DIRTY_FLAGS::LAYOUT);
				}
			}
		}
	}

	void UiWidget::OnSerialize(JsonWriter& writer) const {
		(void)writer;
	}

	void UiWidget::OnDeserialize(const JsonReader& reader) {
		(void)reader;
	}
}
