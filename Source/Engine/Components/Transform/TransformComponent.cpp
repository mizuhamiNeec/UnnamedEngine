#include "TransformComponent.h"

#include <Debug/Debug.h>

#include <Entity/Base/Entity.h>

#include <ImGuiManager/ImGuiManager.h>

void TransformComponent::Update([[maybe_unused]] float deltaTime) {
}

const Vec3& TransformComponent::GetLocalPos() const {
	return position_;
}

void TransformComponent::SetLocalPos(const Vec3& newPosition) {
	position_ = newPosition;
	MarkDirty();
}

const Quaternion& TransformComponent::GetLocalRot() const {
	return rotation_;
}

void TransformComponent::SetLocalRot(const Quaternion& newRotation) {
	rotation_ = newRotation.Normalized();
	MarkDirty();
}

const Vec3& TransformComponent::GetLocalScale() const {
	return scale_;
}

void TransformComponent::SetLocalScale(const Vec3& newScale) {
	scale_ = newScale;
	MarkDirty();
}

Vec3 TransformComponent::GetWorldPos() const {
	if (mOwner && mOwner->GetParent()) {
		if (const TransformComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			// 親のワールド情報を取得
			const Vec3       parentPos = parentTransform->GetWorldPos();
			const Quaternion parentRot = parentTransform->GetWorldRot();

			// ローカル位置を親の回転で変換
			Vec3 rotatedPos = parentRot * position_;

			// 親の位置を加算
			return parentPos + rotatedPos;
		}
	}
	return position_;
}

Quaternion TransformComponent::GetWorldRot() const {
	if (mOwner && mOwner->GetParent()) {
		if (const TransformComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			return parentTransform->GetWorldRot() * rotation_;
		}
	}
	return rotation_;
}

Vec3 TransformComponent::GetWorldScale() const {
	if (mOwner->GetParent()) {
		if (const TransformComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			return parentTransform->GetWorldScale() * scale_;
		}
	}
	return scale_;
}

void TransformComponent::SetWorldPos(const Vec3& newPosition) {
	if (mOwner->GetParent()) {
		if (const TransformComponent* parentTransform = mOwner->GetParent()->
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

void TransformComponent::SetWorldRot(const Quaternion& newRotation) {
	if (mOwner->GetParent()) {
		if (const TransformComponent* parentTransform = mOwner->GetParent()->
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

void TransformComponent::SetWorldScale(const Vec3& newScale) {
	if (mOwner->GetParent()) {
		if (const TransformComponent* parentTransform = mOwner->GetParent()->
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

const Mat4& TransformComponent::GetLocalMat() const {
	if (isDirty_) {
		RecalculateMat();
	}
	return localMat_;
}

Mat4 TransformComponent::GetWorldMat() const {
	if (mOwner && mOwner->GetParent()) {
		if (const TransformComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			return parentTransform->GetWorldMat() * GetLocalMat();
		}
	}
	return GetLocalMat();
}

void TransformComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	ImGuiManager::EditTransform(*this, 0.1f);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 行列を更新する必要がある場合に呼び出します。
//-----------------------------------------------------------------------------
void TransformComponent::MarkDirty() const {
	isDirty_ = true;
	for (const auto& child : mOwner->GetChildren()) {
		if (const auto* childTransform = child->GetTransform()) {
			childTransform->MarkDirty();
		}
	}
}

Entity* TransformComponent::GetOwner() const {
	return mOwner;
}

bool TransformComponent::IsDirty() const {
	return isDirty_;
}

void TransformComponent::SetIsDirty(const bool newIsDirty) const {
	isDirty_ = newIsDirty;
}

void TransformComponent::RecalculateMat() const {
	const Mat4 S = Mat4::Scale(scale_);
	const Mat4 R = Mat4::FromQuaternion(rotation_);
	const Mat4 T = Mat4::Translate(position_);

	localMat_ = R * S * T;
	isDirty_  = false;
}
