#include <engine/particle/ParticleEmitter.h>

#include "engine/particle/ParticleManager.h"

/// @brief パーティクルエミッターを初期化します
/// @param manager パーティクルマネージャーへのポインタ
/// @param groupName パーティクルグループ名
void ParticleEmitter::Init(ParticleManager*   manager,
                           const std::string& groupName) {
	mParticleManager = manager;
	mGroupName       = groupName;
}

/// @brief パーティクルエミッターを更新します
/// @param deltaTime 前のフレームからの経過時間
void ParticleEmitter::Update(float deltaTime) {
	// 時刻を進める
	mEmitter.frequencyTime += deltaTime;
	// 発生頻度より大きいなら発生
	if (mEmitter.frequency <= mEmitter.frequencyTime) {
		// パーティクルを発生させる
		mParticleManager->Emit(mGroupName, Vec3::zero, 1);
		// 余計に過ぎた時間も加味して頻度計算する
		mEmitter.frequencyTime -= mEmitter.frequency;
	}
}

/// @brief パーティクルを即座に発生させます
void ParticleEmitter::Emit() {
	// エミッターの設定値に基づいてパーティクルを発生させる
	mParticleManager->Emit(mGroupName, Vec3::zero, 1);
}
