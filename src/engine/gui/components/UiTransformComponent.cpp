#include "UiTransformComponent.h"

#include "engine/gui/UiSerializationHelpers.h"

namespace Unnamed::Gui {
	void UiTransformComponent::OnAttached(UiWidget& owner) {
		SyncFromWidget(owner);
		mNeedsApply = true;
	}

	void UiTransformComponent::OnBeforeLayout(UiWidget& owner) {
		if (!mNeedsApply) {
			return;
		}

		owner.SetLocalRect(mRect);
		owner.SetAnchors(mAnchors);
		owner.SetMargins(mMargins);
		owner.SetPivot(mPivot);
		owner.SetSizePolicy(mSizePolicy.horizontal, mSizePolicy.vertical);
		owner.SetSizeConstraints(mSizeConstraints);
		mNeedsApply = false;
	}

	void UiTransformComponent::Serialize(JsonWriter& writer) const {
		writer.Key("rect");
		WriteRect(writer, mRect);

		writer.Key("anchors");
		WriteAnchors(writer, mAnchors);

		writer.Key("margins");
		WriteMargins(writer, mMargins);

		writer.Key("pivot");
		WritePivot(writer, mPivot);

		writer.Key("sizePolicy");
		WriteSizePolicy(writer, mSizePolicy);

		writer.Key("constraints");
		WriteConstraints(writer, mSizeConstraints);
	}

	bool UiTransformComponent::Deserialize(
		const JsonReader& reader, const UiDeserializeContext& context
	) {
		(void)context;
		if (reader.Has("rect")) {
			mRect = ReadRect(reader["rect"].GetArray());
		}
		if (reader.Has("anchors")) {
			mAnchors = ReadAnchors(reader["anchors"].GetArray());
		}
		if (reader.Has("margins")) {
			mMargins = ReadMargins(reader["margins"].GetArray());
		}
		if (reader.Has("pivot")) {
			mPivot = ReadPivot(reader["pivot"].GetArray());
		}
		if (reader.Has("sizePolicy")) {
			mSizePolicy = ReadSizePolicy(reader["sizePolicy"].GetArray());
		}
		if (reader.Has("constraints")) {
			mSizeConstraints = ReadConstraints(reader["constraints"]);
		}
		mNeedsApply = true;
		return true;
	}

	void UiTransformComponent::SyncFromWidget(const UiWidget& owner) {
		mRect            = owner.GetLocalRect();
		mAnchors         = owner.GetAnchors();
		mMargins         = owner.GetMargins();
		mPivot           = owner.GetPivot();
		mSizePolicy      = owner.GetSizePolicy();
		mSizeConstraints = owner.GetSizeConstraints();
	}

	void UiTransformComponent::SetRect(const Rect& rect) {
		mRect       = rect;
		mNeedsApply = true;
	}

	void UiTransformComponent::SetAnchors(const Anchors& anchors) {
		mAnchors    = anchors;
		mNeedsApply = true;
	}

	void UiTransformComponent::SetMargins(const Margins& margins) {
		mMargins    = margins;
		mNeedsApply = true;
	}

	void UiTransformComponent::SetPivot(const Pivot& pivot) {
		mPivot      = pivot;
		mNeedsApply = true;
	}

	void UiTransformComponent::SetSizePolicy(const UiSizePolicy sizePolicy) {
		mSizePolicy = sizePolicy;
		mNeedsApply = true;
	}

	void UiTransformComponent::SetSizeConstraints(
		const UiSizeConstraints constraints
	) {
		mSizeConstraints = constraints;
		mNeedsApply      = true;
	}

	const Rect& UiTransformComponent::GetRect() const {
		return mRect;
	}

	const Anchors& UiTransformComponent::GetAnchors() const {
		return mAnchors;
	}

	const Margins& UiTransformComponent::GetMargins() const {
		return mMargins;
	}

	const Pivot& UiTransformComponent::GetPivot() const {
		return mPivot;
	}

	UiSizePolicy UiTransformComponent::GetSizePolicy() const {
		return mSizePolicy;
	}

	const UiSizeConstraints& UiTransformComponent::GetSizeConstraints() const {
		return mSizeConstraints;
	}
}
