#include "CameraAnimator.h"
#include <cmath>
#include <algorithm>
#include <engine/Entity/Entity.h>
#include <engine/ImGui/ImGuiUtil.h>
#include "CameraRotator.h"
#include "player/MovementComponent.h"

#include "engine/OldConsole/Console.h"

namespace {
	/// @brief フェード関数
	/// @param t 入力値
	/// @return フェード後の値
	float Fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

	/// @brief 勾配関数
	/// @param hash ハッシュ値
	/// @param x x座標
	/// @param y y座標
	/// @param z z座標
	/// @return 勾配値
	float Grad(int hash, float x, float y, float z) {
		int   h = hash & 15;
		float u = h < 8 ? x : y;
		float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
		return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
	}

	// パーミュテーションテーブル
	constexpr int kPermutation[256] = {
		151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225,
		140, 36, 103, 30, 69, 142,
		8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197,
		62, 94, 252, 219, 203, 117,
		35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171,
		168, 68, 175, 74, 165, 71,
		134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211,
		133, 230, 220, 105, 92, 41,
		55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73,
		209, 76, 132, 187, 208, 89,
		18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173,
		186, 3, 64, 52, 217, 226,
		250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206,
		59, 227, 47, 16, 58, 17, 182,
		189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70,
		221, 153, 101, 155, 167, 43,
		172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185,
		112, 104, 218, 246, 97,
		228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51,
		145, 235, 249, 14, 239,
		107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121,
		50, 45, 127, 4, 150, 254,
		138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66,
		215, 61, 156, 180
	};
}

/// @brief エンティティにアタッチされたときの処理
/// @param owner 所有エンティティ
void CameraAnimator::OnAttach(Entity& owner) { Component::OnAttach(owner); }

/// @brief 初期化
/// @param movementComponent MovementComponentへのポインタ
/// @param cameraRotator CameraRotatorへのポインタ
void CameraAnimator::Init(
	MovementComponent* movementComponent,
	CameraRotator*     cameraRotator
) {
	mMovement      = movementComponent;
	mCameraRotator = cameraRotator;
}

/// @brief パーリンノイズを計算する
/// @param x x座標
/// @param y y座標
/// @param z z座標
/// @return ノイズ値
float CameraAnimator::PerlinNoise(float x, float y, float z) const {
	int X = static_cast<int>(std::floor(x)) & 255;
	int Y = static_cast<int>(std::floor(y)) & 255;
	int Z = static_cast<int>(std::floor(z)) & 255;

	x -= std::floor(x);
	y -= std::floor(y);
	z -= std::floor(z);

	float u = Fade(x);
	float v = Fade(y);
	float w = Fade(z);

	int p[512];
	for (int i = 0; i < 256; i++) {
		p[i]       = kPermutation[i];
		p[256 + i] = kPermutation[i];
	}

	int A  = p[X] + Y;
	int AA = p[A] + Z;
	int AB = p[A + 1] + Z;
	int B  = p[X + 1] + Y;
	int BA = p[B] + Z;
	int BB = p[B + 1] + Z;

	return std::lerp(
		std::lerp(
			std::lerp(Grad(p[AA], x, y, z), Grad(p[BA], x - 1, y, z), u),
			std::lerp(
				Grad(p[AB], x, y - 1, z), Grad(p[BB], x - 1, y - 1, z),
				u
			),
			v
		),
		std::lerp(
			std::lerp(
				Grad(p[AA + 1], x, y, z - 1),
				Grad(p[BA + 1], x - 1, y, z - 1),
				u
			),
			std::lerp(
				Grad(p[AB + 1], x, y - 1, z - 1),
				Grad(p[BB + 1], x - 1, y - 1, z - 1), u
			),
			v
		),
		w
	);
}

/// @brief 物理演算前の更新（CameraRotatorの回転計算前にカメラ位置を反映する）
/// @param dt デルタタイム
void CameraAnimator::PrePhysics(float) {
	if (!mMovement || !mScene) return;

	// Vault中のカメラオフセットをCameraRotatorの回転計算前に反映する。
	// オフセット値自体はUpdate側で計算済みなので、ここでは適用のみ。
	if (mVaultCameraOffset.SqrLength() > 1e-8f) {
		mScene->SetLocalPos(mCurrentShake + mVaultCameraOffset);
	}
}

/// @brief 更新
/// @param dt デルタタイム
void CameraAnimator::Update(float dt) {
	if (!mMovement) return;

	UpdateJumpAnimation(dt);
	UpdateDoubleJumpAnimation(dt);
	UpdateSlideAnimation(dt);
	UpdateWallrunAnimation(dt);
	UpdateLandingAnimation(dt);
	UpdateBlinkAnimation(dt);
	UpdateVaultAnimation(dt);
	ApplyShakeAndTilt(dt);
}

/// @brief ジャンプアニメーションの更新
/// @param dt デルタタイム
void CameraAnimator::UpdateJumpAnimation(float dt) {
	if (!mMovement) return;

	const bool isInAir       = !mMovement->IsGrounded();
	const bool isWallRunning = mMovement->IsWallRunning();

	// ジャンプ開始検出（地上→空中、ウォールラン中を除く）
	if (isInAir && !mWasInAir && !isWallRunning) { mJumpAnimTime = 0.0f; }

	// ジャンプアニメーション進行
	if (mJumpAnimTime < kJumpShakeDuration) { mJumpAnimTime += dt; }

	mWasInAir = isInAir;
}

/// @brief ダブルジャンプアニメーションの更新
/// @param dt デルタタイム
void CameraAnimator::UpdateDoubleJumpAnimation(float dt) {
	if (!mMovement) return;

	const bool hasDoubleJump = mMovement->HasDoubleJump();

	// ダブルジャンプ使用検出（hasDoubleJump: true→false）
	if (!hasDoubleJump && mHadDoubleJump && !mMovement->IsGrounded()) {
		mDoubleJumpAnimTime = 0.0f;
	}

	// ダブルジャンプアニメーション進行
	if (mDoubleJumpAnimTime < kDoubleJumpShakeDuration) {
		mDoubleJumpAnimTime += dt;
	}

	mHadDoubleJump = hasDoubleJump;
}

/// @brief スライドアニメーションの更新
/// @param dt デルタタイム
void CameraAnimator::UpdateSlideAnimation(float dt) {
	if (!mMovement) return;

	const bool isSliding = mMovement->IsSliding();

	// スライディング開始検出
	if (isSliding && !mWasSliding) {
		mSlideAnimTime   = 0.0f;
		Vec3 vel         = mMovement->GetVelocity();
		vel.y            = 0;
		mSlideEntrySpeed = vel.Length();
	}

	// スライディング中はアニメーション時間を進める
	if (isSliding) { mSlideAnimTime += dt; } else if (mSlideAnimTime > 0.0f) {
		// スライディング終了後は徐々に減衰
		mSlideAnimTime = std::max(0.0f, mSlideAnimTime - dt * 3.0f);
	}

	mWasSliding = isSliding;
}

/// @brief ウォールランアニメーションの更新
/// @param dt デルタタイム
void CameraAnimator::UpdateWallrunAnimation(float dt) {
	if (!mMovement) return;

	const bool isWallRunning = mMovement->IsWallRunning();

	// ウォールラン開始検出
	if (isWallRunning && !mWasWallRunning) {
		mWallrunAnimTime = 0.0f;

		// 壁が左右どちらにあるか判定
		Vec3 wallNormal = mMovement->GetWallRunNormal();
		Vec3 velocity   = mMovement->GetVelocity();
		velocity.y      = 0;
		if (!velocity.IsZero()) {
			Vec3 right   = velocity.Cross(Vec3::up).Normalized();
			mWallrunSide = wallNormal.Dot(right) > 0 ? 1.0f : -1.0f;
		}
	}

	// ウォールラン中はアニメーション時間を進める
	if (isWallRunning) { mWallrunAnimTime += dt; } else if (
		mWallrunAnimTime > 0.0f) {
		// ウォールラン終了後は徐々に減衰
		mWallrunAnimTime = std::max(0.0f, mWallrunAnimTime - dt * 5.0f);
	}

	// ウォールラン中のカメラYawを進行方向に向ける
	if (mCameraRotator) {
		if (isWallRunning) {
			Vec3 wallRunDir = mMovement->GetWallRunDirection();
			wallRunDir.y = 0.0f;
			if (!wallRunDir.IsZero()) {
				wallRunDir.Normalize();
				// atan2でYaw角を計算（度数法）
				float targetYaw = std::atan2(wallRunDir.x, wallRunDir.z)
				                * Math::rad2Deg;
				mCameraRotator->SetWallrunYawTarget(targetYaw, true);
			}
		} else {
			mCameraRotator->SetWallrunYawTarget(0.0f, false);
		}
	}

	mWasWallRunning = isWallRunning;
}

/// @brief 着地アニメーションの更新
/// @param dt デルタタイム
void CameraAnimator::UpdateLandingAnimation(float dt) {
	if (!mMovement) return;

	// Movementコンポーネントから着地検出を取得（吸着処理を考慮した正確な検出）
	if (mMovement->JustLanded()) {
		// 着地時の垂直速度を取得
		float verticalSpeed = std::abs(mMovement->GetLastLandingVelocityY());

		// 落下速度が十分にある場合のみ着地アニメーション
		if (verticalSpeed > kLandingMinSpeed) {
			mLandingActive    = true;
			mLandingAnimTime  = 0.0f;
			mLandingIntensity = std::min(1.0f, verticalSpeed / 10.0f);
		}
	}

	// 着地アニメーション進行
	if (mLandingActive) {
		mLandingAnimTime += dt;
		if (mLandingAnimTime >= kLandingShakeDuration) {
			mLandingActive = false;
		}
	}
}

/// @brief ブリンクアニメーションの更新
/// @param dt デルタタイム
void CameraAnimator::UpdateBlinkAnimation(float dt) {
	if (!mMovement) { return; }

	if (mMovement->ConsumeBlinkTriggered()) {
		mBlinkActive = true;
		mBlinkAnimTime = 0.0f;
	}

	if (!mBlinkActive) {
		mBlinkFovOffset = 0.0f;
		return;
	}

	mBlinkAnimTime += dt;
	const float t = std::clamp(mBlinkAnimTime / kBlinkFovDuration, 0.0f, 1.0f);

	if (t < 0.5f) {
		mBlinkFovOffset = std::lerp(0.0f, kBlinkFovAmount, t * 2.0f);
	} else {
		mBlinkFovOffset = std::lerp(kBlinkFovAmount, 0.0f, (t - 0.5f) * 2.0f);
	}

	if (mBlinkAnimTime >= kBlinkFovDuration) {
		mBlinkActive = false;
		mBlinkFovOffset = 0.0f;
	}
}

/// @brief ヴォールトカメラアニメーションの更新
/// @param dt デルタタイム
/// @details ハルは着地位置に即テレポートするため、カメラのみベジェ曲線で
///          開始位置から着地位置へスムーズに補間する
void CameraAnimator::UpdateVaultAnimation(float dt) {
	if (!mMovement) {
		mVaultCameraOffset = Vec3::zero;
		return;
	}

	if (mMovement->IsSpeedVaulting()) {
		const float t = mMovement->GetVaultProgress();

		// ベジェ曲線上のカメラがあるべきワールド位置を計算
		const Vec3 startPos = mMovement->GetVaultStartPos();
		const Vec3 apexPos  = mMovement->GetVaultApexPos();
		const Vec3 endPos   = mMovement->GetVaultEndPos();

		const float omt = 1.0f - t;
		Vec3 bezierPos = startPos * (omt * omt)
		               + apexPos  * (2.0f * omt * t)
		               + endPos   * (t * t);

		// カメラの目標ワールド位置はベジェ曲線上の位置 + ヘッドオフセット
		Vec3 headOffset = mMovement->GetHeadPos() - mMovement->GetOwner()->GetTransform()->GetWorldPos();
		Vec3 desiredWorldPos = bezierPos + headOffset;

		// cameraRootの現在のワールド位置との差をローカルオフセットに変換
		if (mScene && mOwner && mOwner->GetParent()) {
			if (const auto* parentTransform = mOwner->GetParent()->GetTransform()) {
				Vec3 cameraRootWorldPos = parentTransform->GetWorldPos();
				Vec3 worldOffset = desiredWorldPos - cameraRootWorldPos;
				Quaternion parentWorldRot = parentTransform->GetWorldRot();
				mVaultCameraOffset = parentWorldRot.Inverse().RotateVector(worldOffset);
			}
		}
	} else {
		// Vault終了後はオフセットをスムーズに0に戻す
		const float decay = 1.0f - std::exp(-dt * 15.0f);
		mVaultCameraOffset = mVaultCameraOffset * (1.0f - decay);

		// 十分小さくなったらゼロに
		if (mVaultCameraOffset.SqrLength() < 1e-8f) {
			mVaultCameraOffset = Vec3::zero;
		}
	}
}

/// @brief シェイクと傾きを適用する
/// @param dt デルタタイム
void CameraAnimator::ApplyShakeAndTilt(float dt) {
	if (!mScene) return;

	// ノイズ時間を更新
	mNoiseTime += dt;

	Vec3  shake = Vec3::zero;
	float roll  = 0.0f;
	float pitch = 0.0f;

	// === ジャンプシェイク＆ピッチ（パーリンノイズベース） ===
	if (mJumpAnimTime < kJumpShakeDuration) {
		float t         = mJumpAnimTime / kJumpShakeDuration;
		float intensity = (1.0f - t) * kJumpShakeAmount;

		// パーリンノイズを使用した自然なシェイク
		float noiseX = PerlinNoise(mNoiseTime * 10.0f, 0.0f, 0.0f);
		float noiseY = PerlinNoise(0.0f, mNoiseTime * 10.0f, 0.0f);

		shake.x += noiseX * intensity;
		shake.y += noiseY * intensity;

		// ジャンプ時のピッチ（上を向く）
		// 最初の30%で上を向き、残り70%で戻る
		float pitchT = t * 3.33f; // 0.3秒で1.0になる
		if (pitchT < 1.0f) {
			// 上を向く（ピッチ減少）
			pitch -= kJumpPitchAmount * pitchT;
		} else {
			// 戻る（ピッチ増加）
			float returnT = (pitchT - 1.0f) / 2.33f; // 残りの時間で正規化
			pitch         -= kJumpPitchAmount * (1.0f - returnT);
		}
	}

	// === ダブルジャンプシェイク＆ピッチ（パーリンノイズベース） ===
	if (mDoubleJumpAnimTime < kDoubleJumpShakeDuration) {
		float t         = mDoubleJumpAnimTime / kDoubleJumpShakeDuration;
		float intensity = (1.0f - t) * kDoubleJumpShakeAmount;

		// より速いノイズで強いシェイク
		float noiseX = PerlinNoise(mNoiseTime * 15.0f, 0.0f, 0.0f);
		float noiseY = PerlinNoise(0.0f, mNoiseTime * 15.0f, 0.0f);
		float noiseZ = PerlinNoise(0.0f, 0.0f, mNoiseTime * 12.0f);

		shake.x += noiseX * intensity;
		shake.y += noiseY * intensity;
		shake.z += noiseZ * intensity * 0.5f;

		// ダブルジャンプ時のピッチ（さらに上を向く）
		// 最初の40%で上を向き、残り60%で戻る
		float pitchT = t * 2.5f; // 0.4秒で1.0になる
		if (pitchT < 1.0f) {
			// 上を向く（ピッチ減少）
			pitch -= kDoubleJumpPitchAmount * pitchT;
		} else {
			// 戻る（ピッチ増加）
			float returnT = (pitchT - 1.0f) / 1.5f; // 残りの時間で正規化
			pitch         -= kDoubleJumpPitchAmount * (1.0f - returnT);
		}
	}

	// === スライディング傾き＆シェイク（パーリンノイズベース） ===
	if (mSlideAnimTime > 0.0f) {
		// スライド中の傾き（進行方向に合わせた微妙な傾き）
		float slideT     = std::min(1.0f, mSlideAnimTime * 2.0f);
		float targetRoll = kSlideRollAmount * slideT;
		roll             += targetRoll;

		// スライド中の振動（速度に応じて、パーリンノイズ使用）
		float speedFactor = std::min(1.0f, mSlideEntrySpeed / 10.0f);
		float intensity   = kSlideShakeAmount * speedFactor;

		float noiseX = PerlinNoise(mNoiseTime * 20.0f, 0.0f, 0.0f);
		float noiseY = PerlinNoise(0.0f, mNoiseTime * 20.0f, 0.0f);

		shake.x += noiseX * intensity;
		shake.y += noiseY * intensity * 0.5f;
	}

	// ウォールラン傾き＆シェイク
	if (mWallrunAnimTime > 0.0f) {
		// ウォールラン中の傾き
		roll = std::lerp(
			roll,
			kWallrunRollAmount * mWallrunSide,
			dt * kWallrunRollSpeed
		);

		// ウォールラン中の振動
		float intensity = kWallrunShakeAmount;

		float noiseX = PerlinNoise(mNoiseTime * 12.0f, 0.0f, 0.0f);
		float noiseY = PerlinNoise(0.0f, mNoiseTime * 12.0f, 0.0f);

		shake.x += noiseX * intensity * mWallrunSide;
		shake.y += noiseY * intensity;
	}

	// === 着地シェイク＆ピッチ（パーリンノイズベース） ===
	if (mLandingActive) {
		float t         = mLandingAnimTime / kLandingShakeDuration;
		float intensity = (1.0f - t) * kLandingShakeAmount * mLandingIntensity;

		// パーリンノイズベースの着地シェイク
		float noiseX = PerlinNoise(mNoiseTime * 25.0f, 0.0f, 0.0f);
		float noiseY = PerlinNoise(0.0f, mNoiseTime * 25.0f, 0.0f);

		// 着地時は下方向へのバイアスをかけたシェイク
		shake.x += noiseX * intensity * 0.5f;
		shake.y -= intensity * (1.0f - t * 0.5f) + noiseY * intensity * 0.3f;

		// 着地時のピッチ（下を向いて戻る）
		// 最初の50%で下を向き、残り50%で戻る
		float pitchT = t * 2.0f;
		if (pitchT < 1.0f) {
			// 下を向く（ピッチ増加）
			pitch += kLandingPitchAmount * pitchT * mLandingIntensity;
		} else {
			// 戻る（ピッチ減少）
			pitch += kLandingPitchAmount * (2.0f - pitchT) * mLandingIntensity;
		}
	}

	// === ブリンクシェイク ===
	if (mBlinkActive) {
		const float t = std::clamp(mBlinkAnimTime / kBlinkFovDuration, 0.0f, 1.0f);
		const float intensity = (1.0f - t) * kBlinkShakeAmount;
		const float noiseX = PerlinNoise(mNoiseTime * 18.0f, 1.0f, 0.0f);
		const float noiseY = PerlinNoise(1.0f, mNoiseTime * 18.0f, 0.0f);
		shake.x += noiseX * intensity;
		shake.y += noiseY * intensity;
	}

	// === シェイク、ロール、ピッチを適用 ===
	// 現在の値にスムーズに補間
	float smoothFactor = 1.0f - std::exp(-dt * 10.0f);
	mCurrentShake      = mCurrentShake * (1.0f - smoothFactor) + shake *
	                     smoothFactor;
	mCurrentRoll  = mCurrentRoll * (1.0f - smoothFactor) + roll * smoothFactor;
	mCurrentPitch = mCurrentPitch * (1.0f - smoothFactor) + pitch *
	                smoothFactor;

	// トランスフォームに適用（シェイクのみ、回転はCameraRotatorに委譲）
	auto* transform = mScene;
	if (transform) {
		// シェイク（位置オフセット）+ ヴォールトカメラオフセット
		transform->SetLocalPos(mCurrentShake + mVaultCameraOffset);
	}

	// CameraRotatorにピッチとロールのオフセットを渡す
	if (mCameraRotator) {
		mCameraRotator->SetAnimationPitchOffset(mCurrentPitch);
		mCameraRotator->SetAnimationRollOffset(mCurrentRoll);
	}
}

/// @brief ImGuiでインスペクターを描画する（デバッグ用）
void CameraAnimator::DrawInspectorImGui() {
#ifdef _DEBUG
	ImGui::Text("Camera Animator");
	ImGui::Separator();

	ImGui::Text("Jump Anim Time: %.2f", mJumpAnimTime);
	ImGui::Text("Double Jump Anim Time: %.2f", mDoubleJumpAnimTime);
	ImGui::Text("Slide Anim Time: %.2f", mSlideAnimTime);
	ImGui::Text("Wallrun Anim Time: %.2f", mWallrunAnimTime);
	ImGui::Text("Wallrun Side: %.1f", mWallrunSide);
	ImGui::Text("Landing Active: %s", mLandingActive ? "Yes" : "No");
	ImGui::Separator();
	ImGui::Text(
		"Current Shake: %.3f, %.3f, %.3f", mCurrentShake.x,
		mCurrentShake.y, mCurrentShake.z
	);
	ImGui::Text("Current Roll: %.2f°", mCurrentRoll);
	ImGui::Text("Current Pitch: %.2f°", mCurrentPitch);
#endif
}
