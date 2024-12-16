#include "ParticleEmitter.h"

#include "ParticleManager.h"

void ParticleEmitter::Init(ParticleManager* manager, const std::string& groupName) {
	particleManager_ = manager;
	groupName_ = groupName;
}

void ParticleEmitter::Update(float deltaTime) {
	// 時刻を進める
	emitter_.frequencyTime += deltaTime;
	// 発生頻度より大きいなら発生
	if (emitter_.frequency <= emitter_.frequencyTime) {
		// パーティクルを発生させる
		particleManager_->Emit(groupName_, Vec3::zero, 1);
		// 余計に過ぎた時間も加味して頻度計算する
		emitter_.frequencyTime -= emitter_.frequency;
	}
}

void ParticleEmitter::Emit() {
	// エミッターの設定値に基づいてパーティクルを発生させる
	particleManager_->Emit(groupName_, Vec3::zero, 1);
}
