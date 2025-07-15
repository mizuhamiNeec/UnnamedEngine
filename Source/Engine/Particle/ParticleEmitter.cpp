#include "ParticleEmitter.h"

#include "ParticleManager.h"

void ParticleEmitter::Init(ParticleManager* manager, const std::string& groupName) {
	mParticleManager = manager;
	mGroupName = groupName;
}

void ParticleEmitter::Update(float deltaTime) {
	// 時刻を進める
	mEmitter.frequencyTime += deltaTime;
	// 発生頻度より大きいなら発生
	if (mEmitter.frequency <= mEmitter.frequencyTime)
	{
		// パーティクルを発生させる
		mParticleManager->Emit(mGroupName, Vec3::zero, 1);
		// 余計に過ぎた時間も加味して頻度計算する
		mEmitter.frequencyTime -= mEmitter.frequency;
	}
}

void ParticleEmitter::Emit() {
	// エミッターの設定値に基づいてパーティクルを発生させる
	mParticleManager->Emit(mGroupName, Vec3::zero, 1);
}
