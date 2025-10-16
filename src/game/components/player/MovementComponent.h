#pragma once
#include <engine/Components/base/Component.h>
#include <runtime/core/math/Math.h>
#include <runtime/physics/core/UPhysics.h>

// ステート
enum class MOVEMENT_STATE {
	GROUND,
	AIR,
	WALL_RUN,
	SLIDE,
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

struct MovementData {
	MovementData(float width, float height)
		: currentWidthHu(width), currentHeightHu(height) {
		defaultHeightHu = height;
		crouchHeightHu  = height * 0.75f;
	}

	MovementData() : currentWidthHu(32.0f), currentHeightHu(72.0f) {
		defaultHeightHu = currentHeightHu;
		crouchHeightHu  = currentHeightHu * 0.75f;
	}

	// Input
	Vec2 vecMoveInput  = Vec2::zero;
	Vec3 wishDirection = Vec3::zero;
	bool wishJump      = false;
	bool wishCrouch    = false;

	// State
	MOVEMENT_STATE state      = MOVEMENT_STATE::AIR;
	Vec3           velocity   = Vec3::zero;
	bool           isGrounded = false;

	// Hull
	Unnamed::Box hull{};
	float        currentWidthHu{};
	float        currentHeightHu{};
	float        defaultHeightHu{};
	float        crouchHeightHu{};

	// MovementComponent params (HU/s 相当のcvarと併用)
	float crouchSpeed  = 63.3f;
	float walkSpeed    = 150.0f;
	float sprintSpeed  = 320.0f;
	float currentSpeed = sprintSpeed;

	// Ground cache
	Vec3  lastGroundNormal = Vec3::up;
	float lastGroundDistM  = 0.0f;

	float groundEnterSlopeThreshold = 0.707f;
	float groundLeaveSlopeThreshold = 0.7f;

	// Stuck detection
	Vec3  lastPosition = Vec3::zero;
	float stuckTime    = 0.0f;
	bool  isStuck      = false;

	// Wallrun
	bool  isWallRunning         = false;
	Vec3  wallRunNormal         = Vec3::zero;
	Vec3  wallRunDirection      = Vec3::zero;
	float wallRunTime           = 0.0f;
	float timeSinceLastWallRun  = 0.0f;
	Vec3  lastWallRunNormal     = Vec3::zero;
	bool  wallRunJumpWasPressed = false; // ウォールラン開始時にジャンプが押されていたか?

	// Double jump
	bool hasDoubleJump     = true;  // ダブルジャンプが使用可能か?
	bool lastFrameWishJump = false; // 前フレームのジャンプ入力状態

	// Slide
	bool  isSliding      = false;      // スライディング中か?
	Vec3  slideDirection = Vec3::zero; // スライディング方向
	float slideTime      = 0.0f;       // スライディング経過時間

	// Landing detection
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

	// Collision & response (Source-style)
	void MoveWithCollisions(float dt);

	// params
	static constexpr float kStepHeightHU   = 18.0f; // 72HUハルに対して既定
	static constexpr float kCastSkinHU     = 0.25f;
	static constexpr float kSkinHU         = 0.25f;
	static constexpr float kRestOffsetHU   = 0.75f;
	static constexpr float kMaxAdhesionHU  = 2.0f; // 接地維持の最大距離
	static constexpr float kSnapVyMax      = 1.0f; // m/s
	static constexpr int   kMaxBumps       = 8;    // 最大衝突回数（Source準拠）
	static constexpr int   kMaxClipPlanes  = 5;
	static constexpr float kFracEps        = 1e-4f;
	static constexpr float kAirSpeedCap    = 30.0f;
	static constexpr float kJumpVelocityHu = 400.0f; // HU/s

	float StepHeightM() const { return Math::HtoM(kStepHeightHU); }
	float CastSkinM() const { return Math::HtoM(kCastSkinHU); }
	float SkinM() const { return Math::HtoM(kSkinHU); }
	float RestOffsetM() const { return Math::HtoM(kRestOffsetHU); }
	float MaxAdhesionM() const { return Math::HtoM(kMaxAdhesionHU); }

	// Stuck detection
	void                   DetectAndResolveStuck(float dt);
	static constexpr float kStuckThreshold     = 0.01f; // m/s
	static constexpr float kStuckTimeThreshold = 0.5f;  // seconds
	static constexpr float kStuckEscapeForce   = 5.0f;  // m/s upward

	// Wallrun
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

	// Double jump
	static constexpr float kDoubleJumpVelocityHu = 300.0f;

	// Slide
	void                   TryStartSlide();
	void                   UpdateSlide(float dt);
	void                   EndSlide();
	bool                   CanSlide() const;
	static constexpr float kSlideMinSpeed    = 200.0f; // HU/s - スライディング開始最低速度
	static constexpr float kSlideMaxTime     = 20.0f; // seconds - 最大スライディング時間
	static constexpr float kSlideFriction    = 100.0f; // HU/s^2 - スライディング摩擦（低い）
	static constexpr float kSlideBoostSpeed  = 4.0f; // HU/s - 開始時のブースト
	static constexpr float kSlideStopSpeed   = 50.0f; // HU/s - この速度以下で自動終了
	static constexpr float kSlideHopSpeedCap = 2000.0f; // HU/s - スライドホップの速度上限

private:
	UPhysics::Engine* mUPhysicsEngine = nullptr;
	MovementData      mData;
};
