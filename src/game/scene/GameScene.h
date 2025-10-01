#pragma once
#include <memory>

#include <engine/public/Components/Camera/CameraComponent.h>
#include <engine/public/Components/ColliderComponent/BoxColliderComponent.h>
#include <engine/public/Components/MeshRenderer/SkeletalMeshRenderer.h>
#include <engine/public/Components/MeshRenderer/StaticMeshRenderer.h>
#include <engine/public/CubeMap/CubeMap.h>
#include <engine/public/Entity/Entity.h>
#include <engine/public/Entity/EntityLoader.h>
#include <engine/public/particle/ExplosionEffect.h>
#include <engine/public/particle/ParticleEmitter.h>
#include <engine/public/particle/ParticleObject.h>
#include <engine/public/particle/WindEffect.h>
#include <runtime/physics/core/UPhysics.h>

#include "base/BaseScene.h"

#include "game/components/GameMovementComponent.h"
#include "game/components/weapon/WeaponSway.h"
#include "game/components/weapon/base/WeaponComponent.h"

class D3D12;
class EnemyMovement;
class CameraRotator;
class CameraSystem;

class GameScene : public BaseScene {
public:
	~GameScene() override;
	void Init() override;
	void Update(float deltaTime) override;
	void Render() override;
	void Shutdown() override;

	// ホットリロード機能
	void ReloadWorldMesh();
	void RecreateWorldMeshEntity();
	void SafeReloadWorldMesh();

private:
	D3D12* mRenderer = nullptr;

	std::unique_ptr<CubeMap> mCubeMap;

	std::unique_ptr<Entity> mEntCameraRoot;
	CameraRotator*          mCameraRotator = nullptr;

	std::unique_ptr<Entity>          mCamera;
	std::shared_ptr<CameraComponent> mCameraComponent;

	std::unique_ptr<Entity>             mEntWorldMesh;
	std::shared_ptr<StaticMeshRenderer> mWorldMeshRenderer;

	std::unique_ptr<Entity>                mEntPlayer;
	std::shared_ptr<GameMovementComponent> mGameMovementComponent;
	std::shared_ptr<BoxColliderComponent>  mPlayerCollider;

	std::unique_ptr<Entity>             mEntWeapon;
	std::shared_ptr<WeaponComponent>    mWeaponComponent;
	std::shared_ptr<StaticMeshRenderer> mWeaponMeshRenderer;
	std::shared_ptr<WeaponSway>         mWeaponSway;

	std::unique_ptr<Entity> mEntShakeRoot;

	std::unique_ptr<Entity>               mEntSkeletalMesh;
	std::shared_ptr<SkeletalMeshRenderer> mSkeletalMeshRenderer;

	// テレポートトリガー用のAABB
	Vec3 mTeleportTriggerMin;    // ボックスの最小点
	Vec3 mTeleportTriggerMax;    // ボックスの最大点
	bool mTeleportActive = true; // テレポートの有効/無効状態

	std::unique_ptr<EntityLoader> mEntityLoader;

	//std::unique_ptr<PhysicsEngine> mPhysicsEngine;
	std::unique_ptr<UPhysics::Engine> mUPhysicsEngine;

	std::unique_ptr<ParticleEmitter> mParticleEmitter;

	std::unique_ptr<ParticleObject> mParticleObject;

	std::unique_ptr<WindEffect>      mWindEffect;
	std::unique_ptr<ExplosionEffect> mExplosionEffect;

	// 遅延読み込み用フラグ
	bool mPendingMeshReload = false;
};
