#include "CheckpointComponent.h"

#include <format>

#include <engine/Components/ColliderComponent/AABBCollider.h>
#include <engine/Debug/DebugDraw.h>
#include <engine/Entity/Entity.h>
#include <engine/ImGui/ImGuiWidgets.h>
#include <game/components/checkpoint/CheckpointManager.h>

#include "engine/OldConsole/Console.h"

CheckpointComponent::CheckpointComponent(
	const int order, const Vec3& respawnPosition
)
	: mOrder(order),
	  mRespawnPosition(respawnPosition) {}

CheckpointComponent::~CheckpointComponent() = default;

void CheckpointComponent::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	mCollider = owner.GetComponent<AABBCollider>();

	// CheckpointManagerに登録
	CheckpointManager::RegisterCheckpoint(this);
}

void CheckpointComponent::OnDetach() {
	CheckpointManager::UnregisterCheckpoint(this);
	Component::OnDetach();
}

void CheckpointComponent::Update(float) {
	if (!mCollider) { return; }

	CheckPlayerCollision();

	// デバッグ表示
	const Vec4          color     = mActivated ? Vec4::green : Vec4::yellow;
	const Unnamed::AABB worldAABB = mCollider->GetWorldAABB();
	const Vec3          center    = (worldAABB.min + worldAABB.max) * 0.5f;
	const Vec3          size      = worldAABB.Size();

	DebugDraw::DrawBox(
		center,
		Quaternion::identity,
		size,
		color
	);

	// リスポーン位置を表示
	DebugDraw::DrawSphere(
		mRespawnPosition, Quaternion::identity, 0.5f, Vec4::cyan
	);
}

void CheckpointComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader(
		"CheckpointComponent", ImGuiTreeNodeFlags_DefaultOpen
	)) {
		ImGui::Text("Order: %d", mOrder);
		ImGui::Text("Activated: %s", mActivated ? "Yes" : "No");
		ImGui::SeparatorText("Respawn Position");
		ImGuiWidgets::DragVec3(
			"Position##Checkpoint", mRespawnPosition, Vec3::zero, 0.1f, "%.2f"
		);

		if (ImGui::Button("Activate")) { Activate(); }
		ImGui::SameLine();
		if (ImGui::Button("Reset")) { Reset(); }
	}
#endif
}

bool CheckpointComponent::IsActivated() const { return mActivated; }

void CheckpointComponent::Activate() {
	if (!mActivated) {
		mActivated = true;
		CheckpointManager::OnCheckpointActivated(this);
	}
}

void CheckpointComponent::Reset() {
	mActivated = false;
	mWasInside = false;
}

int CheckpointComponent::GetOrder() const { return mOrder; }

Vec3 CheckpointComponent::GetRespawnPosition() const {
	return mRespawnPosition;
}

void CheckpointComponent::SetRespawnPosition(const Vec3& position) {
	mRespawnPosition = position;
}

void CheckpointComponent::CheckPlayerCollision() {
	// プレイヤーエンティティを取得
	Entity* player = CheckpointManager::GetPlayer();
	if (!player) { return; }

	// プレイヤーのAABBコライダーを取得
	auto* playerCollider = player->GetComponent<AABBCollider>();
	if (!playerCollider) { return; }

	// ワールド座標でのAABBを取得
	const Unnamed::AABB playerWorldAABB     = playerCollider->GetWorldAABB();
	const Unnamed::AABB checkpointWorldAABB = mCollider->GetWorldAABB();

	// AABB同士の衝突判定（標準的なAABB交差テスト）
	const bool isInside =
		playerWorldAABB.min.x <= checkpointWorldAABB.max.x &&
		playerWorldAABB.max.x >= checkpointWorldAABB.min.x &&
		playerWorldAABB.min.y <= checkpointWorldAABB.max.y &&
		playerWorldAABB.max.y >= checkpointWorldAABB.min.y &&
		playerWorldAABB.min.z <= checkpointWorldAABB.max.z &&
		playerWorldAABB.max.z >= checkpointWorldAABB.min.z;

	// 入った瞬間のみ起動
	if (isInside && !mWasInside && !mActivated) {
		// CheckpointManagerを通じて順番をチェック
		if (CheckpointManager::CanActivateCheckpoint(this)) { Activate(); }
	}

	mWasInside = isInside;
}
