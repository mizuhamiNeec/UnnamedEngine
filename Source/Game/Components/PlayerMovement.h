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
	void ProcessInput();
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


	bool  bCanDoubleJump_      = false;
	bool  bDoubleJumped_       = false;
	float doubleJumpVelocity_  = 300.0f; // 二段ジャンプの速度
	bool  bJumpKeyWasReleased_ = true;


	SlideState  previousSlideState_ = NotSliding; // 前回のスライド状態
	const float kMinSlideTime       = 0.5f;       // 最小スライド時間（秒）

	std::vector<EntityShakeInfo> shakeEntities_;

	void UpdateCameraShake(float deltaTime);

	void CollideAndSlide(const Vec3& desiredDisplacement);

	// プレイヤーの移動入力
	Vec3 moveInput_ = Vec3::zero;
	Vec3 wishdir_   = Vec3::zero;
};
