#pragma once
#include <map>
#include <vector>

#include <runtime/core/math/Math.h>

class CheckpointComponent;
class Entity;

/**
 * @brief チェックポイントシステムのマネージャー
 * @details 全チェックポイントの管理、順番制御、リスポーン処理を行います。
 */
class CheckpointManager {
public:
	/// @brief 初期化
	static void Initialize();

	/// @brief シャットダウン
	static void Shutdown();

	/// @brief プレイヤーエンティティを設定
	static void SetPlayer(Entity* player);

	/// @brief プレイヤーエンティティを取得
	static Entity* GetPlayer();

	/// @brief チェックポイントを登録
	static void RegisterCheckpoint(CheckpointComponent* checkpoint);

	/// @brief チェックポイントの登録を解除
	static void UnregisterCheckpoint(CheckpointComponent* checkpoint);

	/// @brief 全チェックポイントをリセット
	static void ResetAllCheckpoints();

	/// @brief チェックポイントが起動可能か（順番チェック）
	/// @param checkpoint チェックする対象のチェックポイント
	/// @return 起動可能ならtrue
	static bool CanActivateCheckpoint(CheckpointComponent* checkpoint);

	/// @brief チェックポイントが起動された時の処理
	static void OnCheckpointActivated(CheckpointComponent* checkpoint);

	/// @brief 全てのチェックポイントが起動されているか
	static bool AreAllCheckpointsActivated();

	/// @brief 最後に起動されたチェックポイントを取得
	static CheckpointComponent* GetLastActivatedCheckpoint();

	/// @brief 次に起動すべきチェックポイントを取得
	static CheckpointComponent* GetNextCheckpoint();

	/// @brief 最後のチェックポイントにリスポーン
	static void RespawnAtLastCheckpoint();

	/// @brief デバッグ情報を取得
	static int GetActivatedCheckpointCount();
	static int GetTotalCheckpointCount();

private:
	static Entity*                             sPlayer;
	static std::map<int, CheckpointComponent*> sCheckpoints;
	// order -> checkpoint
	static CheckpointComponent* sLastActivatedCheckpoint;
	static int                  sNextExpectedOrder;
	static bool                 sIsInitialized;
};
