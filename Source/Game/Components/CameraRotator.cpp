#include "CameraRotator.h"

#include "Camera/CameraManager.h"
#include "Components/CameraComponent.h"
#include "Components/TransformComponent.h"
#include "Entity/Base/Entity.h"
#include "Input/InputSystem.h"
#include "Lib/Console/ConVarManager.h"
#include "Lib/Math/Vector/Vec3.h"
#include "Lib/Timer/EngineTimer.h"

CameraRotator::~CameraRotator() {}

void CameraRotator::OnAttach(Entity& owner) {
	Component::OnAttach(owner);

	transform_ = owner_->GetTransform();
}

void CameraRotator::Update([[maybe_unused]] float deltaTime) {
	Vec2 delta = InputSystem::GetMouseDelta();
	// 回転
	float sensitivity = ConVarManager::GetConVar("sensitivity")->GetValueAsFloat();
	float m_pitch = 0.022f;
	float m_yaw = 0.022f;
	float min = -89.0f;
	float max = 89.0f;

	rot_.y += delta.y * sensitivity * m_pitch * Math::deg2Rad;
	rot_.x += delta.x * sensitivity * m_yaw * Math::deg2Rad;

	rot_.y = std::clamp(rot_.y, min * Math::deg2Rad, max * Math::deg2Rad);

	transform_->SetWorldRot(Quaternion::Euler(Vec3::up * rot_.x + Vec3::right * rot_.y));
}

void CameraRotator::DrawInspectorImGui() {}
