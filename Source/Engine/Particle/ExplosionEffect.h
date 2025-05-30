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
		startColor_ = startColor;
		endColor_   = endColor;
	}

	void TriggerExplosion(const Vec3& position, uint32_t particleCount = 50,
	                      float       coneAngle = 180.0f);
	void Update(float deltaTime);
	void Draw() const;

private:
	Vec4 startColor_;
	Vec4 endColor_;

	std::unique_ptr<ParticleObject> explosionParticleObject;
};
