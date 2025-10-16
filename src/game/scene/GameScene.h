#pragma once
#include <memory>

#include <engine/Components/MeshRenderer/SkeletalMeshRenderer.h>
#include <engine/Components/MeshRenderer/StaticMeshRenderer.h>
#include <engine/CubeMap/CubeMap.h>
#include <engine/Entity/Entity.h>
#include <engine/particle/ExplosionEffect.h>
#include <engine/particle/ParticleEmitter.h>
#include <engine/particle/ParticleObject.h>
#include <engine/particle/WindEffect.h>
#include <runtime/physics/core/UPhysics.h>

#include <game/components/weapon/WeaponSway.h>
#include <game/components/weapon/base/WeaponComponent.h>
#include <game/scene/base/BaseScene.h>
#include <game/components/player/MovementComponent.h>
#include <game/components/CameraAnimator.h>

class D3D12;
class EnemyMovement;
class CameraRotator;
class CameraSystem;
class CameraComponent;
class IConVar;

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
	void RegisterConVars();
	void LoadCoreTextures() const;
	void InitializeCubeMap();
	void InitializeParticles();
	void InitializeEffects();
	void InitializePhysics();
	void InitializeCamera();
	void InitializePlayer();
	void InitializeWeapon();
	void InitializeWorldMesh();
	void InitializeCameraRoot();
	void InitializeShakeRoot();
	void InitializeSkeletalMesh();
	void ConfigureEntityHierarchy();
	void ConfigureConsole();
	void InitializeTeleportTrigger();

	void HandleMeshReload();
	void SyncCameraRoot() const;
	void HandleWeaponInput();
	void HandleWeaponFire(const std::shared_ptr<CameraComponent>& camera);
	void UpdateSkeletalAnimation();
	void UpdatePostProcessing(float deltaTime);
	void UpdateTeleport();
	void UpdateParticlesAndEffects(float deltaTime);
	void UpdateEntities(float deltaTime);
#ifdef _DEBUG
	void DrawDebugHud(const std::shared_ptr<CameraComponent>& camera) const;
#endif

private:
	D3D12* mRenderer = nullptr;

	std::unique_ptr<CubeMap> mCubeMap;

	std::unique_ptr<Entity> mEntCameraRoot;
	CameraRotator*          mCameraRotator = nullptr;

	std::unique_ptr<Entity> mCamera;

	std::unique_ptr<Entity>             mEntWorldMesh;
	std::shared_ptr<StaticMeshRenderer> mWorldMeshRenderer;

	std::unique_ptr<Entity>   mEntPlayer;
	std::shared_ptr<MovementComponent> mMovementComponent;

	std::unique_ptr<Entity>             mEntWeapon;
	std::shared_ptr<WeaponComponent>    mWeaponComponent;
	std::shared_ptr<StaticMeshRenderer> mWeaponMeshRenderer;
	std::shared_ptr<WeaponSway>         mWeaponSway;

	std::unique_ptr<Entity>         mEntShakeRoot;
	std::shared_ptr<CameraAnimator> mCameraAnimator;

	std::unique_ptr<Entity>               mEntSkeletalMesh;
	std::shared_ptr<SkeletalMeshRenderer> mSkeletalMeshRenderer;

	// テレポート用AABB
	Vec3 mTeleportTriggerMin;    // ボックスの最小点
	Vec3 mTeleportTriggerMax;    // ボックスの最大点
	bool mTeleportActive = true; // テレポートの有効/無効状態

	std::unique_ptr<UPhysics::Engine> mUPhysicsEngine;

	std::unique_ptr<ParticleEmitter> mParticleEmitter;

	std::unique_ptr<ParticleObject> mParticleObject;

	std::unique_ptr<WindEffect>      mWindEffect;
	std::unique_ptr<ExplosionEffect> mExplosionEffect;

	// 遅延読み込み用フラグ
	bool mPendingMeshReload = false;
	bool mMeshReloadArmed   = false;

	IConVar* mShowPosConVar = nullptr;
	IConVar* mNameConVar    = nullptr;
	IConVar* mClearConVar   = nullptr;
};
