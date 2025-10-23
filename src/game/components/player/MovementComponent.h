#pragma once
#include <engine/Components/base/Component.h>
#include <runtime/core/math/Math.h>
#include <runtime/physics/core/UPhysics.h>

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
	Unnamed::Box hull{};
	float        currentWidthHu{};
	float        currentHeightHu{};
	float        defaultHeightHu{};
	float        crouchHeightHu{};

	// 地上での速度
	float crouchSpeed  = 63.3f;
	float walkSpeed    = 150.0f;
	float sprintSpeed  = 320.0f;
	float currentSpeed = sprintSpeed;

	// 接地検知
	Vec3  lastGroundNormal = Vec3::up;
	float lastGroundDistM  = 0.0f;

	float groundEnterSlopeThreshold = 0.707f;
	float groundLeaveSlopeThreshold = 0.7f;

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
};

class MovementComponent : public Component {
public:
	void OnAttach(Entity& owner) override;
	void Init(UPhysics::Engine* uphysics, const MovementData& md);

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

private:
	// 高レベル
	void ProcessInput();
	void ProcessMovement(float dt);

	// 各移動モード
	void Ground(float wishspeed, float dt);
	void Air(float wishspeed, float dt);
	void Slide(float wishspeed, float dt);

	// forces
	void ApplyHalfGravity(float dt);
	void Friction(float dt);
	void Accelerate(Vec3 dir, float speed, float accel, float dt);
	void AirAccelerate(Vec3 dir, float speed, float accel, float dt);

	void UpdateHullDimensions();
	void CheckForNaNAndClamp();

	// 衝突応答
	void MoveWithCollisions(float dt);

	// params
	static constexpr float kStepHeightHu   = 18.0f; // HL2 Def
	static constexpr float kCastSkinHu     = 0.25f;
	static constexpr float kSkinHu         = 0.25f;
	static constexpr float kRestOffsetHu   = 0.75f;
	static constexpr float kMaxAdhesionHu  = 2.0f; // 接地維持の最大距離
	static constexpr float kSnapVyMax      = 1.0f; // m/s
	static constexpr int   kMaxBumps       = 8;    // 最大衝突回数
	static constexpr int   kMaxClipPlanes  = 5;
	static constexpr float kFracEps        = 1e-4f;
	static constexpr float kAirSpeedCap    = 30.0f;
	static constexpr float kJumpVelocityHu = 400.0f; // HU/s

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

	// スタック検知
	void                   DetectAndResolveStuck(float dt);
	static constexpr float kStuckThreshold     = 0.01f; // m/s
	static constexpr float kStuckTimeThreshold = 0.5f;  // seconds
	static constexpr float kStuckEscapeForce   = 5.0f;  // m/s upward

	// ウォールラン
	void                   Wallrun(float wishspeed, float dt);
	bool                   TryStartWallrun();
	void                   UpdateWallrun(float dt);
	void                   EndWallrun();
	bool                   CanWallrun() const;
	static constexpr float kWallrunMinSpeed          = 200.0f; // HU/s
	static constexpr float kWallrunMaxTime           = 2.5f; // seconds
	static constexpr float kWallrunGravityScale      = 0.1f; // 重力軽減（小さいほど軽い）
	static constexpr float kWallrunJumpForce         = 350.0f; // HU/s
	static constexpr float kWallrunCooldown          = 0.1f; // seconds
	static constexpr float kWallrunSameWallCooldown  = 1.0f; // seconds
	static constexpr bool  kWallrunDetachOnSideInput = true; // 左右入力で離脱するか
	static constexpr float kWallrunVerticalDamping   = 0.3f; // 地上ジャンプからの垂直速度減衰

	// ダブルジャンプ
	static constexpr float kDoubleJumpVelocityHu = 300.0f;

	// スライディング
	void                   TryStartSlide();
	void                   UpdateSlide(float dt);
	void                   EndSlide();
	bool                   CanSlide() const;
	static constexpr float kSlideMinSpeed    = 200.0f;  // HU/s - スライディング開始最低速度
	static constexpr float kSlideMaxTime     = 20.0f;   // seconds - 最大スライディング時間
	static constexpr float kSlideFriction    = 100.0f;  // HU/s^2 - スライディング摩擦
	static constexpr float kSlideBoostSpeed  = 4.0f;    // HU/s - 開始ブースト
	static constexpr float kSlideStopSpeed   = 50.0f;   // HU/s - この速度以下で自動終了
	static constexpr float kSlideHopSpeedCap = 2000.0f; // HU/s - スライドホップの速度上限

	UPhysics::Engine* mUPhysicsEngine = nullptr;
	MovementData      mData;
};
