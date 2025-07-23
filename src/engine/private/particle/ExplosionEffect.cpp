#include <engine/public/particle/ExplosionEffect.h>

#include <math/public/Vec3.h>
#include <math/public/random/Random.h>

ExplosionEffect::~ExplosionEffect() {
	if (mExplosionParticleObject) {
		mExplosionParticleObject->Shutdown();
		mExplosionParticleObject.reset();
	}
}

void ExplosionEffect::Init(ParticleManager*   particleManager,
                           const std::string& texturePath) {
	mExplosionParticleObject = std::make_unique<ParticleObject>();
	mExplosionParticleObject->Init(particleManager, texturePath);
	mExplosionParticleObject->SetBillboardType(BillboardType::XZ);
}

void ExplosionEffect::TriggerExplosion(const Vec3& position,
                                       const Vec3& normal,
                                       uint32_t    particleCount,
                                       float       coneAngle) {
	for (uint32_t i = 0; i < particleCount; ++i) {
		Vec3 randomDir = Random::Vec3Range(-Vec3::one, Vec3::one);
		randomDir.Normalize();
		float randomSpeed = Random::FloatRange(5.0f, 15.0f);
		Vec3  velocity    = normal + randomDir * randomSpeed;

		mExplosionParticleObject->EmitParticlesAtPosition(
			position,
			0,
			coneAngle,
			Vec3::one * 10.0f,
			Vec3::down * 9.81f,
			velocity,
			1, mStartColor, mEndColor, Vec3::one * 4.0f, Vec3::zero
		);
	}
}

void ExplosionEffect::Update(float deltaTime) const {
	if (mExplosionParticleObject) {
		mExplosionParticleObject->Update(deltaTime);
	}
}

void ExplosionEffect::Draw() const {
	if (mExplosionParticleObject) {
		mExplosionParticleObject->Draw();
	}
}
