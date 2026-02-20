#pragma once
#include <engine/Components/base/Component.h>
#include <runtime/core/math/Math.h>
#include <runtime/physics/core/UPhysics.h>
#include <memory>

namespace Unnamed {
	class ConsoleSystem;
}

class Audio;
class AABBCollider;
class KinematicCollisionResolver;

/**
 * @brief プレイヤーの移動状態
 */
enum class MOVEMENT_STATE {
	GROUND,   ///< 地面に接地している
	AIR,      ///< 空中にいる
	WALL_RUN, ///< 壁走り中
	SLIDE,    ///< スライド中
};

namespace {
	const char* ToString(const MOVEMENT_STATE e) {
		switch (e) {
			case MOVEMENT_STATE::GROUND: return "GROUND";
			case MOVEMENT_STATE::AIR: return "AIR";
			case MOVEMENT_STATE::WALL_RUN: return "WALL_RUN";
			case MOVEMENT_STATE::SLIDE: return "SLIDE";
		}
		return "unknown";
	}
}

/**
 * @brief プレイヤーの移動データ構造体
 * @details プレイヤーの速度、状態、入力などの移動に関する全ての情報を保持します
 */
struct MovementData {
	MovementData(float width, float height);
	MovementData();

	// 入力
	Vec2 vecMoveInput  = Vec2::zero;
	Vec3 wishDirection = Vec3::zero;
	bool wishJump      = false;
	bool wishCrouch    = false;

	// ステート
	MOVEMENT_STATE state      = MOVEMENT_STATE::AIR;
	Vec3           velocity   = Vec3::zero;
	bool           isGrounded = false;

	// ハル
	float currentWidthHu{};
	float currentHeightHu{};
	float defaultHeightHu{};
	float crouchHeightHu{};

	// 地上での速度
	float crouchSpeed  = 63.3f;
	float walkSpeed    = 150.0f;
	float sprintSpeed  = 320.0f;
	float currentSpeed = sprintSpeed;

	// 接地検知
	Vec3  lastGroundNormal = Vec3::up;
	float lastGroundDistM  = 0.0f;
	float groundNormalY    = 0.7f;

	// スタック検知
	Vec3  lastPosition = Vec3::zero;
	float stuckTime    = 0.0f;
	bool  isStuck      = false;

	// ウォールラン
	bool  isWallRunning         = false;
	Vec3  wallRunNormal         = Vec3::zero;
	Vec3  wallRunDirection      = Vec3::zero;
	float wallRunTime           = 0.0f;
	float timeSinceLastWallRun  = 0.0f;
	Vec3  lastWallRunNormal     = Vec3::zero;
	bool  wallRunJumpWasPressed = false; // ウォールラン開始時にジャンプが押されていたか?

	// ダブルジャンプ
	bool hasDoubleJump     = true;  // ダブルジャンプが使用可能か?
	bool lastFrameWishJump = false; // 前フレームのジャンプ入力状態

	// スライディング
	bool  isSliding      = false;      // スライディング中か?
	Vec3  slideDirection = Vec3::zero; // スライディング方向
	float slideTime      = 0.0f;       // スライディング経過時間

	// 着地検知
	bool  wasGroundedLastFrame = false; // 前フレームの接地していたか?
	float lastLandingVelocityY = 0.0f;  // 着地時の垂直速度
	bool  justLanded           = false; // 今フレーム着地したか?
	float jumpSnapDisableTime  = 0.0f;

	// 入力強度
	float moveInputIntensity = 0.0f;
};

class MovementComponent : public Component {
public:
	void OnAttach(Entity& owner) override;
	void Init(UPhysics::Engine* uPhysics, const MovementData& md);

	void PrePhysics(float dt) override;
	void Update(float dt) override;
	void PostPhysics(float dt) override;

	void DrawInspectorImGui() override;

	Vec3&              GetVelocity();
	[[nodiscard]] Vec3 GetHeadPos() const;
	void               SetVelocity(const Vec3& v);

	// Getters for camera animation
	[[nodiscard]] bool  IsGrounded() const;
	[[nodiscard]] bool  WishJump() const;
	[[nodiscard]] bool  IsWallRunning() const;
	[[nodiscard]] bool  IsSliding() const;
	[[nodiscard]] bool  HasDoubleJump() const;
	[[nodiscard]] Vec3  GetWallRunNormal() const;
	[[nodiscard]] bool  JustLanded() const;
	[[nodiscard]] float GetLastLandingVelocityY() const;
	[[nodiscard]] bool  IsDucking() const;

	// ウォールラン・スライド制御（Stateから呼ばれる）
	bool TryStartWallrun();
	void UpdateWallrun(float dt);
	void EndWallrun();
	bool CanWallrun() const;

	void TryStartSlide();
	void UpdateSlide(float dt);
	void EndSlide();
	bool CanSlide() const;

	void UpdateHullDimensions();
	void CheckForNaNAndClamp();

	UPhysics::Engine* GetPhysicsEngine() const { return mUPhysicsEngine; }
	Unnamed::Box&     GetHull() { return mHull; }
	MovementData&     GetData() { return mData; }

	// 動的地形
	Vec3       mSurfaceVelocity      = Vec3::zero;
	Entity*    mLastGroundEntity     = nullptr;
	Vec3       mLastGroundPosition   = Vec3::zero;
	Quaternion mLastGroundRotation   = Quaternion::identity;
	Vec3       mRelativeVelocity     = Vec3::zero;
	bool       mWasOnMovingSurface   = false;

private:
	void ProcessInput();
	void ProcessMovement(float dt);
	void ProcessJump(bool isOnMovingSurface);
	void ProcessLanding();
	void ProcessFootstep(float dt);
	void UpdateMovingSurface(float dt, Entity*& currentGroundEntity, bool& isOnMovingSurface);
	void UpdateCrouch(float dt);

	// ウォールラン定数
	static constexpr float kWallrunMinSpeed         = 200.0f; // HU/s
	static constexpr float kWallrunMaxTime          = 2.5f;   // seconds
	static constexpr float kWallrunGravityScale     = 0.1f;
	static constexpr float kWallrunJumpForce        = 350.0f; // HU/s
	static constexpr float kWallrunCooldown         = 0.1f;   // seconds
	static constexpr float kWallrunSameWallCooldown = 1.0f;   // seconds
	static constexpr bool  kWallrunDetachOnSideInput = true;
	static constexpr float kWallrunVerticalDamping  = 0.3f;

	// ダブルジャンプ
	static constexpr float kDoubleJumpVelocityHu = 300.0f;

	// ジャンプ
	static constexpr float kJumpVelocityHu      = 400.0f; // HU/s
	static constexpr float kJumpSnapDisableTime  = 0.5f;  // seconds

	// スライディング定数
	static constexpr float kSlideMinSpeed    = 200.0f;  // HU/s
	static constexpr float kSlideMaxTime     = 20.0f;   // seconds
	static constexpr float kSlideBoostSpeed  = 50.0f;   // HU/s
	static constexpr float kSlideStopSpeed   = 50.0f;   // HU/s
	static constexpr float kSlideHopSpeedCap = 2000.0f; // HU/s

	// 動的地形定数
	static constexpr float kDynamicCheckSkinHu = 8.0f;

	// 足音定数
	static constexpr float kStepIntervalM = 2.0f;

	UPhysics::Engine*       mUPhysicsEngine = nullptr;
	AABBCollider*           mCollider       = nullptr;
	Unnamed::Box            mHull;
	MovementData            mData;
	Unnamed::ConsoleSystem* mConsoleSystem = nullptr;

	std::unique_ptr<KinematicCollisionResolver> mCollisionResolver;

	std::shared_ptr<Audio> mFootstepAudio;
	std::shared_ptr<Audio> mLandAudio;
	float                  mStepDistance = 0.0f;
};
