#include "engine/public/particle/WindEffect.h"

#include "engine/public/Camera/CameraManager.h"
#include "engine/public/Components/Camera/CameraComponent.h"
#include "engine/public/Debug/Debug.h"
#include "engine/public/Entity/Entity.h"
#include "engine/public/particle/ParticleManager.h"
#include "engine/public/particle/ParticleObject.h"

#include "game/public/components/GameMovementComponent.h"
#include "game/public/components/PlayerMovement.h"
#include "game/public/components/PlayerMovementUPhysics.h"

#include "math/public/random/Random.h"

WindEffect::~WindEffect() {
	if (mWindParticle) {
		mWindParticle->Shutdown();
		mWindParticle.reset();
	}
}

void WindEffect::Init([[maybe_unused]] ParticleManager* particleManager,
                      GameMovementComponent*            playerMovement) {
	mPlayerMovement = playerMovement;

	// 風パーティクル用のオブジェクト初期化
	mWindParticle = std::make_unique<ParticleObject>();
	mWindParticle->Init(particleManager, "./resources/textures/circle.png");
}

void WindEffect::Update([[maybe_unused]] const float deltaTime) {
	if (!mPlayerMovement || !mWindParticle) {
		return;
	}

	// プレイヤーの速度ベクトル
	const Vec3  playerVelocity = mPlayerMovement->Velocity();
	const float playerSpeed    = playerVelocity.Length();

	// 速度が閾値を超えている場合のみエフェクトを表示
	if (playerSpeed > mSpeedThreshold) {
		// 速度に基づいてエミッション率を計算
		const float speedFactor = (std::min)(
			(playerSpeed - mSpeedThreshold) / (mMaxEffectSpeed -
				mSpeedThreshold), 1.0f);
		const float currentEmissionRate = mMinEmissionRate + speedFactor * (
			mMaxEmissionRate - mMinEmissionRate);

		// エミッションタイマーを更新
		mEmissionTimer += deltaTime;

		// 次のエミッションまでの時間を計算

		// 時間が経過したらパーティクルを放出
		if (float emissionInterval = 1.0f / currentEmissionRate; mEmissionTimer
			>= emissionInterval) {
			mEmissionTimer = 0.0f;

			// 進行方向から来るように位置生成
			const Vec3 spawnPosition = GetRandomPositionInPlayerDirection();

			// パーティクルの速度をプレイヤーの進行方向に合わせる（見た目上、進行方向から"風が来る"状態）
			const Vec3 particleVelocity = -playerVelocity.Normalized();

			// 速度に応じてパーティクルの速さを変更
			const float particleSpeed = mMinSpeed + speedFactor * (mMaxSpeed -
				mMinSpeed);

			// 放出するパーティクルの数（速度が速いほど多く）
			const uint32_t particleCount = static_cast<uint32_t>(1 + speedFactor
				* 3);

			// パーティクルを放出
			mWindParticle->EmitParticlesAtPosition(
				spawnPosition,                    // 位置
				0,                                // シェイプタイプ（球体）
				30.0f,                            // コーン角度
				Vec3(0.1f, 0.1f, 0.1f),           // 抵抗
				Vec3::zero,                       // 重力なし
				particleVelocity * particleSpeed, // 速度
				particleCount,                    // 放出数
				Vec4(1.0f, 1.0f, 1.0f, 1.0f),
				Vec4::cyan,
				Vec3::one * 0.5f,
				Vec3::zero
			);
		}
	}

	// パーティクルの更新
	mWindParticle->Update(deltaTime);
}

void WindEffect::Draw() const {
	if (mWindParticle) {
		mWindParticle->Draw();
	}
}

Vec3 WindEffect::GetRandomPositionInPlayerDirection() const {
	auto camera = CameraManager::GetActiveCamera();
	if (!camera || !mPlayerMovement) {
		return Vec3::zero;
	}

	// アスペクト比取得
	const float aspectRatio = camera->GetAspectRatio();

	// プレイヤー位置
	const Vec3 playerPos = mPlayerMovement->GetOwner()->GetTransform()->
	                                        GetWorldPos();

	// 進行方向ベクトル
	Vec3 moveDir = mPlayerMovement->Velocity().Normalized();
	if (moveDir.Length() < 0.01f) {
		moveDir = camera->GetViewMat().Inverse().GetForward(); // ほぼ静止時はカメラ前方でも
	}

	// 進行方向に直交するベクトルを算出
	const Vec3 right = Vec3::up.Cross(moveDir).Normalized();
	const Vec3 up    = moveDir.Cross(right).Normalized();

	// ランダムな位置を進行方向側に生成
	const float randomAngle = Random::FloatRange(
		0.0f, std::numbers::pi_v<float> * 2.0f);
	const float randomDistance = Random::FloatRange(6.0f, 24.0f);
	const float randomHeight   = Random::FloatRange(-4.0f, 8.0f);

	// 横方向はアスペクト比に対応
	const float horizontalScale = aspectRatio;

	const Vec3 randomPos = playerPos +
		moveDir * randomDistance +
		right * std::cos(randomAngle) * randomDistance * 0.5f * horizontalScale
		+
		up * std::sin(randomAngle) * randomDistance * 0.5f +
		up * randomHeight;

	Debug::DrawAxis(randomPos, Quaternion::identity);

	return randomPos;
}
