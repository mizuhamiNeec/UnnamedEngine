#include "SpeedBoostAreaComponent.h"

#include <engine/Components/ColliderComponent/AABBCollider.h>
#include <engine/Debug/DebugDraw.h>
#include <engine/Entity/Entity.h>
#include <engine/ImGui/ImGuiWidgets.h>
#include <game/components/checkpoint/CheckpointManager.h>
#include <game/components/player/MovementComponent.h>

SpeedBoostAreaComponent::SpeedBoostAreaComponent(
	const float boostMultiplier,
	const float durationSec
)
	: mBoostMultiplier(boostMultiplier),
	  mDurationSec(durationSec) {}

void SpeedBoostAreaComponent::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	mCollider = owner.GetComponent<AABBCollider>();
}

void SpeedBoostAreaComponent::Update(float) {
	if (!mCollider) { return; }

	CheckPlayerCollision();

	const Unnamed::AABB worldAABB = mCollider->GetWorldAABB();
	DebugDraw::DrawBox(
		worldAABB.Center(),
		Quaternion::identity,
		worldAABB.Size(),
		Vec4(0.2f, 1.0f, 0.3f, 1.0f)
	);
}

void SpeedBoostAreaComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader(
		"SpeedBoostAreaComponent", ImGuiTreeNodeFlags_DefaultOpen
	)) {
		ImGui::DragFloat(
			"Boost Multiplier",
			&mBoostMultiplier,
			0.01f,
			1.0f,
			5.0f,
			"%.2f"
		);
		ImGui::DragFloat(
			"Duration (sec)",
			&mDurationSec,
			0.05f,
			0.1f,
			20.0f,
			"%.2f"
		);
	}
#endif
}

void SpeedBoostAreaComponent::SetBoostMultiplier(const float multiplier) {
	mBoostMultiplier = multiplier;
}

void SpeedBoostAreaComponent::SetDurationSec(const float durationSec) {
	mDurationSec = durationSec;
}

float SpeedBoostAreaComponent::GetBoostMultiplier() const noexcept {
	return mBoostMultiplier;
}

float SpeedBoostAreaComponent::GetDurationSec() const noexcept {
	return mDurationSec;
}

void SpeedBoostAreaComponent::CheckPlayerCollision() {
	Entity* player = CheckpointManager::GetPlayer();
	if (!player) { return; }

	auto* playerCollider = player->GetComponent<AABBCollider>();
	auto* movement       = player->GetComponent<MovementComponent>();
	if (!playerCollider || !movement) { return; }

	const Unnamed::AABB playerWorldAABB = playerCollider->GetWorldAABB();
	const Unnamed::AABB areaWorldAABB   = mCollider->GetWorldAABB();

	const bool isInside =
		playerWorldAABB.min.x <= areaWorldAABB.max.x &&
		playerWorldAABB.max.x >= areaWorldAABB.min.x &&
		playerWorldAABB.min.y <= areaWorldAABB.max.y &&
		playerWorldAABB.max.y >= areaWorldAABB.min.y &&
		playerWorldAABB.min.z <= areaWorldAABB.max.z &&
		playerWorldAABB.max.z >= areaWorldAABB.min.z;

	if (isInside && !mWasInside) { ApplyBoost(*movement); }

	mWasInside = isInside;
}

void SpeedBoostAreaComponent::ApplyBoost(MovementComponent& movement) const {
	movement.ApplySpeedBoost(mBoostMultiplier, mDurationSec);
}

