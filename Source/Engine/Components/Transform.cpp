#include "Transform.h"

#include "../ImGuiManager/ImGuiManager.h"

const Vec3& Components::Transform::GetPosition() const {
	return position_;
}

void Components::Transform::SetPosition(const Vec3& position) {
	position_ = position;
}

const Quaternion& Components::Transform::GetRotation() const {
	return rotation_;
}

void Components::Transform::SetRotation(const Quaternion& rotation) {
	rotation_ = rotation;
}

const Vec3& Components::Transform::GetScale() const {
	return scale_;
}

void Components::Transform::SetScale(const Vec3& scale) {
	scale_ = scale;
}

Mat4 Components::Transform::GetTransformMatrix() const {
	Mat4 scale = Mat4::Scale(scale_);
	Mat4 rotate = Mat4::RotateQuaternion(rotation_);
	Mat4 translate = Mat4::Translate(position_);

	return scale * rotate * translate;
}

//void Components::Transform::DrawImGui() {
//	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
//		ImGuiManager::DragVec3("Position", position_, 0.01f, "%.3f");
//
//		// 回転を取っておく
//		Vec3 rotate = rotation_.ToEulerDegrees();
//		if (ImGuiManager::DragVec3("Rotation", rotate, 0.01f, "%.3f")) {
//			rotation_ = Quaternion::EulerDegrees(rotate);
//		}
//
//		ImGuiManager::DragVec3("Scale", scale_, 0.01f, "%.3f");
//	}
//}
