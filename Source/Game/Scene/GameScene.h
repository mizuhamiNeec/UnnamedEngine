#pragma once
#include <memory>
#include <Components/PlayerMovement.h>
#include <Components/ColliderComponent/BoxColliderComponent.h>
#include <Components/ColliderComponent/MeshColliderComponent.h>
#include <Components/MeshRenderer/StaticMeshRenderer.h>
#include <Components/Weapon/WeaponSway.h>
#include <CubeMap/CubeMap.h>
#include <Entity/Base/Entity.h>
#include <Object3D/Object3D.h>
#include <Object3D/Object3DCommon.h>
#include <Particle/ExplosionEffect.h>
#include <Particle/ParticleEmitter.h>
#include <Particle/ParticleObject.h>
#include <Particle/WindEffect.h>
#include <Physics/Physics.h>
#include <Physics/PhysicsEngine.h>
#include <Renderer/Renderer.h>
#include <Scene/Base/BaseScene.h>
#include <Sprite/Sprite.h>
#include <Sprite/SpriteCommon.h>

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

	std::unique_ptr<Entity> mCameraRoot;
	CameraRotator*          mCameraRotator = nullptr;

	std::unique_ptr<Entity>          mCamera;
	std::shared_ptr<CameraComponent> mCameraComponent;

	std::unique_ptr<Entity>             mEntWorldMesh;
	std::shared_ptr<StaticMeshRenderer> mWorldMeshRenderer;

	std::unique_ptr<Entity>               mEntPlayer;
	std::shared_ptr<PlayerMovement>       mPlayerMovement;
	std::shared_ptr<BoxColliderComponent> mPlayerCollider;

	std::unique_ptr<Entity>             mEntWeapon;
	std::shared_ptr<WeaponComponent>    mWeaponComponent;
	std::shared_ptr<StaticMeshRenderer> mWeaponMeshRenderer;
	std::shared_ptr<WeaponSway>         mWeaponSway;

	std::unique_ptr<Entity> mEntShakeRoot;

	// テレポートトリガー用のAABB
	Vec3 mTeleportTriggerMin;    // ボックスの最小点
	Vec3 mTeleportTriggerMax;    // ボックスの最大点
	bool mTeleportActive = true; // テレポートの有効/無効状態

	std::unique_ptr<EntityLoader> mEntityLoader;

	std::unique_ptr<PhysicsEngine> mPhysicsEngine;

	std::unique_ptr<ParticleEmitter> mParticleEmitter;

	std::unique_ptr<ParticleObject> mParticleObject;

	std::unique_ptr<WindEffect>      mWindEffect;
	std::unique_ptr<ExplosionEffect> mExplosionEffect;

	// 遅延読み込み用フラグ
	bool mPendingMeshReload = false;
};
