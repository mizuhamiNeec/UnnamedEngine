#pragma once

#include <engine/Components/Transform/SceneComponent.h>
#include <runtime/core/math/Math.h>
#include <runtime/physics/core/UPhysics.h>


struct MovementData;

/// @brief プレイヤー移動用のキネマティックな衝突解決と移動処理を行うユーティリティクラス
class KinematicCollisionResolver {
public:
	KinematicCollisionResolver(UPhysics::Engine* physicsEngine,
	                           MovementData*     data,
	                           SceneComponent*   sceneComponent,
	                           Unnamed::Box*     hull);

	/// @brief 衝突判定付きで移動を行う
	/// @param dt 経過時間
	void MoveWithCollisions(float dt);

	/// @brief MovementComponent::UpdateHullDimensions から呼ばれ、ハルの同期を行う
	void SyncHullFromComponent();

	/// @brief スタック状態の検出と解決
	/// @param dt 経過時間
	void DetectAndResolveStuck(float dt);

    /// @brief ハルを描画用ではなく衝突用に取得
    [[nodiscard]] Unnamed::Box BuildHullAtFeet(const Vec3& feetPos) const;

private:
	UPhysics::Engine* mPhysicsEngine = nullptr;
	MovementData*     mData          = nullptr;
	SceneComponent*   mScene         = nullptr;
	Unnamed::Box*     mHull          = nullptr;

	static constexpr float kStepHeightHu    = 18.0f; // HL2 Default
	static constexpr float kCastSkinHu      = 0.5f;
	static constexpr float kSkinHu          = 1.0f;
	static constexpr float kRestOffsetHu    = 0.75f;
	static constexpr float kMaxAdhesionHu   = 2.0f; // 接地維持の最大距離
	static constexpr float kSnapVyMax       = 1.0f; // m/s
	static constexpr int   kMaxBumps        = 8;    // 最大衝突回数
	static constexpr int   kMaxClipPlanes   = 5;
	static constexpr float kFracEps         = 0.01f;

	// スタック検知
	static constexpr float kStuckThreshold     = 0.01f; // m/s
	static constexpr float kStuckTimeThreshold = 0.5f;  // seconds
	static constexpr float kStuckEscapeForce   = 5.0f;  // m/s upward

	[[nodiscard]] static float StepHeightM() {
		return Math::HtoM(kStepHeightHu);
	}

	[[nodiscard]] static float CastSkinM() { return Math::HtoM(kCastSkinHu); }
	[[nodiscard]] static float SkinM() { return Math::HtoM(kSkinHu); }

	[[nodiscard]] static float RestOffsetM() {
		return Math::HtoM(kRestOffsetHu);
	}

	[[nodiscard]] static float MaxAdhesionM() {
		return Math::HtoM(kMaxAdhesionHu);
	}

	// 内部処理
	void ResolvePenetration();
	int  SlideMove(Vec3& position, Vec3& velocity, float timeTotal);
	void StepMove(Vec3& position, Vec3& velocity, float timeTotal);
	bool GroundCheck(Vec3& position);

	static Vec3 ClipVelocity(const Vec3& vel, const Vec3& normal, float overbounce);
	void        UpdateEntityHullDimensions(); // 移動や解消後に現在のハル位置を同期するため
};
