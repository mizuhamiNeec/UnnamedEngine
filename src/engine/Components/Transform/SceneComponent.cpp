
#include "engine/Components/Transform/SceneComponent.h"

#include "engine/Entity/Entity.h"
#include "engine/ImGui/ImGuiManager.h"

void SceneComponent::Update([[maybe_unused]] float deltaTime) {
}

const Vec3& SceneComponent::GetLocalPos() const {
	return position_;
}

void SceneComponent::SetLocalPos(const Vec3& newPosition) {
	position_ = newPosition;
	MarkDirty();
}

const Quaternion& SceneComponent::GetLocalRot() const {
	return rotation_;
}

void SceneComponent::SetLocalRot(const Quaternion& newRotation) {
	rotation_ = newRotation.Normalized();
	MarkDirty();
}

const Vec3& SceneComponent::GetLocalScale() const {
	return scale_;
}

void SceneComponent::SetLocalScale(const Vec3& newScale) {
	scale_ = newScale;
	MarkDirty();
}

Vec3 SceneComponent::GetWorldPos() const {
	if (mOwner && mOwner->GetParent()) {
		if (const SceneComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			// 親のワールド情報を取得
			const Vec3       parentPos = parentTransform->GetWorldPos();
			const Quaternion parentRot = parentTransform->GetWorldRot();

			// ローカル位置を親の回転で変換
			const Vec3 rotatedPos = parentRot * position_;

			// 親の位置を加算
			return parentPos + rotatedPos;
		}
	}
	return position_;
}

Quaternion SceneComponent::GetWorldRot() const {
	if (mOwner && mOwner->GetParent()) {
		if (const SceneComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			return parentTransform->GetWorldRot() * rotation_;
		}
	}
	return rotation_;
}

Vec3 SceneComponent::GetWorldScale() const {
	if (mOwner->GetParent()) {
		if (const SceneComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			return parentTransform->GetWorldScale() * scale_;
		}
	}
	return scale_;
}

void SceneComponent::SetWorldPos(const Vec3& newPosition) {
	if (mOwner->GetParent()) {
		if (const SceneComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			// 親の逆行列を用いてワールド座標をローカル座標に変換
			const Mat4 parentWorldMat = parentTransform->GetWorldMat();
			const Mat4 parentInvMat = parentWorldMat.Inverse();
			position_ = parentInvMat.TransformPoint(newPosition);
			MarkDirty();
			return;
		}
	}
	// 親がいない場合はそのまま設定
	position_ = newPosition;
	MarkDirty();
}

void SceneComponent::SetWorldRot(const Quaternion& newRotation) {
	if (mOwner->GetParent()) {
		if (const SceneComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			// 親の回転を取得
			Quaternion parentRot = parentTransform->GetWorldRot();
			// 親の回転の逆クォータニオンをかけることでローカル回転を取得
			const Quaternion invParentRot = parentRot.Inverse();
			rotation_                     = invParentRot * newRotation;
			MarkDirty();
			return;
		}
	}
	rotation_ = newRotation; // 親がいない場合はそのまま設定
	MarkDirty();
}

void SceneComponent::SetWorldScale(const Vec3& newScale) {
	if (mOwner->GetParent()) {
		if (const SceneComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			const Vec3 parentScale = parentTransform->GetWorldScale();
			scale_                 = newScale / parentScale; // 親スケールとの比率を設定
			MarkDirty();
			return;
		}
	}
	scale_ = newScale; // 親がいない場合そのまま設定
	MarkDirty();
}

const Mat4& SceneComponent::GetLocalMat() const {
	if (isDirty_) {
		RecalculateMat();
	}
	return localMat_;
}

Mat4 SceneComponent::GetWorldMat() const {
	if (mOwner && mOwner->GetParent()) {
		if (const SceneComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			return parentTransform->GetWorldMat() * GetLocalMat();
		}
	}
	return GetLocalMat();
}

void SceneComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	ImGuiManager::EditTransform(*this, 0.1f);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 行列を更新する必要がある場合に呼び出します。
//-----------------------------------------------------------------------------
void SceneComponent::MarkDirty() const {
	isDirty_ = true;
	for (const auto& child : mOwner->GetChildren()) {
		if (const auto* childTransform = child->GetTransform()) {
			childTransform->MarkDirty();
		}
	}
}

Entity* SceneComponent::GetOwner() const {
	return mOwner;
}

bool SceneComponent::IsDirty() const {
	return isDirty_;
}

void SceneComponent::SetIsDirty(const bool newIsDirty) const {
	isDirty_ = newIsDirty;
}

void SceneComponent::RecalculateMat() const {
	const Mat4 S = Mat4::Scale(scale_);
	const Mat4 R = Mat4::FromQuaternion(rotation_);
	const Mat4 T = Mat4::Translate(position_);

	localMat_ = R * S * T;
	isDirty_  = false;
}
