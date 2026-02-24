#include "JumpPadComponent.h"

#include <engine/Components/ColliderComponent/AABBCollider.h>
#include <engine/Debug/DebugDraw.h>
#include <engine/Entity/Entity.h>
#include <engine/ImGui/ImGuiWidgets.h>
#include <game/components/checkpoint/CheckpointManager.h>
#include <game/components/player/MovementComponent.h>

#include "engine/Engine.h"
#include "engine/ResourceSystem/Audio/AudioManager.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	class Engine;
}

JumpPadComponent::JumpPadComponent(const float boostVelocityHu)
	: mBoostVelocityHu(boostVelocityHu) {}

void JumpPadComponent::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	mCollider = owner.GetComponent<AABBCollider>();

	if (auto* engine = ServiceLocator::Get<Unnamed::Engine>()) {
		auto* audioManager = engine->GetAudioManagerInstance();
		if (audioManager) {
			mJumpPadSound = audioManager->GetAudio(
				"./content/parkour/sounds/se/Jumppad.wav"
			);
			mJumpPadSound->SetVolume(0.125f);
		}
	}
}

void JumpPadComponent::Update(float) {
	if (!mCollider) { return; }

	CheckPlayerCollision();

	const Unnamed::AABB worldAABB = mCollider->GetWorldAABB();
	DebugDraw::DrawBox(
		worldAABB.Center(),
		Quaternion::identity,
		worldAABB.Size(),
		Vec4(0.2f, 0.8f, 1.0f, 1.0f)
	);
}

void JumpPadComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader(
		"JumpPadComponent", ImGuiTreeNodeFlags_DefaultOpen
	)) {
		ImGui::DragFloat(
			"Boost Velocity (HU/s)",
			&mBoostVelocityHu,
			0.1f,
			0.0f,
			5000.0f,
			"%.1f"
		);
	}
#endif
}

void JumpPadComponent::SetBoostVelocityHu(const float boostVelocityHu) {
	mBoostVelocityHu = boostVelocityHu;
}

float JumpPadComponent::GetBoostVelocityHu() const noexcept {
	return mBoostVelocityHu;
}

void JumpPadComponent::CheckPlayerCollision() {
	Entity* player = CheckpointManager::GetPlayer();
	if (!player) { return; }

	auto* playerCollider = player->GetComponent<AABBCollider>();
	auto* movement       = player->GetComponent<MovementComponent>();
	if (!playerCollider || !movement) { return; }

	const Unnamed::AABB playerWorldAABB = playerCollider->GetWorldAABB();
	const Unnamed::AABB padWorldAABB    = mCollider->GetWorldAABB();

	const bool isInside =
		playerWorldAABB.min.x <= padWorldAABB.max.x &&
		playerWorldAABB.max.x >= padWorldAABB.min.x &&
		playerWorldAABB.min.y <= padWorldAABB.max.y &&
		playerWorldAABB.max.y >= padWorldAABB.min.y &&
		playerWorldAABB.min.z <= padWorldAABB.max.z &&
		playerWorldAABB.max.z >= padWorldAABB.min.z;

	if (isInside && !mWasInside) { ApplyBoost(*movement); }

	mWasInside = isInside;
}

void JumpPadComponent::ApplyBoost(MovementComponent& movement) const {
	auto& data                = movement.GetData();
	data.isGrounded           = false; // ジャンプパッドに乗った瞬間は地面にいないとみなす
	data.state                = MOVEMENT_STATE::AIR; // 空中状態に遷移
	data.wasGroundedLastFrame = false; // ジャンプパッドの効果で空中にいるので、前フレームも地面にいなかったことにする

	Vec3 velocity = movement.GetVelocity();
	velocity.y    = Math::HtoM(mBoostVelocityHu);
	movement.SetVelocity(velocity);

	if (mJumpPadSound) { mJumpPadSound->Play(); }
}
