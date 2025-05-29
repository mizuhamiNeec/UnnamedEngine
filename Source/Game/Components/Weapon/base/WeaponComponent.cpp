#include "WeaponComponent.h"

#include "Camera/CameraManager.h"
#include "Components/Camera/CameraComponent.h"
#include "Lib/Math/Vector/Vec3.h"
#include "Physics/PhysicsEngine.h"

//-----------------------------------------------------------------------------
// 近接攻撃モジュール
//-----------------------------------------------------------------------------
MeleeModule::~MeleeModule() {
}

void MeleeModule::Execute([[maybe_unused]] Entity& entity) {
	auto                  cam     = CameraManager::GetActiveCamera();
	[[maybe_unused]] Vec3 forward = cam->GetViewMat().GetForward();
	[[maybe_unused]] Vec3 eyePos  = cam->GetViewMat().GetTranslate();
}

void MeleeModule::Update([[maybe_unused]] const float& deltaTime) {
}

void MeleeModule::DrawInspectorImGui() {
}

//-----------------------------------------------------------------------------
// WeaponComponent
//-----------------------------------------------------------------------------

WeaponComponent::~WeaponComponent() {
}

void WeaponComponent::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
}

void WeaponComponent::Update([[maybe_unused]] float deltaTime) {
}

void WeaponComponent::Render(ID3D12GraphicsCommandList* commandList) {
	Component::Render(commandList);
}

void WeaponComponent::DrawInspectorImGui() {
}

Entity* WeaponComponent::GetOwner() const {
	return Component::GetOwner();
}

void WeaponComponent::LoadFromJson([[maybe_unused]] const std::string& jsonPath) {
}
