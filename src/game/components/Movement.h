#pragma once
#include <engine/Components/base/Component.h>

#include <runtime/core/math/Math.h>

#include <runtime/physics/core/UPhysics.h>

// ヘッダでの無名名前空間は避ける
enum class MOVEMENT_STATE {
	GROUND,
	AIR, // 飛んでるんじゃない、落ちてるだけだ。かっこつけてな
	WALL_RUN,
	SLIDE,
};

inline const char* ToString(const MOVEMENT_STATE e) {
	switch (e) {
	case MOVEMENT_STATE::GROUND: return "GROUND";
	case MOVEMENT_STATE::AIR: return "AIR";
	case MOVEMENT_STATE::WALL_RUN: return "WALL_RUN";
	case MOVEMENT_STATE::SLIDE: return "SLIDE";
	}
	return "unknown";
}

struct MovementData {
	MovementData(
		const float width, const float height
	) : currentWidth(width), currentHeight(height) {
		defaultHeight = height;
		crouchHeight  = height * 0.75f;
	}

	MovementData() : currentWidth(32.0f), currentHeight(72.0f) {
		defaultHeight = currentHeight;
		crouchHeight  = currentHeight * 0.75f;
	}

	// Input
	Vec2 vecMoveInput  = Vec2::zero; // 移動入力
	Vec3 wishDirection = Vec3::zero; // カメラが向いている方向(y成分は0)
	bool wishJump      = false;      // そのフレームでジャンプしたい
	bool wishCrouch    = false;      // そのフレームでしゃがみたい

	// State
	MOVEMENT_STATE state = MOVEMENT_STATE::AIR; // 現在の移動状態
	Vec3           velocity;                    // 速度ベクトル

	// Hull
	float currentWidth;  // キャラクターの幅
	float currentHeight; // キャラクターの高さ
	float defaultHeight; // 通常時の高さ
	float crouchHeight;  // しゃがみ時の高さ

	// Movement params
	float crouchSpeed  = 63.3f;
	float walkSpeed    = 150.f;
	float sprintSpeed  = 320.f;
	float currentSpeed = sprintSpeed; // 現在の移動速度
};

class Movement : public Component {
public:
	void OnAttach(Entity& owner) override;
	void Init(UPhysics::Engine* uphysics, const MovementData& movementData);

	void PrePhysics(float deltaTime) override;
	void Update(float deltaTime) override;
	void PostPhysics(float deltaTime) override;

	void DrawInspectorImGui() override;

	// Accessor
	Vec3& GetVelocity();
	Vec3  GetHeadPos() const;

	void SetVelocity(const Vec3& vel);

private:
	void ProcessInput();
	void ProcessMovement(float dt);

	void Ground(float wishspeed, float dt);
	void Air(float wishspeed, float dt);
	void WallRun(float dt);
	void Slide(float dt);

	void ApplyHalfGravity(float dt);
	void Friction(float dt);
	void Accelerate(Vec3 dir, float speed, float accel, float dt);
	void AirAccelerate(Vec3 dir, float speed, float accel, float dt);

	void TryMove();

	void CheckForNaNAndClamp();

private:
	UPhysics::Engine* mUPhysicsEngine = nullptr;

	MovementData mData;

	static constexpr float kAirSpeedCap = 30.0f;
};
