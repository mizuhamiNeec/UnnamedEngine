#pragma once
#include <engine/Components/base/Component.h>
#include <runtime/core/math/Math.h>
#include <runtime/physics/core/UPhysics.h>

// --- Movement state ----------------------------------------------------------
enum class MOVEMENT_STATE {
	GROUND,
	AIR,
	WALL_RUN,
	SLIDE,
};

inline const char* ToString(MOVEMENT_STATE e) {
	switch (e) {
	case MOVEMENT_STATE::GROUND: return "GROUND";
	case MOVEMENT_STATE::AIR: return "AIR";
	case MOVEMENT_STATE::WALL_RUN: return "WALL_RUN";
	case MOVEMENT_STATE::SLIDE: return "SLIDE";
	}
	return "unknown";
}

// --- Data container ----------------------------------------------------------
struct MovementData {
	MovementData(float width, float height)
		: currentWidth(width), currentHeight(height) {
		defaultHeight = height;
		crouchHeight  = height * 0.75f;
	}

	MovementData() : currentWidth(32.0f), currentHeight(72.0f) {
		defaultHeight = currentHeight;
		crouchHeight  = currentHeight * 0.75f;
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
	float        currentWidth{};  // HU
	float        currentHeight{}; // HU
	float        defaultHeight{}; // HU
	float        crouchHeight{};  // HU

	// Movement params (HU/s 相当のcvarと併用)
	float crouchSpeed  = 63.3f;
	float walkSpeed    = 150.0f;
	float sprintSpeed  = 320.0f;
	float currentSpeed = sprintSpeed;

	// Ground cache
	Vec3  lastGroundNormal = Vec3::up;
	float lastGroundDistM  = 0.0f;
};

// --- Component ---------------------------------------------------------------
class Movement : public Component {
public:
	void OnAttach(Entity& owner) override;
	void Init(UPhysics::Engine* uphysics, const MovementData& md);

	void PrePhysics(float dt) override;
	void Update(float dt) override;
	void PostPhysics(float dt) override;

	void DrawInspectorImGui() override;

	Vec3& GetVelocity();
	Vec3  GetHeadPos() const;
	void  SetVelocity(const Vec3& v);

private:
	// high-level
	void ProcessInput();
	void ProcessMovement(float dt);

	// modes
	void Ground(float wishspeed, float dt);
	void Air(float wishspeed, float dt);

	// forces
	void ApplyHalfGravity(float dt);
	void Friction(float dt);
	void Accelerate(Vec3 dir, float speed, float accel, float dt);
	void AirAccelerate(Vec3 dir, float speed, float accel, float dt);

	// core move
	void MoveCollideAndSlide(float dt);

	// low-level traces
	bool SlideMove(float dt, Vec3& inOutVel, Vec3& outPos);

	// ground helpers
	bool GroundProbe(float probeDownMeters, Vec3& outN, float& outDownDist);
	bool IsWalkable(const Vec3& n, bool wasGrounded) const;

	// step helpers
	bool TryStepMove(float dt, Vec3& ioVel, Vec3& outPos);
	bool StepDownToGround(Vec3& ioPos, Vec3& ioVel, float maxDownM,
	                      Vec3* outGroundN = nullptr);

	// misc
	static Vec3 ClipVelocity(const Vec3& v, const Vec3& n, float overbounce);
	static Vec3 ProjectOntoPlane(const Vec3& v, const Vec3& n);

	void StayOnGround(Vec3& ioVel, Vec3& ioPos); // 上げない・下だけ
	bool RefreshGroundCache(float probeLenM);
	void RefreshHullFromTransform();
	bool SweepNoTimeWithSkin(const Vec3& delta, float skinM, Vec3& outPos);
	void CheckForNaNAndClamp();

	// params (Source 寄り)
	static constexpr float kStepHeightHU   = 18.0f; // 72HUハルに対して既定
	static constexpr float kCastSkinHU     = 0.25f;
	static constexpr float kSkinHU         = 0.20f;
	static constexpr float kRestOffsetHU   = 0.75f;
	static constexpr float kMaxAdhesionHU  = 4.0f;
	static constexpr float kSnapVyMax      = 1.0f;  // m/s
	static constexpr float kGroundEnterDeg = 45.0f; // 接地“になる”
	static constexpr float kGroundExitDeg  = 47.0f; // 接地“やめる”
	static constexpr int   kMaxClipPlanes  = 5;
	static constexpr float kFracEps        = 1e-4f;
	static constexpr float kAirSpeedCap    = 30.0f;

	float StepHeightM() const { return Math::HtoM(kStepHeightHU); }
	float CastSkinM() const { return Math::HtoM(kCastSkinHU); }
	float SkinM() const { return Math::HtoM(kSkinHU); }
	float RestOffsetM() const { return Math::HtoM(kRestOffsetHU); }
	float MaxAdhesionM() const { return Math::HtoM(kMaxAdhesionHU); }

private:
	UPhysics::Engine* mUPhysicsEngine = nullptr;
	MovementData      mData;
};
