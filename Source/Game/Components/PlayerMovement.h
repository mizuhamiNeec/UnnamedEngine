#pragma once

#include <vector>
#include <Components/CharacterMovement.h>
#include <Components/Base/Component.h>

#include <Lib/Math/Vector/Vec3.h>

#include "Lib/Math/Quaternion/Quaternion.h"

class TransformComponent;

class PlayerMovement : public CharacterMovement {
public:
	~PlayerMovement() override;
	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

	void Move() override;

	void SetIsGrounded(bool cond);

	void SetVelocity(Vec3 newVel);

	[[nodiscard]] Vec3 GetHeadPos() const override;

	void AddCameraShakeEntity(Entity* ent,
	                          float   rotationMultiplier = 1.0f) {
		if (!ent) return;

		// 既に存在する場合は強度を更新
		for (auto& info : shakeEntities_) {
			if (info.entity == ent) {
				info.rotationMultiplier = rotationMultiplier;
				return;
			}
		}

		// 新規追加
		EntityShakeInfo info;
		info.entity             = ent;
		info.rotationMultiplier = rotationMultiplier;
		shakeEntities_.push_back(info);
	}

	void StartCameraShake(float duration, float amplitude, float frequency,
	                      const Vec3& direction, float rotationAmplitude,
	                      float rotationFrequency, const Vec3& rotationAxis);

private:
	Vec3 moveInput_ = Vec3::zero;
	Vec3 wishdir_   = Vec3::zero;
	
	bool  bCanDoubleJump_      = false;
	bool  bDoubleJumped_       = false;
	float doubleJumpVelocity_  = 300.0f; // 二段ジャンプの速度
	bool  bJumpKeyWasReleased_ = true;
	
	enum SlideState {
		NotSliding,
		Sliding,
		SlideRecovery
	};

	SlideState slideState         = NotSliding;
	SlideState previousSlideState_ = NotSliding; // 前回のスライド状態
	float      slideTimer         = 0.0f;
	float      slideRecoveryTimer = 0.0f;

	const float slideMinSpeed     = 180.0f; // スライドを維持するための最小速度
	const float slideBoost        = 1.5f;   // スライド中の加速量
	const float slideFriction     = 0.25f;  // スライド中の摩擦係数
	const float maxSlideTime      = 1.0f;   // スライドの最大維持時間(秒)
	const float slideRecoveryTime = 0.5f;   // スライド後の回復時間(秒)
	const float slideDownForce    = 150.0f; // スライド中の下方向への力
	const float kMinSlideTime     = 0.5f;   // 最小スライド時間（秒）

	Vec3 mSlideDir = Vec3::zero; // スライド中の方向ベクトル
	
	struct CameraShake {
		float duration    = 0.0f;       // 揺れの持続時間
		float currentTime = 0.0f;       // 経過時間
		float amplitude   = 0.0f;       // 揺れの強さ
		float frequency   = 0.0f;       // 揺れの頻度
		Vec3  direction   = Vec3::zero; // 揺れの方向
		bool  isActive    = false;      // 揺れがアクティブかどうか

		// 回転
		float rotationAmplitude = 0.0f;     // 回転の強さ
		float rotationFrequency = 0.0f;     // 回転の頻度
		Vec3  rotationAxis      = Vec3::up; // 回転軸

		float baseAmplitude;         // 基本の振幅
		float baseRotationAmplitude; // 基本の回転振幅
		float maxAmplitude;          // 最大振幅
		float maxRotationAmplitude;  // 最大回転振幅
	};

	CameraShake cameraShake_;

	struct EntityShakeInfo {
		Entity* entity             = nullptr;
		float   rotationMultiplier = 1.0f; // 回転の倍率
	};

	std::vector<EntityShakeInfo> shakeEntities_;
	
	void ProcessInput();
	void UpdateCameraShake(float deltaTime);
	void CollideAndSlide(const Vec3& desiredDisplacement);
};
