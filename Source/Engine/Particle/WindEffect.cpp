#include "WindEffect.h"
#include <Camera/CameraManager.h>
#include <Lib/Math/Random/Random.h>

void WindEffect::Init(ParticleManager* particleManager, PlayerMovement* playerMovement) {
	mPlayerMovement = playerMovement;

	// 風パーティクル用のオブジェクト初期化
	mWindParticle = std::make_unique<ParticleObject>();
	mWindParticle->Init(particleManager, "./Resources/Textures/circle.png");
}

void WindEffect::Update(float deltaTime) {
	if (!mPlayerMovement || !mWindParticle) {
		return;
	}

	// プレイヤーの速度を取得（ヒートユニット単位）
	float playerSpeed = mPlayerMovement->GetVelocity().Length();

	// 速度が閾値を超えている場合のみエフェクトを表示
	if (playerSpeed > mSpeedThreshold) {
		// 速度に基づいてエミッション率を計算
		float speedFactor = (std::min)((playerSpeed - mSpeedThreshold) / (mMaxEffectSpeed - mSpeedThreshold), 1.0f);
		float currentEmissionRate = mMinEmissionRate + speedFactor * (mMaxEmissionRate - mMinEmissionRate);

		// エミッションタイマーを更新
		mEmissionTimer += deltaTime;

		// 次のエミッションまでの時間を計算
		float emissionInterval = 1.0f / currentEmissionRate;

		// 時間が経過したらパーティクルを放出
		if (mEmissionTimer >= emissionInterval) {
			mEmissionTimer = 0.0f;

			// カメラの周りにランダムな位置を生成

			auto camera = CameraManager::GetActiveCamera();
			Vec3 cameraPos = camera->GetViewMat().Inverse().GetTranslate();

			// カメラの前方向を取得
			Vec3 cameraForward = camera->GetViewMat().Inverse().GetForward();
			Vec3 spawnPosition = GetRandomPositionAroundCamera();
			//Vec3 spawnPosition = cameraPos + cameraForward * 100.0f;

			// プレイヤーの進行方向と逆向きにパーティクルを放出
			Vec3 particleVelocity = -Math::HtoM(cameraForward).Normalized();

			// 速度に応じてパーティクルの速さを変更
			float particleSpeed = mMinSpeed + speedFactor * (mMaxSpeed - mMinSpeed);

			// 放出するパーティクルの数（速度が速いほど多く）
			uint32_t particleCount = static_cast<uint32_t>(1 + speedFactor * 3);

			// パーティクルを放出
			mWindParticle->EmitParticlesAtPosition(
				spawnPosition,                       // 位置
				0,                                   // シェイプタイプ（球体）
				30.0f,                               // コーン角度
				Vec3(0.1f, 0.1f, 0.1f),             // 抵抗
				Vec3::zero,                          // 重力なし
				particleVelocity * particleSpeed, // 速度
				particleCount // 放出数
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

Vec3 WindEffect::GetRandomPositionAroundCamera() const {
	auto camera = CameraManager::GetActiveCamera();
	if (!camera) {
		return Vec3::zero;
	}

	// カメラの位置を取得
	Vec3 cameraPos = camera->GetViewMat().Inverse().GetTranslate();

	// カメラの前方向を取得
	Vec3 cameraForward = camera->GetViewMat().Inverse().GetForward();
	Vec3 cameraRight = camera->GetViewMat().Inverse().GetRight();
	Vec3 cameraUp = camera->GetViewMat().Inverse().GetUp();

	// ランダムな位置をカメラの視野内に生成
	float randomAngle = Random::FloatRange(0.0f, std::numbers::pi_v<float> *2.0f);
	float randomDistance = Random::FloatRange(6.0f, 24.0f);
	float randomHeight = Random::FloatRange(-1.0f, 1.0f);

	// カメラの前方向を基準に円周上の位置を計算
	Vec3 randomPos = cameraPos +
		cameraForward * randomDistance +
		cameraRight * std::cos(randomAngle) * randomDistance * 0.5f +
		cameraUp * std::sin(randomAngle) * randomDistance * 0.5f +
		cameraUp * randomHeight;

	return randomPos;
}
