#pragma once
#include <engine/Components/base/Component.h>
#include <runtime/core/math/Math.h>

class AABBCollider;

/**
 * @brief チェックポイントコンポーネント
 * @details プレイヤーが通過するチェックポイントを管理します。
 *          順番通りに通過する必要があります。
 */
class CheckpointComponent : public Component {
public:
	explicit CheckpointComponent(int order, const Vec3& respawnPosition);
	~CheckpointComponent() override;

	void OnAttach(Entity& owner) override;
	void OnDetach() override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

	/// @brief このチェックポイントが起動されているか
	[[nodiscard]] bool IsActivated() const;

	/// @brief チェックポイントを起動する
	void Activate();

	/// @brief チェックポイントをリセット（未起動状態に戻す）
	void Reset();

	/// @brief チェックポイントの順番を取得
	[[nodiscard]] int GetOrder() const;

	/// @brief リスポーン位置を取得
	[[nodiscard]] Vec3 GetRespawnPosition() const;

	/// @brief リスポーン位置を設定
	void SetRespawnPosition(const Vec3& position);

private:
	void CheckPlayerCollision();

	AABBCollider* mCollider        = nullptr;
	int           mOrder           = 0;
	Vec3          mRespawnPosition = Vec3::zero;
	bool          mActivated       = false;
	bool          mWasInside       = false; // 前フレームで内部にいたか
};
