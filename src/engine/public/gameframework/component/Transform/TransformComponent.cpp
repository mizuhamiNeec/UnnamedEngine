#include <pch.h>

#include <engine/public/gameframework/component/Transform/TransformComponent.h>
#include <engine/public/gameframework/entity/base/BaseEntity.h>

#include <core/json/JsonReader.h>
#include <core/json/JsonWriter.h>

#include "engine/public/Debug/Debug.h"

namespace Unnamed {
	const Vec3& TransformComponent::Position() const noexcept {
		return mLocalPos;
	}

	const Quaternion& TransformComponent::Rotation() const noexcept {
		return mLocalRot;
	}

	const Vec3& TransformComponent::Scale() const noexcept {
		return mLocalScale;
	}

	TransformComponent* TransformComponent::Parent() const {
		return mParent;
	}

	const Mat4& TransformComponent::WorldMat() const noexcept {
		return mWorldMat;
	}

	void TransformComponent::SetPosition(const Vec3& newPosition) {
		mLocalPos = newPosition;
		MarkDirty();
	}

	void TransformComponent::SetRotation(const Quaternion& newRotation) {
		mLocalRot = newRotation;
		MarkDirty();
	}

	void TransformComponent::SetScale(const Vec3& newScale) {
		mLocalScale = newScale;
		MarkDirty();
	}

	void TransformComponent::SetParent(TransformComponent* newParent) {
		if (mParent == newParent) {
			Warning(
				GetComponentName(),
				"SetParent: 新しい親は現在の親と同じです。ロジックの確認をしてください。"
			);
			return;
		}

		// 前回の親から自分を削除
		if (mParent) {
			auto& children = mParent->mChildren;
			std::erase(children, this);
		}

		// 新しい親を設定
		mParent = newParent;
		if (mParent) {
			mParent->mChildren.emplace_back(this);
		}

		MarkDirty();
	}

	void TransformComponent::OnAttached() {
		Msg(GetComponentName(), "OnAttached: コンポーネントが取り付けられました。");
	}

	void TransformComponent::OnDetached() {
		Msg(GetComponentName(), "OnDetached: コンポーネントが取り外されました。");
	}

	void TransformComponent::PrePhysicsTick(float) {
	}

	void TransformComponent::OnTick(float) {
		if (!mIsDirty) {
			return; // ・・・・・・なにも!!! な゛かった・・・!!!!(ドンッ!!)
		}

		Mat4 S = Mat4::Scale(mLocalScale);
		Mat4 R = Mat4::FromQuaternion(mLocalRot);
		Mat4 T = Mat4::Translate(mLocalPos);

		const Mat4 localMat = S * R * T;

		if (mParent) {
			mWorldMat = localMat * mParent->WorldMat();
		} else {
			mWorldMat = localMat;
		}

		mIsDirty = false;
	}

	void TransformComponent::PostPhysicsTick(float) {
	}

	void TransformComponent::OnPreRender() const {
	}

	void TransformComponent::OnRender() const {
	}

	void TransformComponent::OnPostRender() const {
	}

	void TransformComponent::Serialize(JsonWriter& writer) const {
		if (mParent) {
			writer.Key("parentId");
			writer.Write(mParent->GetOwner()->GetId());
		}

		writer.Key("guid");
		writer.Write(GetId());

		writer.Key("pos");
		writer.Write(
			std::vector{mLocalPos.x, mLocalPos.y, mLocalPos.z}
		);

		writer.Key("rot");
		writer.Write(
			std::vector{mLocalRot.x, mLocalRot.y, mLocalRot.z, mLocalRot.w}
		);

		writer.Key("scale");
		writer.Write(
			std::vector{mLocalScale.x, mLocalScale.y, mLocalScale.z}
		);

		writer.Key("type");
		writer.Write(GetComponentName());
	}

	void TransformComponent::Deserialize(const JsonReader& reader) {
		if (
			auto p = reader.Read<std::vector<float>>("pos");
			p && p->size() == 3
		) {
			mLocalPos = {(*p)[0], (*p)[1], (*p)[2]};
		}

		if (
			auto r = reader.Read<std::vector<float>>("rot");
			r && r->size() == 4
		) {
			mLocalRot = {(*r)[0], (*r)[1], (*r)[2], (*r)[3]};
		}

		if (
			auto s = reader.Read<std::vector<float>>("scale");
			s && s->size() == 3
		) {
			mLocalScale = {(*s)[0], (*s)[1], (*s)[2]};
		}

		// if (
		// 	auto parentId = reader.Read<uint64_t>("parentId");
		// 	parentId && mOwner
		// ) {
		// 	auto* parentEntity = mOwner->GetWorld()->GetEntityById(*parentId);
		// 	if (parentEntity) {
		// 		auto* parentTransform =
		// 			parentEntity->GetComponent<TransformComponent>();
		// 		if (parentTransform) {
		// 			SetParent(parentTransform);
		// 		} else {
		// 			Warning(
		// 				GetComponentName(),
		// 				"Deserialize: 親の TransformComponent が見つかりません。"
		// 			);
		// 		}
		// 	} else {
		// 		Warning(
		// 			GetComponentName(),
		// 			"Deserialize: 親のエンティティが見つかりません。"
		// 		);
		// 	}
		// }
	}

	std::string_view TransformComponent::GetComponentName() const {
		return "Transform";
	}

	void TransformComponent::MarkDirty() {
		mIsDirty = true;
		for (auto* child : mChildren) {
			child->MarkDirty();
		}
	}
}
