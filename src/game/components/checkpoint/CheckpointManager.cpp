#include "CheckpointManager.h"

#include <algorithm>

#include <engine/Entity/Entity.h>
#include <engine/unnamed/subsystem/console/Log.h>
#include <game/components/checkpoint/CheckpointComponent.h>
#include <game/components/player/MovementComponent.h>

#include "engine/OldConsole/Console.h"

// 静的メンバの定義
Entity* CheckpointManager::sPlayer = nullptr;
std::map<int, CheckpointComponent*> CheckpointManager::sCheckpoints;
CheckpointComponent* CheckpointManager::sLastActivatedCheckpoint = nullptr;
int CheckpointManager::sNextExpectedOrder = 0;

void CheckpointManager::Initialize() {
	sPlayer = nullptr;
	sCheckpoints.clear();
	sLastActivatedCheckpoint = nullptr;
	sNextExpectedOrder       = 0;
}

void CheckpointManager::Shutdown() {
	sPlayer = nullptr;
	sCheckpoints.clear();
	sLastActivatedCheckpoint = nullptr;
	sNextExpectedOrder       = 0;
}

void CheckpointManager::SetPlayer(Entity* player) { sPlayer = player; }

Entity* CheckpointManager::GetPlayer() { return sPlayer; }

void CheckpointManager::RegisterCheckpoint(CheckpointComponent* checkpoint) {
	if (!checkpoint) { return; }

	const int order     = checkpoint->GetOrder();
	sCheckpoints[order] = checkpoint;
}

void CheckpointManager::UnregisterCheckpoint(CheckpointComponent* checkpoint) {
	if (!checkpoint) { return; }

	const int order = checkpoint->GetOrder();
	if (sCheckpoints.contains(order)) { sCheckpoints.erase(order); }

	if (sLastActivatedCheckpoint == checkpoint) {
		sLastActivatedCheckpoint = nullptr;
	}
}

void CheckpointManager::ResetAllCheckpoints() {
	for (const auto& [order, checkpoint] : sCheckpoints) {
		checkpoint->Reset();
	}
	sLastActivatedCheckpoint = nullptr;
	sNextExpectedOrder       = 0;
}

bool CheckpointManager::CanActivateCheckpoint(CheckpointComponent* checkpoint) {
	if (!checkpoint) { return false; }

	// 既に起動されている場合は不可
	if (checkpoint->IsActivated()) { return false; }

	// 順番通りかチェック
	const int order = checkpoint->GetOrder();
	return order == sNextExpectedOrder;
}

void CheckpointManager::OnCheckpointActivated(CheckpointComponent* checkpoint) {
	if (!checkpoint) { return; }

	const int order = checkpoint->GetOrder();
	Console::Print(
		std::format("Checkpoint {} activated!", order),
		Vec4(0.0f, 1.0f, 1.0f, 1.0f)
	);

	sLastActivatedCheckpoint = checkpoint;
	sNextExpectedOrder       = order + 1;
}

bool CheckpointManager::AreAllCheckpointsActivated() {
	for (const auto& [order, checkpoint] : sCheckpoints) {
		if (!checkpoint->IsActivated()) { return false; }
	}
	return !sCheckpoints.empty();
}

CheckpointComponent* CheckpointManager::GetLastActivatedCheckpoint() {
	return sLastActivatedCheckpoint;
}

void CheckpointManager::RespawnAtLastCheckpoint() {
	if (!sPlayer) {
		Console::Print(
			"Cannot respawn: Player not set!",
			Vec4(1.0f, 0.0f, 0.0f, 1.0f)
		);
		return;
	}

	Vec3 respawnPos = Vec3::up * 2.0f; // デフォルトのスポーン位置

	if (sLastActivatedCheckpoint) {
		respawnPos = sLastActivatedCheckpoint->GetRespawnPosition();
		Console::Print(
			std::format(
				"Respawning at checkpoint {}...",
				sLastActivatedCheckpoint->GetOrder()
			),
			Vec4(0.0f, 1.0f, 1.0f, 1.0f)
		);
	} else {
		Console::Print(
			"Respawning at start position...\n",
			Vec4(1.0f, 1.0f, 0.0f, 1.0f)
		);
	}

	// プレイヤーの位置をリセット
	sPlayer->GetTransform()->SetWorldPos(respawnPos);

	// プレイヤーの速度をリセット
	auto* movementComponent = sPlayer->GetComponent<MovementComponent>();
	if (movementComponent) { movementComponent->SetVelocity(Vec3::zero); }
}

int CheckpointManager::GetActivatedCheckpointCount() {
	int count = 0;
	for (const auto& [order, checkpoint] : sCheckpoints) {
		if (checkpoint->IsActivated()) { count++; }
	}
	return count;
}

int CheckpointManager::GetTotalCheckpointCount() {
	return static_cast<int>(sCheckpoints.size());
}

CheckpointComponent* CheckpointManager::GetNextCheckpoint() {
	auto it = sCheckpoints.find(sNextExpectedOrder);
	if (it != sCheckpoints.end()) { return it->second; }
	return nullptr;
}
