#pragma once

#include <memory>

#include <engine/particle/ParticleObject.h>

#include <game/components/player/MovementComponent.h>

class WindEffect {
public:
	~WindEffect();

	void Init(ParticleManager* particleManager,
	          MovementComponent*        playerMovement);
	void Update(float deltaTime);
	void Draw() const;

private:
	std::unique_ptr<ParticleObject> mWindParticle;
	MovementComponent*                       mPlayerMovement = nullptr;

	// エフェクトのパラメータ
	float mMinEmissionRate = 0.0f;        // 最小エミッション率
	float mMaxEmissionRate = 10000000.0f; // 最大エミッション率
	float mMinSpeed        = 10.0f;       // 最小スピード (HU)
	float mMaxSpeed        = 40.0f;       // 最大スピード (HU)
	float mEmissionTimer   = 0.0f;        // エミッションタイマー
	float mSpeedThreshold  = 10.0f;       // 風エフェクトが始まる速度閾値 (HU)
	float mMaxEffectSpeed  = 3000.0f;     // 最大効果が現れる速度 (HU)

	Vec3 GetRandomPositionInPlayerDirection() const;
};
