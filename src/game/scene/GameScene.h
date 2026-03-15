#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
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

#include <game/components/CameraAnimator.h>
#include <game/components/player/MovementComponent.h>
#include <game/components/weapon/base/WeaponComponent.h>
#include <game/scene/base/BaseScene.h>

#include "engine/Sprite/Sprite.h"

class Audio;
class GameTime;
class D3D12;
class EnemyMovement;
class CameraRotator;
class CameraSystem;
class CameraComponent;
class IConVar;
struct ReplayUserCmdFrame;

/**
 * @brief メインゲームシーンクラス
 * @details プレイヤー、敵、武器などのゲームプレイ要素を管理します
 */
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

	void SetDemoPlaybackEnabled(bool enabled);
	[[nodiscard]] bool IsDemoPlaybackEnabled() const;
	[[nodiscard]] MovementComponent* GetMovementComponent() const;
	[[nodiscard]] CameraRotator* GetCameraRotator() const;
	void ApplyReplayAuthoritativeState(const ReplayUserCmdFrame& frame);

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
	void InitializeFanMesh();
	void InitializeCameraRoot();
	void InitializeShakeRoot();
	void InitializeSkeletalMesh();
	void InitializeJumpPad();
	void InitializeSpeedBoostArea();
	void ConfigureEntityHierarchy();
	void ConfigureConsole();
	void InitializeTeleportTrigger();
	void InitializeCheckpoints();
	void InitializeGoal();

	void HandleMeshReload();
	void SyncCameraRoot() const;
	void HandleWeaponInput();
	void HandleWeaponFire(const std::shared_ptr<CameraComponent>& camera);
	void UpdateSkeletalAnimation();
	void UpdatePlayer(float deltaTime);
	void UpdatePostProcessing(float deltaTime);
	void UpdateTeleport();
	void UpdateParticlesAndEffects(float deltaTime);
	void UpdateReplayRecording(float deltaTime);
	void UpdateEntities(float deltaTime);
	void UpdateOpeningSequence(float deltaTime);
	void UpdateOpeningCameraTour(float deltaTime);
	void UpdateOpeningCountdown(float deltaTime);
	void UpdateOpeningCountdownAudio();
	void StartGameplayFromCountdown();
	void StartGameplayPresentation();
	void UpdateOpeningCountdownSprites() const;
	void UpdateOpeningFadeSprite();
	void UpdateCheckpointSplits();
	void UpdateRaceTimerSprites();
	void HideRaceTimerSprites();
	void DrawGameplayHud() const;
	void EnterOpeningCountdown();
	void CompleteOpeningSequence();
	void ApplyOpeningCameraPose(const Vec3& cameraPos, const Vec3& lookAtPos);
	[[nodiscard]] bool IsOpeningSequenceActive() const;
	void SetPlayerGameplayActive(bool active) const;
	void QueueReturnToTitle();

#ifdef _DEBUG
	void DrawDebugHud(const std::shared_ptr<CameraComponent>& camera) const;
#endif

	D3D12*    mRenderer = nullptr;
	GameTime* mTimer    = nullptr;

	std::unique_ptr<CubeMap> mCubeMap;

	std::unique_ptr<Entity> mEntCameraRoot;
	CameraRotator*          mCameraRotator = nullptr;

	std::unique_ptr<Entity> mCamera;

	std::unique_ptr<Entity>             mEntWorldMesh;
	std::shared_ptr<StaticMeshRenderer> mWorldMeshRenderer;

	std::unique_ptr<Entity>            mEntPlayer;
	std::shared_ptr<MovementComponent> mMovementComponent;

	std::unique_ptr<Entity>             mEntWeapon;
	std::shared_ptr<WeaponComponent>    mWeaponComponent;
	std::shared_ptr<StaticMeshRenderer> mWeaponMeshRenderer;

	std::unique_ptr<Entity>         mEntShakeRoot;
	std::shared_ptr<CameraAnimator> mCameraAnimator;

	std::unique_ptr<Entity> mEntViewmodelRoot;

	std::unique_ptr<Entity>               mEntSkeletalMesh;
	std::shared_ptr<SkeletalMeshRenderer> mSkeletalMeshRenderer;

	std::unique_ptr<Entity>             mFanEntity;
	std::shared_ptr<StaticMeshRenderer> mFanMeshRenderer;

	std::unique_ptr<Entity> mJumpPadEntity;
	std::unique_ptr<Entity> mJumpPadEntity2;
	std::unique_ptr<Entity> mSpeedBoostAreaEntity;
	std::unique_ptr<Entity> mSpeedBoostAreaEntity2;

	// テレポート用AABB
	Vec3 mTeleportTriggerMin;    // ボックスの最小点
	Vec3 mTeleportTriggerMax;    // ボックスの最大点
	bool mTeleportActive = true; // テレポートの有効/無効状態

	std::unique_ptr<UPhysics::Engine> mUPhysicsEngine;

	std::unique_ptr<ParticleEmitter> mParticleEmitter;

	std::unique_ptr<ParticleObject> mParticleObject;

	std::unique_ptr<WindEffect>      mWindEffect;
	std::unique_ptr<ExplosionEffect> mExplosionEffect;

	std::vector<std::unique_ptr<Entity>> mCheckpointEntities;
	std::unique_ptr<Entity>              mGoalEntity;

	std::unique_ptr<Sprite> mNextCheckpointSprite;
	std::unique_ptr<Sprite> mNextCheckpointArrowSprite;
	std::unique_ptr<Sprite> mCountdownDigitSprite;
	std::unique_ptr<Sprite> mCountdownStartSprite;
	std::unique_ptr<Sprite> mOpeningFadeSprite;
	std::array<std::unique_ptr<Sprite>, 8> mRaceTimerSprites;

	Vec3 mCountdownDigitBaseSize = Vec3::one;
	Vec3 mCountdownStartBaseSize = Vec3::one;
	Vec3 mRaceTimerDigitBaseSize = Vec3::one;
	Vec3 mRaceTimerColonBaseSize = Vec3::one;
	Vec3 mRaceTimerDotBaseSize   = Vec3::one;
	struct CheckpointSplitEntry {
		int    order   = 0;
		double timeSec = 0.0;
	};
	std::vector<CheckpointSplitEntry> mCheckpointSplits;
	int                               mLastActivatedCheckpointCount = 0;

	std::shared_ptr<Audio> mWind;
	std::shared_ptr<Audio> mRun;
	std::shared_ptr<Audio> mCountdownCountSe;
	std::shared_ptr<Audio> mCountdownStartSe;

	// 遅延読み込み用フラグ
	bool mPendingMeshReload    = false;
	bool mMeshReloadArmed      = false;
	bool mPendingReturnToTitle = false;
	bool mIsDemoPlayback       = false;
	enum class OpeningPhase {
		Tour,
		Countdown,
		Gameplay
	};
	OpeningPhase mOpeningPhase = OpeningPhase::Gameplay;
	std::size_t  mOpeningShotIndex       = 0;
	float        mOpeningShotElapsedSec  = 0.0f;
	float        mOpeningShotFadeElapsedSec = 0.0f;
	float        mCountdownElapsedSec    = 0.0f;
	float        mOpeningFadeAlpha       = 0.0f;
	Vec2         mOpeningFixedLookAngles = Vec2::zero;
	Vec2         mOpeningPlayerLookAngles = Vec2::zero;
	bool         mOpeningShotFadeActive   = false;
	bool         mOpeningShotFadeSwapped  = false;
	int          mLastCountdownCueStep    = -1;
	bool         mOpeningGameplayStarted  = false;
	bool         mGameplayPresentationStarted = false;
	float mRecordingTickAccumulatorSec = 0.0f;
	float mFanMovePhase = 100.0f;
	uint32_t mPendingReplayEdgeButtons = 0u;

	IConVar* mShowPosConVar = nullptr;
	IConVar* mNameConVar    = nullptr;
	IConVar* mClearConVar   = nullptr;
};
