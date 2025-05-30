#include "ExplosionEffect.h"

#include "Lib/Math/Random/Random.h"

ExplosionEffect::~ExplosionEffect() {
	if (explosionParticleObject) {
		explosionParticleObject->Shutdown();
		explosionParticleObject.reset();
	}
}

void ExplosionEffect::Init(ParticleManager*   particleManager,
                           const std::string& texturePath) {
	explosionParticleObject = std::make_unique<ParticleObject>();
	explosionParticleObject->Init(particleManager, texturePath);
	explosionParticleObject->SetBillboardType(BillboardType::XZ);
}

void ExplosionEffect::TriggerExplosion(const Vec3& position,
                                       uint32_t    particleCount,
                                       float       coneAngle) {
	for (uint32_t i = 0; i < particleCount; ++i) {
		Vec3 randomDir = Random::Vec3Range(-Vec3::one, Vec3::one);
		randomDir.Normalize();
		float randomSpeed = Random::FloatRange(5.0f, 15.0f);
		Vec3  velocity    = randomDir * randomSpeed;

		explosionParticleObject->EmitParticlesAtPosition(
			position,
			0,
			coneAngle,
			Vec3::one * 0.1f,
			Vec3::down * -9.81f,
			velocity,
			1, startColor_, endColor_
		);
	}
}

void ExplosionEffect::Update(float deltaTime) {
	if (explosionParticleObject) {
		explosionParticleObject->Update(deltaTime);
	}
}

void ExplosionEffect::Draw() const {
	if (explosionParticleObject) {
		explosionParticleObject->Draw();
	}
}
