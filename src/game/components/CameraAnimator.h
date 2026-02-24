#pragma once
#include <engine/Components/base/Component.h>
#include <runtime/core/math/Math.h>

#include "engine/Components/Transform/SceneComponent.h"

class MovementComponent;
class CameraRotator;

/**
 * @brief カメラアニメーション用のコンポーネント
 */
class CameraAnimator : public Component {
public:
	/**
	 * @brief エンティティにアタッチされた際に呼ばれる
	 * @param owner 所有者エンティティ
	 */
	void OnAttach(Entity& owner) override;

	/**
	 * @brief コンポーネントの初期化
	 * @param movementComponent 移動コンポーネントへの参照
	 * @param cameraRotator カメラ回転コンポーネントへの参照
	 */
	void Init(
		MovementComponent* movementComponent,
		CameraRotator*     cameraRotator
	);

	/**
	 * @brief 毎フレーム更新処理を行う
	 * @param dt 前フレームからの経過時間
	 */
	void PrePhysics(float dt) override;
	void Update(float dt) override;

	[[nodiscard]] float GetBlinkFovOffsetDeg() const noexcept {
		return mBlinkFovOffset;
	}

	/// @brief Vault開始時にカメラオフセットを即座に設定する
	/// @param offset ローカル空間でのオフセット
	void SetVaultCameraOffset(const Vec3& offset) {
		mVaultCameraOffset = offset;
	}

	/// @brief 現在のVaultカメラオフセットを即座にLocalPosに反映する
	void ApplyVaultOffsetImmediate() {
		if (mScene) {
			mScene->SetLocalPos(mCurrentShake + mVaultCameraOffset);
		}
	}

	/**
	 * @brief ImGuiインスペクタ用のUI描画
	 */
	void DrawInspectorImGui() override;

private:
	void UpdateJumpAnimation(float dt);
	void UpdateDoubleJumpAnimation(float dt);
	void UpdateSlideAnimation(float dt);
	void UpdateWallrunAnimation(float dt);
	void UpdateLandingAnimation(float dt);
	void UpdateBlinkAnimation(float dt);
	void UpdateVaultAnimation(float dt);
	void ApplyShakeAndTilt(float dt);

	MovementComponent* mMovement = nullptr;

	// ジャンプアニメーション
	bool  mWasInAir           = false;
	float mJumpAnimTime       = 0.0f;
	float mDoubleJumpAnimTime = 0.0f;
	bool  mHadDoubleJump      = true;

	// スライディングアニメーション
	bool  mWasSliding      = false;
	float mSlideAnimTime   = 0.0f;
	float mSlideEntrySpeed = 0.0f;

	// ウォールランアニメーション
	bool  mWasWallRunning  = false;
	float mWallrunAnimTime = 0.0f;
	float mWallrunSide     = 0.0f; // -1 = left, +1 = right

	// 着地アニメーション
	bool  mLandingActive    = false;
	float mLandingAnimTime  = 0.0f;
	float mLandingIntensity = 0.0f;

	// ブリンクアニメーション
	bool  mBlinkActive   = false;
	float mBlinkAnimTime = 0.0f;
	float mBlinkFovOffset = 0.0f;

	// ヴォールトカメラアニメーション
	Vec3  mVaultCameraOffset = Vec3::zero; // カメラ位置オフセット

	// 現在のシェイク/傾き値
	Vec3  mCurrentShake = Vec3::zero;
	float mCurrentRoll  = 0.0f; // Z軸回転（傾き）
	float mCurrentPitch = 0.0f; // X軸回転（上下）

	// 借り物
	CameraRotator* mCameraRotator = nullptr;

	// パーリンノイズ用の時間変数
	float mNoiseTime = 0.0f;

	// パーリンノイズ生成
	float PerlinNoise(float x, float y, float z) const;

	//
	static constexpr float kJumpShakeAmount   = 0.15f;
	static constexpr float kJumpShakeDuration = 0.2f;

	static constexpr float kDoubleJumpShakeAmount   = 0.4f;
	static constexpr float kDoubleJumpShakeDuration = 0.3f;

	static constexpr float kSlideRollAmount  = 2.0f; // degrees
	static constexpr float kSlideShakeAmount = 0.01f;
	static constexpr float kSlideRollSpeed   = 30.0f;

	static constexpr float kWallrunRollAmount  = 45.0f;
	static constexpr float kWallrunRollSpeed   = 15.0f;
	static constexpr float kWallrunShakeAmount = 0.1f;

	static constexpr float kLandingShakeAmount   = 0.8f;
	static constexpr float kLandingShakeDuration = 0.25f;
	static constexpr float kLandingMinSpeed      = 3.0f; // m/s
	static constexpr float kLandingPitchAmount   = 4.0f; // degrees - 着地時に下を向く

	// degrees - ジャンプ時に上を向く
	static constexpr float kJumpPitchAmount = 6.0f;

	// degrees - ダブルジャンプ時にさらに上を向く
	static constexpr float kDoubleJumpPitchAmount = 7.0f;

	// ブリンクFOV
	static constexpr float kBlinkFovAmount   = 20.0f; // degrees
	static constexpr float kBlinkFovDuration = 0.05f;
	static constexpr float kBlinkShakeAmount = 0.06f;

	static constexpr float kShakeFrequency = 15.0f;
};
