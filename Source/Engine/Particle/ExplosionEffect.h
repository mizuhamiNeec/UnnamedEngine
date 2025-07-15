#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <Lib/Math/Vector/Vec3.h>
#include <Particle/ParticleObject.h>

class ParticleManager;

class ExplosionEffect {
public:
	~ExplosionEffect();
	void Init(ParticleManager* particleManager, const std::string& texturePath);

	void SetColorGradient(const Vec4& startColor, const Vec4& endColor) {
		mStartColor = startColor;
		mEndColor   = endColor;
	}

	void TriggerExplosion(const Vec3& position, const Vec3& normal,
	                      uint32_t particleCount = 50, float coneAngle = 180.0f);
	void Update(float deltaTime);
	void Draw() const;

private:
	Vec4 mStartColor;
	Vec4 mEndColor;

	std::unique_ptr<ParticleObject> mExplosionParticleObject;
};
