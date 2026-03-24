#include "GameScene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <string>

#include <engine/Engine.h>
#include <engine/EngineServices.h>
#include <engine/Camera/CameraManager.h>
#include <engine/Components/Camera/CameraComponent.h>
#include <engine/Components/ColliderComponent/AABBCollider.h>
#include <engine/Components/ColliderComponent/MeshColliderComponent.h>
#include <engine/Debug/DebugDraw.h>
#include <engine/Debug/DebugHud.h>
#include <engine/ImGui/ImGuiUtil.h>
#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/ConVarManager.h>
#include <engine/ResourceSystem/Audio/AudioManager.h>
#include <engine/TextureManager/TexManager.h>
#include <engine/unnamed/subsystem/console/Log.h>
#include <runtime/core/math/Math.h>

#include <game/components/CameraRotator.h>
#include <game/components/JumpPadComponent.h>
#include <game/components/RotateComponent.h>
#include <game/components/SpeedBoostAreaComponent.h>
#include <game/components/player/PlayerInputController.h>
#include <game/components/checkpoint/CheckpointComponent.h>
#include <game/components/checkpoint/CheckpointManager.h>
#include <game/components/checkpoint/GoalComponent.h>
#include <game/components/player/KinematicCollisionResolver.h>
#include <game/replay/ReplayManager.h>

#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

#include "game/components/ViewmodelSway.h"

namespace {
	constexpr char kDevMeasureTexturePath[] =
		"./content/core/textures/dev_measure.png";
	constexpr char kUvCheckerTexturePath[] =
		"./content/core/textures/uvChecker.png";
	constexpr char kWaveTexturePath[] =
		"./content/core/textures/wave.dds";
	constexpr char kSmokeTexturePath[] =
		"./content/core/textures/smoke.png";
	constexpr char kPingTexturePath[] =
		"./content/parkour/textures/ping.png";
	constexpr char kArrowTexturePath[] =
		"./content/parkour/textures/arrow.png";
	constexpr char kDigitsAtlasPath[] =
		"./content/parkour/textures/digits.png";
	constexpr char kCountdownStartTexturePath[] =
		"./content/parkour/textures/start.png";
	constexpr char kColonTexturePath[] =
		"./content/parkour/textures/colon.png";
	constexpr char kDotTexturePath[] =
		"./content/parkour/textures/dot.png";
	constexpr char kFadeOverlayTexturePath[] =
		"./content/parkour/textures/title_overlay.png";
	constexpr char kCountdownCountSePath[] =
		"./content/parkour/sounds/se/count.wav";
	constexpr char kCountdownStartSePath[] =
		"./content/parkour/sounds/se/start.wav";
	constexpr char kWindParticleTexturePath[] =
		"./content/core/textures/circle.png";
	constexpr char kWeaponMeshPath[]   = "./content/core/models/weapon.obj";
	constexpr char kWeaponScriptPath[] =
		"./content/parkour/scripts/weapon_handgun.json";
	constexpr char kSkeletalMeshPath[] =
		"./content/parkour/models/hand/hand.gltf";
	constexpr char kFanMeshPath[] =
		"./content/core/models/fan.obj";
	constexpr char kRotateMeshPath[] =
		"./content/parkour/models/map/rotate.obj";
	constexpr char kWorldMeshInitialPath[] =
		"./content/parkour/models/map/sp_city.obj";
	constexpr char kWorldMeshReloadPath[] =
		"./content/parkour/models/map/sp_city.obj";
	constexpr char kAirAccelerateCommand[] =
		"sv_airaccelerate 100000000000000000";
	constexpr char  kMeshReloadBindCommand[]   = "bind f5 +f5";
	constexpr char  kSelfDestructBindCommand[] = "bind k +selfdestruct_test";
	constexpr Vec3  kShakeRootOffset(0.08f, -0.1f, 0.18f);
	constexpr float kCameraRootHeight  = 1.7f;
	constexpr float kPlayerSpawnHeight = 2.0f;
	constexpr Vec3  kTeleportTriggerCenter(19.5072f, -29.2608f, 260.096f);
	const Vec3      kTeleportTriggerExtent(Vec3::one * 13.0048f);
	constexpr float kTeleportReenableBuffer      = 1.0f;
	constexpr float kExplosionNormalOffset       = 2.0f;
	constexpr int   kExplosionParticleCount      = 32;
	constexpr float kExplosionParticleLifetime   = 30.0f;
	constexpr float kBlastMinSafeDistance        = 0.5f;
	constexpr float kBlurScale                   = 0.01f;
	constexpr float kBlastRadiusHu               = 512.0f;
	constexpr float kBlastPowerHu                = 1024.0f;
	constexpr float kPlayerCameraForwardOffsetHU = 4.0f;
	constexpr float kJumpPadBoostVelocityHu      = 800.0f;
	constexpr Vec3  kJumpPadPosition(10.0f, 0.0f, 6.0f);
	constexpr Vec3  kJumpPadPosition2(34.0f, 0.0f, -12.0f);
	constexpr float kJumpPadWidthHu  = 128.0f;
	constexpr float kJumpPadHeightHu = 64.0f;
	constexpr float kJumpPadDepthHu  = 128.0f;

	constexpr float kSpeedBoostMultiplier  = 1.5f;
	constexpr float kSpeedBoostDurationSec = 3.0f;
	constexpr Vec3  kSpeedBoostPosition(14.0f, 0.0f, 12.0f);
	constexpr Vec3  kSpeedBoostPosition2(42.0f, 0.0f, 18.0f);
	constexpr float kSpeedBoostWidthHu  = 192.0f;
	constexpr float kSpeedBoostHeightHu = 48.0f;
	constexpr float kSpeedBoostDepthHu  = 192.0f;

	template <typename T>
	std::shared_ptr<T> AdoptComponent(T* raw) {
		return std::shared_ptr<T>(raw, [](T*) {});
	}

	void ReRegisterRotateMeshes(
		UPhysics::Engine* physics, const Entity* e1, const Entity* e2
	) {
		if (!physics) { return; }
		if (e1) { physics->RegisterEntity(const_cast<Entity*>(e1)); }
		if (e2) { physics->RegisterEntity(const_cast<Entity*>(e2)); }
	}

	constexpr uint32_t kDefaultReplayTickRate = 66;
	constexpr int      kReplayCatchUpSteps    = 8;

	enum class CutsceneMotionType {
		Pan,
		Dolly,
		PanDolly
	};

	struct CutsceneShot {
		Vec3               startPos    = Vec3::zero;
		Vec3               endPos      = Vec3::zero;
		Vec3               startLook   = Vec3::forward;
		Vec3               endLook     = Vec3::forward;
		float              durationSec = 0.0f;
		CutsceneMotionType motion      = CutsceneMotionType::PanDolly;
	};

	constexpr std::array<CutsceneShot, 4> kOpeningCutsceneShots{
		{
			{
				.startPos    = Vec3(0.0f, 4.0f, 0.0f),
				.endPos      = Vec3(0.0f, 4.0f, 40.0f),
				.startLook   = Vec3(0.0f, 3.0f, 50.0f),
				.endLook     = Vec3(0.0f, 3.0f, 50.0f),
				.durationSec = 3.0f,
				.motion      = CutsceneMotionType::PanDolly
			},
			{
				.startPos    = Vec3(-52.0f, 8.0f, 118.0f),
				.endPos      = Vec3(-52.0f, 8.0f, 118.0f),
				.startLook   = Vec3(-28.0f, 1.0f, 140.0f),
				.endLook     = Vec3(-100.0f, 1.0f, 119.0f),
				.durationSec = 4.0f,
				.motion      = CutsceneMotionType::PanDolly
			},
			{
				.startPos    = Vec3(-113.794f, 75.0f, 92.0f),
				.endPos      = Vec3(-113.794f, 75.0f, 5.0f),
				.startLook   = Vec3(-113.794f, 0.0f, 92.5f),
				.endLook     = Vec3(-113.794f, 0.0f, 5.5f),
				.durationSec = 3.0f,
				.motion      = CutsceneMotionType::PanDolly
			},
			{
				.startPos    = Vec3(0.0f, 6.0f, 0.0f),
				.endPos      = Vec3(0.0f, 3.626f, 0.0f),
				.startLook   = Vec3(0.0f, 6.0f, 1.0f),
				.endLook     = Vec3(0.0f, 3.626f, 1.0f),
				.durationSec = 0.5f,
				.motion      = CutsceneMotionType::PanDolly
			}
		}
	};

	constexpr float kOpeningCountdownDigitDurationSec = 1.0f;
	constexpr float kOpeningCountdownStartDurationSec = 0.8f;
	constexpr float kOpeningShotFadeOutSec = 0.25f;
	constexpr float kOpeningShotFadeInSec = 0.25f;
	constexpr float kCountdownAtlasHeightPx = 64.0f;
	constexpr float kCountdownDigitWidthPx = 64.0f;
	constexpr float kGameOverBlinkOnSec = 0.06f;
	constexpr float kGameOverBlinkOffSec = 0.05f;
	constexpr int   kGameOverBlinkCount = 2;
	constexpr float kGameOverBlinkAlpha = 0.3f;
	constexpr Vec3  kGameOverBlinkColor = Vec3(0.175f, 0.01f, 0.01f);
	constexpr float kGameOverBlackFadeDurationSec = 0.55f;
	constexpr float kGameOverHoldBlackDurationSec = 0.20f;
	constexpr float kRunBgmBaseVolume = 0.20f;
	constexpr float kReturnToTitleBgmFadeDurationSec = 0.35f;

	float EvaluateCutsceneEase(const float t) {
		return Math::CubicBezier(
			std::clamp(t, 0.0f, 1.0f), 0.2f, 0.0f, 0.0f, 1.0f
		);
	}

	std::string FormatRaceTime(const double totalSeconds) {
		const double clampedSec    = std::max(0.0, totalSeconds);
		const int    totalCentisec = static_cast<int>(std::floor(
			clampedSec * 100.0
		));
		const int minutes      = totalCentisec / (60 * 100);
		const int seconds      = (totalCentisec / 100) % 60;
		const int centiseconds = totalCentisec % 100;
		return std::format("{:02}:{:02}.{:02}", minutes, seconds, centiseconds);
	}
}

/// @brief デストラクタ
GameScene::~GameScene() {
	// Shutdown() が呼ばれていない場合に備えて安全にクリア
	if (mUPhysicsEngine || mCamera || mEntPlayer) { Shutdown(); }
}

/// @brief 初期化
void GameScene::Init() {
	mRecordingTickAccumulatorSec  = 0.0f;
	mPendingReplayEdgeButtons     = 0u;
	mFanMovePhase                 = 100.0f;
	mOpeningPhase                 = OpeningPhase::Gameplay;
	mOpeningShotIndex             = 0;
	mOpeningShotElapsedSec        = 0.0f;
	mOpeningShotFadeElapsedSec    = 0.0f;
	mCountdownElapsedSec          = 0.0f;
	mOpeningFadeAlpha             = 0.0f;
	mOpeningFixedLookAngles       = Vec2::zero;
	mOpeningPlayerLookAngles      = Vec2::zero;
	mOpeningShotFadeActive        = false;
	mOpeningShotFadeSwapped       = false;
	mLastCountdownCueStep         = -1;
	mOpeningGameplayStarted       = false;
	mGameplayPresentationStarted  = false;
	mLastActivatedCheckpointCount = 0;
	mCheckpointSplits.clear();
	mPendingReturnToTitle        = false;
	mReturnToTitleRequestSent    = false;
	mReturnToTitleFadeElapsedSec = 0.0f;

	// 各種マネージャーの取得
	auto* engine = Unnamed::EngineServices::Get();
	UASSERT(engine && "Engine instance not registered");

	mAudioManager = engine ? engine->GetAudioManagerInstance() : nullptr;
	mRenderer = engine ? engine->GetRendererInstance() : nullptr;
	mResourceManager = engine ? engine->GetResourceManagerInstance() : nullptr;
	mSrvManager = engine ? engine->GetSrvManagerInstance() : nullptr;
	mTimer = ServiceLocator::Get<Unnamed::TimeSystem>()->GetGameTime();
	mSpriteCommon = engine ? engine->GetSpriteCommonInstance() : nullptr;

	// CheckpointManagerを初期化
	CheckpointManager::Initialize();
	// 各種初期化処理
	LoadCoreTextures();
	InitializeCubeMap();
	InitializeParticles();
	InitializePhysics();
	InitializeCamera();
	InitializePlayer();
	InitializeJumpPad();
	InitializeSpeedBoostArea();
	InitializeWorldMesh();
	InitializeFanMesh();
	InitializeRotateMesh();
	InitializeCameraRoot();
	InitializeShakeRoot();
	InitializeSkeletalMesh();
	InitializeWeapon();
	ConfigureEntityHierarchy();
	InitializeEffects();
	ConfigureConsole();
	InitializeTeleportTrigger();
	InitializeCheckpoints();
	InitializeGoal();

	// 次のチェックポイント矢印スプライトの初期化
	mNextCheckpointSprite = std::make_unique<Sprite>();
	mNextCheckpointSprite->Init(
		mSpriteCommon,
		kPingTexturePath
	);
	mNextCheckpointSprite->SetAnchorPoint({0.5f, 0.5f});

	mNextCheckpointArrowSprite = std::make_unique<Sprite>();
	mNextCheckpointArrowSprite->Init(
		mSpriteCommon,
		kArrowTexturePath
	);
	mNextCheckpointArrowSprite->SetAnchorPoint({0.5f, 0.5f});

	mCountdownDigitSprite = std::make_unique<Sprite>();
	mCountdownDigitSprite->Init(
		mSpriteCommon,
		kDigitsAtlasPath
	);
	mCountdownDigitSprite->SetAnchorPoint({0.5f, 0.5f});
	mCountdownDigitSprite->SetTextureLeftTop({0.0f, 0.0f});
	mCountdownDigitSprite->SetTextureSize(
		{kCountdownDigitWidthPx, kCountdownAtlasHeightPx}
	);
	mCountdownDigitBaseSize = mCountdownDigitSprite->GetSize();

	mCountdownStartSprite = std::make_unique<Sprite>();
	mCountdownStartSprite->Init(
		mSpriteCommon,
		kCountdownStartTexturePath
	);
	mCountdownStartSprite->SetAnchorPoint({0.5f, 0.5f});
	mCountdownStartBaseSize = mCountdownStartSprite->GetSize();

	mOpeningFadeSprite = std::make_unique<Sprite>();
	mOpeningFadeSprite->Init(
		mSpriteCommon,
		kFadeOverlayTexturePath
	);
	mOpeningFadeSprite->SetAnchorPoint({0.0f, 0.0f});
	mOpeningFadeAlpha = 0.0f;
	UpdateOpeningFadeSprite();

	mGameOverOverlaySprite = std::make_unique<Sprite>();
	mGameOverOverlaySprite->Init(
		mSpriteCommon,
		kFadeOverlayTexturePath
	);
	mGameOverOverlaySprite->SetAnchorPoint({0.0f, 0.0f});
	ResetGameOverState();
	UpdateGameOverOverlaySprite();

	for (std::size_t i = 0; i < mRaceTimerSprites.size(); ++i) {
		mRaceTimerSprites[i]        = std::make_unique<Sprite>();
		const bool  useDigitTexture = (i != 2 && i != 5);
		const char* texturePath     = useDigitTexture ?
			                              kDigitsAtlasPath :
			                              (i == 2 ?
				                               kColonTexturePath :
				                               kDotTexturePath);
		mRaceTimerSprites[i]->Init(mSpriteCommon, texturePath);
		mRaceTimerSprites[i]->SetAnchorPoint({0.0f, 0.0f});
		if (useDigitTexture) {
			mRaceTimerSprites[i]->SetTextureLeftTop({0.0f, 0.0f});
			mRaceTimerSprites[i]->SetTextureSize(
				{kCountdownDigitWidthPx, kCountdownAtlasHeightPx}
			);
			mRaceTimerDigitBaseSize = mRaceTimerSprites[i]->GetSize();
		} else if (i == 2) {
			mRaceTimerColonBaseSize = mRaceTimerSprites[i]->GetSize();
		} else { mRaceTimerDotBaseSize = mRaceTimerSprites[i]->GetSize(); }
	}
	HideRaceTimerSprites();

	mRun = mAudioManager->GetAudio(
		"./content/parkour/sounds/bgm/Run.wav"
	);

	mWind = mAudioManager->GetAudio(
		"./content/parkour/sounds/amb/wind.wav"
	);
	mCountdownCountSe = mAudioManager->GetAudio(kCountdownCountSePath);
	mCountdownStartSe = mAudioManager->GetAudio(kCountdownStartSePath);
	mDenySe           = mAudioManager->GetAudio(
		"./content/parkour/sounds/se/deny.wav"
	);

	if (!mIsDemoPlayback && mEntSkeletalMesh) {
		mEntSkeletalMesh->SetVisible(false);
	}

	if (mIsDemoPlayback) {
		SetPlayerGameplayActive(true);
		mOpeningPhase                 = OpeningPhase::Gameplay;
		mOpeningFadeAlpha             = 0.0f;
		mOpeningShotFadeActive        = false;
		mOpeningShotFadeSwapped       = false;
		mOpeningShotFadeElapsedSec    = 0.0f;
		mLastCountdownCueStep         = -1;
		mOpeningGameplayStarted       = true;
		mLastActivatedCheckpointCount = 0;
		mCheckpointSplits.clear();
		if (mTimer) { mTimer->StartGame(); }
		StartGameplayPresentation();
	} else {
		SetPlayerGameplayActive(false);
		mOpeningPhase              = OpeningPhase::Tour;
		mOpeningFadeAlpha          = 0.0f;
		mOpeningShotFadeActive     = false;
		mOpeningShotFadeSwapped    = false;
		mOpeningShotFadeElapsedSec = 0.0f;
		mLastCountdownCueStep      = -1;
		mOpeningGameplayStarted    = false;
		mOpeningShotIndex          = 0;
		mOpeningShotElapsedSec     = 0.0f;
		mCountdownElapsedSec       = 0.0f;
		if (mCameraRotator) {
			mOpeningPlayerLookAngles = mCameraRotator->GetLookAnglesDegrees();
		}
		if (!kOpeningCutsceneShots.empty()) {
			const CutsceneShot& firstShot = kOpeningCutsceneShots[0];
			ApplyOpeningCameraPose(firstShot.startPos, firstShot.startLook);
		}
	}

	if (mRun) {
		mRun->Play(true);
		mRun->SetVolume(kRunBgmBaseVolume);
	}
}

/// @brief 更新
/// @param deltaTime 経過時間
void GameScene::Update(const float deltaTime) {
	bool       gameOverActive      = IsGameOverSequenceActive();
	const bool returnToTitleActive = IsReturnToTitleTransitionActive();
	if (!gameOverActive && !returnToTitleActive) { HandleMeshReload(); }

	// ファンを物理エンジンから登録解除
	if (mUPhysicsEngine) {
		mUPhysicsEngine->UnregisterEntity(mFanEntity.get());
		mUPhysicsEngine->UnregisterEntity(mRotateMesh1.get());
		mUPhysicsEngine->UnregisterEntity(mRotateMesh2.get());
	}

	bool openingActive = IsOpeningSequenceActive();
	if (!gameOverActive && !mIsDemoPlayback &&
	    mOpeningPhase != OpeningPhase::Gameplay) {
		UpdateOpeningSequence(deltaTime);
		openingActive = IsOpeningSequenceActive();
	}

	if (!gameOverActive && !mIsDemoPlayback && !openingActive &&
	    !returnToTitleActive &&
	    InputSystem::IsTriggered("+selfdestruct_test")) {
		StartSelfDestructGameOver();
		gameOverActive = IsGameOverSequenceActive();
	}

	if (!gameOverActive && !mIsDemoPlayback &&
	    !returnToTitleActive &&
	    InputSystem::IsTriggered("backtotitle")) { QueueReturnToTitle(); }
	if (!gameOverActive && returnToTitleActive) {
		UpdateReturnToTitleTransition(deltaTime);
	}

	const auto camera = CameraManager::GetActiveCamera();
	if (gameOverActive) {
		if (mNextCheckpointSprite) { mNextCheckpointSprite->SetPos(Vec3::min); }
		if (mNextCheckpointArrowSprite) {
			mNextCheckpointArrowSprite->SetPos(Vec3::min);
		}
		HideRaceTimerSprites();
		UpdateGameOverSequence(deltaTime);
	} else if (!openingActive) {
		SyncCameraRoot();
		HandleWeaponInput();
		HandleWeaponFire(camera);
		UpdateSkeletalAnimation();
		UpdatePlayer(deltaTime);
		UpdateTeleport();
		UpdateReplayRecording(deltaTime);

		auto* nextCheckpoint = CheckpointManager::GetNextCheckpoint();
		auto* goal           = mGoalEntity ?
			                       mGoalEntity->GetComponent<
				                       GoalComponent>() :
			                       nullptr;

		if (goal && !goal->IsReached()) {
			bool  isOutOfScreen = false;
			float angle         = 0.0f;
			Vec2  screenPos;

			Vec2 clientSize = Unnamed::EngineServices::Get() ?
				                  Unnamed::EngineServices::Get()->
				                  GetViewportSizeInstance() :
				                  Vec2{};

			Vec2 viewportSize = clientSize;

			if (nextCheckpoint) {
				screenPos = Math::WorldToScreen(
					nextCheckpoint->GetOwner()->GetTransform()->GetWorldPos(),
					viewportSize,
					true,
					100.0f,
					isOutOfScreen,
					angle
				);
			} else {
				// 次のチェックポイントがない場合はゴールに向かう
				screenPos = Math::WorldToScreen(
					goal->GetOwner()->GetTransform()->GetWorldPos(),
					viewportSize,
					true,
					100.0f,
					isOutOfScreen,
					angle
				);
			}

			// 画面中心からの距離に応じて透明度を変化させる
			{
				const Vec2  center   = viewportSize * 0.5f;
				const Vec2  toCenter = screenPos - center;
				const float dist     = std::sqrt(
					toCenter.x * toCenter.x + toCenter.y * toCenter.y
				);

				const float maxDist = std::max(
					1.0f, std::sqrt(center.x * center.x + center.y * center.y)
				);
				const float t     = std::clamp(dist / maxDist, 0.0f, 1.0f);
				const float alpha = std::lerp(0.025f, 1.0f, t);

				mNextCheckpointSprite->SetColor(Vec4(1.0f, 1.0f, 1.0f, alpha));
			}

			mNextCheckpointSprite->SetPos(
				screenPos
			);

			mNextCheckpointArrowSprite->SetPos(screenPos);
			if (isOutOfScreen) {
				mNextCheckpointArrowSprite->SetRot(Vec3::forward * angle);
			} else { mNextCheckpointArrowSprite->SetPos(Vec3::min); }
		} else { CheckpointManager::ResetAllCheckpoints(); }
	} else {
		if (mNextCheckpointSprite) { mNextCheckpointSprite->SetPos(Vec3::min); }
		if (mNextCheckpointArrowSprite) {
			mNextCheckpointArrowSprite->SetPos(Vec3::min);
		}
	}

	UpdatePostProcessing(deltaTime);
	UpdateParticlesAndEffects(deltaTime);
	if (!gameOverActive) { UpdateEntities(deltaTime); }

	if (mNextCheckpointSprite) { mNextCheckpointSprite->Update(); }
	if (mNextCheckpointArrowSprite) { mNextCheckpointArrowSprite->Update(); }
	UpdateCheckpointSplits();
	UpdateRaceTimerSprites();
	DrawGameplayHud();
	UpdateOpeningFadeSprite();
	UpdateGameOverOverlaySprite();

#ifdef _DEBUG
	// チェックポイントのデバッグ表示
	if (!gameOverActive && !openingActive && !mCheckpointEntities.empty()) {
		constexpr Vec4 lineColor(0.0f, 1.0f, 0.0f, 1.0f); // 緑色

		// 順番に隣接するチェックポイント同士を結ぶ
		for (size_t i = 0; i + 1 < mCheckpointEntities.size(); ++i) {
			auto* a = mCheckpointEntities[i].get();
			auto* b = mCheckpointEntities[i + 1].get();
			if (!a || !b) { continue; }
			const Vec3 posA = a->GetTransform()->GetWorldPos();
			const Vec3 posB = b->GetTransform()->GetWorldPos();
			DebugDraw::DrawLine(posA, posB, lineColor);
		}

		// 最後のチェックポイントからゴールへ繋ぐ
		auto* last = mCheckpointEntities.back().get();
		if (last && mGoalEntity) {
			const Vec3 posLast = last->GetTransform()->GetWorldPos();
			const Vec3 posGoal = mGoalEntity->GetTransform()->GetWorldPos();
			DebugDraw::DrawLine(posLast, posGoal, lineColor);
		}
	}
	DrawDebugHud(camera);
#endif
}

/// @brief 描画
void GameScene::Render() {
	if (!mRenderer) { return; }

	auto* commandList = mRenderer->GetCommandList();

	if (mClearConVar && mClearConVar->GetValueAsBool() && mCubeMap) {
		mCubeMap->Render(commandList);
	}

	for (const auto* entity : mEntities) {
		if (entity) { entity->Render(commandList); }
	}

	if (auto* engine = Unnamed::EngineServices::Get()) {
		if (auto* particleManager = engine->GetParticleManagerInstance()) {
			particleManager->Render();
		}
	}

	if (mParticleObject) { mParticleObject->Draw(); }

	if (mWindEffect) { mWindEffect->Draw(); }

	if (mExplosionEffect) { mExplosionEffect->Draw(); }

	if (mSpriteCommon) { mSpriteCommon->Render(); }

	if (!IsOpeningSequenceActive()) {
		// if (mNextCheckpointSprite) { mNextCheckpointSprite->Draw(); }
		// if (mNextCheckpointArrowSprite) { mNextCheckpointArrowSprite->Draw(); }
		if (!mIsDemoPlayback) {
			for (const auto& timerSprite : mRaceTimerSprites) {
				if (timerSprite) { timerSprite->Draw(); }
			}
		}
	}

	if (mOpeningPhase == OpeningPhase::Countdown) {
		if (mCountdownDigitSprite) { mCountdownDigitSprite->Draw(); }
		if (mCountdownStartSprite) { mCountdownStartSprite->Draw(); }
	}
	if (mOpeningFadeSprite && mOpeningFadeAlpha > 0.0f) {
		mOpeningFadeSprite->Draw();
	}
	if (mGameOverOverlaySprite &&
	    mGameOverOverlayColor.w > 0.0f) { mGameOverOverlaySprite->Draw(); }
}

void GameScene::SetDemoPlaybackEnabled(const bool enabled) {
	mIsDemoPlayback = enabled;
}

bool GameScene::IsDemoPlaybackEnabled() const { return mIsDemoPlayback; }

MovementComponent* GameScene::GetMovementComponent() const {
	return mMovementComponent.get();
}

CameraRotator* GameScene::GetCameraRotator() const { return mCameraRotator; }

void GameScene::ApplyReplayAuthoritativeState(const ReplayUserCmdFrame& frame) {
	const bool wasSpeedVaulting = mMovementComponent ?
		                              mMovementComponent->IsSpeedVaulting() :
		                              false;

	if (frame.hasAuthoritativeState && mEntPlayer) {
		mEntPlayer->GetTransform()->SetWorldPos(
			Vec3(frame.playerPosX, frame.playerPosY, frame.playerPosZ)
		);
	}

	if (mMovementComponent) {
		if (frame.hasAuthoritativeState) {
			mMovementComponent->SetVelocity(
				Vec3(frame.playerVelX, frame.playerVelY, frame.playerVelZ)
			);
		}
		if (frame.hasVaultState) {
			mMovementComponent->ApplyReplayVaultState(
				frame.isSpeedVaulting,
				frame.vaultProgress,
				Vec3(frame.vaultStartX, frame.vaultStartY, frame.vaultStartZ),
				Vec3(frame.vaultApexX, frame.vaultApexY, frame.vaultApexZ),
				Vec3(frame.vaultEndX, frame.vaultEndY, frame.vaultEndZ)
			);
		}
	}

	const bool enteredSpeedVault =
		frame.hasVaultState && frame.isSpeedVaulting && !wasSpeedVaulting;
	if (!enteredSpeedVault) { SyncCameraRoot(); }

	// リプレイ補正後のトランスフォームを同フレーム描画へ反映する。
	if (const auto camera = CameraManager::GetActiveCamera()) {
		camera->Update(0.0f);
	}
}

/// @brief コンソール変数の登録
void GameScene::RegisterConVars() {
	ConVarManager::RegisterConVar("sv_ducktime", 1000.0f, "ms");
	ConVarManager::RegisterConVar(
		"sv_jumptime", 510.0f,
		"ms approx - based on the 21 unit height jump"
	);
	ConVarManager::RegisterConVar("sv_jumpheight", 21.0f, "units");
	ConVarManager::RegisterConVar("sv_timetounduck", 0.2f * 1000.0f, "ms");
	ConVarManager::RegisterConVar(
		"sv_timetounduckinv",
		1000.0f - 0.2f * 1000.0f, "ms"
	);
}

/// @brief コアテクスチャの読み込み
void GameScene::LoadCoreTextures() const {
	auto* engine     = Unnamed::EngineServices::Get();
	auto* texManager = engine ? engine->GetTexManagerInstance() : nullptr;
	if (!texManager) { return; }

	struct TextureRequest {
		const char* path = nullptr;
	};

	constexpr std::array<TextureRequest, 11> requests{
		{
			{kDevMeasureTexturePath},
			{kUvCheckerTexturePath},
			{kWaveTexturePath},
			{kSmokeTexturePath},
			{kPingTexturePath},
			{kArrowTexturePath},
			{kDigitsAtlasPath},
			{kColonTexturePath},
			{kDotTexturePath},
			{kCountdownStartTexturePath},
			{kFadeOverlayTexturePath},
		}
	};

	for (const auto& request : requests) {
		if (!request.path) { continue; }
		texManager->LoadTexture(request.path);
	}
}

/// @brief キューブマップの初期化
void GameScene::InitializeCubeMap() {
	if (!mRenderer || !mSrvManager) { return; }
	auto* engine     = Unnamed::EngineServices::Get();
	auto* texManager = engine ? engine->GetTexManagerInstance() : nullptr;
	if (!texManager) { return; }

	mCubeMap = std::make_unique<CubeMap>(
		mRenderer->GetDevice(),
		mSrvManager,
		texManager,
		kWaveTexturePath
	);
}

/// @brief パーティクルの初期化
void GameScene::InitializeParticles() {
	auto* engine          = Unnamed::EngineServices::Get();
	auto* particleManager = engine ?
		                        engine->GetParticleManagerInstance() :
		                        nullptr;
	if (!particleManager) { return; }

	particleManager->CreateParticleGroup("wind", kWindParticleTexturePath);

	mParticleEmitter = std::make_unique<ParticleEmitter>();
	mParticleEmitter->Init(particleManager, "wind");

	mParticleObject = std::make_unique<ParticleObject>();
	mParticleObject->Init(particleManager, kWindParticleTexturePath);
}

/// @brief エフェクトの初期化
void GameScene::InitializeEffects() {
	auto* engine          = Unnamed::EngineServices::Get();
	auto* particleManager = engine ?
		                        engine->GetParticleManagerInstance() :
		                        nullptr;
	if (!particleManager) { return; }

	if (mMovementComponent) {
		mWindEffect = std::make_unique<WindEffect>();
		mWindEffect->Init(particleManager, mMovementComponent.get());
	}

	mExplosionEffect = std::make_unique<ExplosionEffect>();
	mExplosionEffect->Init(particleManager, kSmokeTexturePath);
	mExplosionEffect->SetColorGradient(
		Vec4(0.78f, 0.29f, 0.05f, 1.0f),
		Vec4(0.04f, 0.04f, 0.05f, 1.0f)
	);
}

/// @brief 物理エンジンの初期化
void GameScene::InitializePhysics() {
	mUPhysicsEngine = std::make_unique<UPhysics::Engine>();
	mUPhysicsEngine->Init();
}

/// @brief カメラの初期化
void GameScene::InitializeCamera() {
	mCamera = std::make_unique<Entity>("camera");
	AddEntity(mCamera.get());

	auto* rawCamera = mCamera->AddComponent<CameraComponent>();
	if (!rawCamera) { return; }

	const auto camera = AdoptComponent(rawCamera);
	CameraManager::AddCamera(camera);
	CameraManager::SetActiveCamera(camera);
}

/// @brief プレイヤーの初期化
void GameScene::InitializePlayer() {
	mEntPlayer = std::make_unique<Entity>("player");
	mEntPlayer->GetTransform()->SetLocalPos(Vec3::up * kPlayerSpawnHeight);

	constexpr float widthHU     = 32.0f;
	constexpr float heightHU    = 72.0f;
	const auto      halfExtents = Vec3(
		Math::HtoM(widthHU) * 0.5f,
		Math::HtoM(heightHU) * 0.5f,
		Math::HtoM(widthHU) * 0.5f
	);
	// プレイヤーの位置は足元基準なので、AABBも足元基準にする
	// min: 足元から下方向、max: 足元から上方向（頭の位置）
	Unnamed::AABB playerAABB(
		Vec3(-halfExtents.x, 0.0f, -halfExtents.z), // 足元が原点
		Vec3(halfExtents.x, Math::HtoM(heightHU), halfExtents.z)
	);
	mEntPlayer->AddComponent<AABBCollider>(playerAABB, Vec3::zero);

	auto* movement     = mEntPlayer->AddComponent<MovementComponent>();
	mMovementComponent = AdoptComponent(movement);

	const MovementData moveData(32.0f, 72.0f);

	if (mMovementComponent && mUPhysicsEngine) {
		mMovementComponent->Init(mUPhysicsEngine.get(), moveData);
	}

	AddEntity(mEntPlayer.get());

	// CheckpointManagerにプレイヤーを登録
	CheckpointManager::SetPlayer(mEntPlayer.get());
}

/// @brief 武器の初期化
void GameScene::InitializeWeapon() {
	auto* meshManager = mResourceManager ?
		                    mResourceManager->GetMeshManager() :
		                    nullptr;
	if (meshManager) { meshManager->LoadMeshFromFile(kWeaponMeshPath); }

	mEntWeapon          = std::make_unique<Entity>("weapon");
	auto* renderer      = mEntWeapon->AddComponent<StaticMeshRenderer>();
	mWeaponMeshRenderer = AdoptComponent(renderer);
	if (mWeaponMeshRenderer && meshManager) {
		if (auto* mesh = meshManager->GetStaticMesh(kWeaponMeshPath)) {
			mWeaponMeshRenderer->SetStaticMesh(mesh);
		}
	}

	auto* weaponComponent = mEntWeapon->AddComponent<WeaponComponent>(
		kWeaponScriptPath
	);
	mWeaponComponent = AdoptComponent(weaponComponent);

	AddEntity(mEntWeapon.get());
	mEntWeapon->SetVisible(false);
}

/// @brief ワールドメッシュの初期化
void GameScene::InitializeWorldMesh() {
	auto* meshManager = mResourceManager ?
		                    mResourceManager->GetMeshManager() :
		                    nullptr;
	if (meshManager) { meshManager->LoadMeshFromFile(kWorldMeshInitialPath); }

	mEntWorldMesh      = std::make_unique<Entity>("worldMesh");
	auto* renderer     = mEntWorldMesh->AddComponent<StaticMeshRenderer>();
	mWorldMeshRenderer = AdoptComponent(renderer);
	if (mWorldMeshRenderer && meshManager) {
		if (auto* mesh = meshManager->GetStaticMesh(kWorldMeshInitialPath)) {
			mWorldMeshRenderer->SetStaticMesh(mesh);
		}
	}

	mEntWorldMesh->AddComponent<MeshColliderComponent>();
	AddEntity(mEntWorldMesh.get());

	if (mUPhysicsEngine) {
		mUPhysicsEngine->RegisterEntity(mEntWorldMesh.get());
	}
}

void GameScene::InitializeFanMesh() {
	auto* meshManager = mResourceManager ?
		                    mResourceManager->GetMeshManager() :
		                    nullptr;
	if (meshManager) { meshManager->LoadMeshFromFile(kFanMeshPath); }

	mFanEntity = std::make_unique<Entity>("fan");
	mFanEntity->GetTransform()->SetWorldPos(Vec3::down * 16.0f);
	auto* fanRenderer = mFanEntity->AddComponent<StaticMeshRenderer>();
	mFanMeshRenderer  = AdoptComponent(fanRenderer);
	if (mFanMeshRenderer && meshManager) {
		if (auto* mesh = meshManager->GetStaticMesh(kFanMeshPath)) {
			mFanMeshRenderer->SetStaticMesh(mesh);
		}
	}

	mFanEntity->AddComponent<RotateComponent>();
	mFanEntity->AddComponent<MeshColliderComponent>();

	AddEntity(mFanEntity.get());

	if (mUPhysicsEngine) { mUPhysicsEngine->RegisterEntity(mFanEntity.get()); }
}

void GameScene::InitializeRotateMesh() {
	auto* meshManager = mResourceManager ?
		                    mResourceManager->GetMeshManager() :
		                    nullptr;

	if (meshManager) { meshManager->LoadMeshFromFile(kRotateMeshPath); }

	mRotateMesh1          = std::make_unique<Entity>("rotateMesh1");
	auto* rotateRenderer1 = mRotateMesh1->AddComponent<StaticMeshRenderer>();
	mRotateMeshRenderer1  = AdoptComponent(rotateRenderer1);
	if (mRotateMeshRenderer1 && meshManager) {
		if (auto* mesh = meshManager->GetStaticMesh(kRotateMeshPath)) {
			mRotateMeshRenderer1->SetStaticMesh(mesh);
		}
	}

	mRotateMesh1->GetTransform()->SetWorldPos(
		Vec3(
			-78.029f,
			-77.6224f,
			-136.55f
		)
	);

	mRotateMesh2          = std::make_unique<Entity>("rotateMesh2");
	auto* rotateRenderer2 = mRotateMesh2->AddComponent<StaticMeshRenderer>();
	mRotateMeshRenderer2  = AdoptComponent(rotateRenderer2);
	if (mRotateMeshRenderer2 && meshManager) {
		if (auto* mesh = meshManager->GetStaticMesh(kRotateMeshPath)) {
			mRotateMeshRenderer2->SetStaticMesh(mesh);
		}
	}

	mRotateMesh2->GetTransform()->SetWorldPos(
		Vec3(
			-78.0288f,
			-77.6224f,
			-182.067f
		)
	);

	auto* rotate1 = mRotateMesh1->AddComponent<RotateComponent>();
	rotate1->SetRotationRate(Vec3::up * 90.0f);
	auto* rotate2 = mRotateMesh2->AddComponent<RotateComponent>();
	rotate2->SetRotationRate(Vec3::up * -90.0f);

	mRotateMesh1->AddComponent<MeshColliderComponent>();
	mRotateMesh2->AddComponent<MeshColliderComponent>();

	AddEntity(mRotateMesh1.get());
	AddEntity(mRotateMesh2.get());

	if (mUPhysicsEngine) {
		mUPhysicsEngine->RegisterEntity(mRotateMesh1.get());
		mUPhysicsEngine->RegisterEntity(mRotateMesh2.get());
	}
}

/// @brief カメラルートの初期化
void GameScene::InitializeCameraRoot() {
	mEntCameraRoot = std::make_unique<Entity>("cameraRoot");
	mEntCameraRoot->GetTransform()->SetLocalPos(Vec3::up * kCameraRootHeight);
	mCameraRotator = mEntCameraRoot->AddComponent<CameraRotator>();
	AddEntity(mEntCameraRoot.get());
}

/// @brief シェイクルートの初期化
void GameScene::InitializeShakeRoot() {
	mEntShakeRoot = std::make_unique<Entity>("shakeRoot");

	// CameraAnimatorコンポーネントを追加
	auto* animator  = mEntShakeRoot->AddComponent<CameraAnimator>();
	mCameraAnimator = AdoptComponent(animator);

	if (mCameraAnimator && mMovementComponent && mCameraRotator) {
		mCameraAnimator->Init(mMovementComponent.get(), mCameraRotator);
		mMovementComponent->SetCameraAnimator(mCameraAnimator.get());
	}
}

/// @brief スケルタルメッシュの初期化
void GameScene::InitializeSkeletalMesh() {
	auto* meshManager = mResourceManager ?
		                    mResourceManager->GetMeshManager() :
		                    nullptr;
	if (meshManager) {
		meshManager->LoadSkeletalMeshFromFile(kSkeletalMeshPath);
	}

	mEntSkeletalMesh = std::make_unique<Entity>("SkeletalMeshEntity");
	auto* renderer = mEntSkeletalMesh->AddComponent<SkeletalMeshRenderer>();
	mSkeletalMeshRenderer = AdoptComponent(renderer);

	mEntViewmodelRoot = std::make_unique<Entity>("ViewmodelRoot");
	mEntViewmodelRoot->AddComponent<ViewmodelSway>();

	if (mSkeletalMeshRenderer && meshManager) {
		if (auto* mesh = meshManager->GetSkeletalMesh(kSkeletalMeshPath)) {
			mSkeletalMeshRenderer->SetSkeletalMesh(mesh);
		}
	}
	mSkeletalMeshRenderer->PlayAnimation("Sprint", true);

	AddEntity(mEntViewmodelRoot.get());
	AddEntity(mEntSkeletalMesh.get());
}

/// @brief ジャンプパッドの初期化
void GameScene::InitializeJumpPad() {
	mJumpPadEntity = std::make_unique<Entity>("JumpPad");
	mJumpPadEntity->GetTransform()->SetWorldPos(kJumpPadPosition);

	const float width  = Math::HtoM(kJumpPadWidthHu);
	const float height = Math::HtoM(kJumpPadHeightHu);
	const float depth  = Math::HtoM(kJumpPadDepthHu);

	Unnamed::AABB aabb(
		Vec3(-width * 0.5f, 0.0f, -depth * 0.5f),
		Vec3(width * 0.5f, height, depth * 0.5f)
	);
	mJumpPadEntity->AddComponent<AABBCollider>(aabb, Vec3::zero);

	mJumpPadEntity->AddComponent<JumpPadComponent>(
		kJumpPadBoostVelocityHu
	);

	AddEntity(mJumpPadEntity.get());

	mJumpPadEntity2 = std::make_unique<Entity>("JumpPad2");
	mJumpPadEntity2->GetTransform()->SetWorldPos(kJumpPadPosition2);
	mJumpPadEntity2->AddComponent<AABBCollider>(aabb, Vec3::zero);
	mJumpPadEntity2->AddComponent<JumpPadComponent>(
		kJumpPadBoostVelocityHu
	);
	AddEntity(mJumpPadEntity2.get());
}

void GameScene::InitializeSpeedBoostArea() {
	mSpeedBoostAreaEntity = std::make_unique<Entity>("SpeedBoostArea");
	mSpeedBoostAreaEntity->GetTransform()->SetWorldPos(kSpeedBoostPosition);

	const float width  = Math::HtoM(kSpeedBoostWidthHu);
	const float height = Math::HtoM(kSpeedBoostHeightHu);
	const float depth  = Math::HtoM(kSpeedBoostDepthHu);

	Unnamed::AABB aabb(
		Vec3(-width * 0.5f, 0.0f, -depth * 0.5f),
		Vec3(width * 0.5f, height, depth * 0.5f)
	);
	mSpeedBoostAreaEntity->AddComponent<AABBCollider>(aabb, Vec3::zero);

	mSpeedBoostAreaEntity->AddComponent<SpeedBoostAreaComponent>(
		kSpeedBoostMultiplier,
		kSpeedBoostDurationSec
	);

	AddEntity(mSpeedBoostAreaEntity.get());

	mSpeedBoostAreaEntity2 = std::make_unique<Entity>("SpeedBoostArea2");
	mSpeedBoostAreaEntity2->GetTransform()->SetWorldPos(kSpeedBoostPosition2);
	mSpeedBoostAreaEntity2->AddComponent<AABBCollider>(aabb, Vec3::zero);
	mSpeedBoostAreaEntity2->AddComponent<SpeedBoostAreaComponent>(
		kSpeedBoostMultiplier,
		kSpeedBoostDurationSec
	);
	AddEntity(mSpeedBoostAreaEntity2.get());
}

/// @brief エンティティ階層の設定
void GameScene::ConfigureEntityHierarchy() {
	if (mEntShakeRoot && mEntCameraRoot) {
		mEntShakeRoot->SetParent(mEntCameraRoot.get());
		mEntShakeRoot->GetTransform()->SetLocalPos(Vec3::zero);
		AddEntity(mEntShakeRoot.get()); // ShakeRootをシーンに追加
	}

	if (mCamera && mEntShakeRoot) {
		mCamera->SetParent(mEntShakeRoot.get());
		mCamera->GetTransform()->SetLocalPos(
			Vec3::forward *
			Math::HtoM(kPlayerCameraForwardOffsetHU)
		);
	}

	if (mEntSkeletalMesh && mEntCameraRoot) {
		auto* transform = mEntSkeletalMesh->GetTransform();
		transform->SetLocalPos(Vec3::zero);
		transform->SetLocalRot(Quaternion::EulerDegrees(0.0f, 180.0f, 0.0f));
		mEntViewmodelRoot->SetParent(mEntCameraRoot.get());
		mEntSkeletalMesh->SetParent(mEntViewmodelRoot.get());

		mEntWeapon->SetParent(mEntViewmodelRoot.get());
		mEntWeapon->GetTransform()->SetLocalPos(kShakeRootOffset);

		mEntViewmodelRoot->SetParent(mEntShakeRoot.get());
	}
}

/// @brief コンソールの設定
void GameScene::ConfigureConsole() {
	Console::SubmitCommand(kAirAccelerateCommand);
	Console::SubmitCommand(kMeshReloadBindCommand, true);
	Console::SubmitCommand(kSelfDestructBindCommand, true);

	mShowPosConVar = ConVarManager::GetConVar("cl_showpos");
	mNameConVar    = ConVarManager::GetConVar("name");
	mClearConVar   = ConVarManager::GetConVar("r_clear");
}

/// @brief テレポートトリガーの初期化
void GameScene::InitializeTeleportTrigger() {
	const Vec3 triggerSize = kTeleportTriggerExtent * 2.0f;
	mTeleportTriggerMin    = kTeleportTriggerCenter - triggerSize * 0.5f;
	mTeleportTriggerMax    = kTeleportTriggerCenter + triggerSize * 0.5f;
	mTeleportActive        = true;
}

/// @brief チェックポイントの初期化
void GameScene::InitializeCheckpoints() {
	// チェックポイント1
	{
		auto checkpoint = std::make_unique<Entity>("Checkpoint1");
		// エンティティ位置は地面（Y=0）の中心点とする
		checkpoint->GetTransform()->SetWorldPos(Vec3(26.010f, 1.626f, -4.865f));

		const float   width  = Math::HtoM(16.0f);
		const float   height = Math::HtoM(192.0f);
		const float   depth  = Math::HtoM(128.0f);
		Unnamed::AABB aabb(
			Vec3(-width * 0.5f, 0.0f, -depth * 0.5f),
			Vec3(width * 0.5f, height, depth * 0.5f)
		);
		checkpoint->AddComponent<AABBCollider>(aabb, Vec3::zero);

		// 0番目、リスポーン位置
		checkpoint->AddComponent<CheckpointComponent>(
			0, Vec3(26.01f, 1.63f, -4.86f)
		);

		AddEntity(checkpoint.get());
		mCheckpointEntities.push_back(std::move(checkpoint));
	}

	// チェックポイント2
	{
		auto checkpoint = std::make_unique<Entity>("Checkpoint2");
		checkpoint->GetTransform()->
		            SetWorldPos(Vec3(149.555f, 8.128f, 40.640f));

		const float   width  = Math::HtoM(16.0f);
		const float   height = Math::HtoM(128.0f);
		const float   depth  = Math::HtoM(128.0f);
		Unnamed::AABB aabb(
			Vec3(-width * 0.5f, 0.0f, -depth * 0.5f),
			Vec3(width * 0.5f, height, depth * 0.5f)
		);
		checkpoint->AddComponent<AABBCollider>(aabb, Vec3::zero);

		checkpoint->AddComponent<CheckpointComponent>(
			1, Vec3(149.555f, 8.128f, 40.640f) // リスポーン位置も地面基準に
		);

		AddEntity(checkpoint.get());
		mCheckpointEntities.push_back(std::move(checkpoint));
	}

	// チェックポイント3
	{
		auto checkpoint = std::make_unique<Entity>("Checkpoint3");
		checkpoint->GetTransform()->SetWorldPos(
			Vec3(162.560f, 1.626f, 177.190f)
		);

		const float   width  = Math::HtoM(1024.0f);
		const float   height = Math::HtoM(1280.0f);
		const float   depth  = Math::HtoM(16.0f);
		Unnamed::AABB aabb(
			Vec3(-width * 0.5f, 0.0f, -depth * 0.5f),
			Vec3(width * 0.5f, height, depth * 0.5f)
		);
		checkpoint->AddComponent<AABBCollider>(aabb, Vec3::zero);

		checkpoint->AddComponent<CheckpointComponent>(
			2, Vec3(162.560f, 4.0f, 177.190f) // リスポーン位置も地面基準に
		);

		AddEntity(checkpoint.get());
		mCheckpointEntities.push_back(std::move(checkpoint));
	}

	Console::Print(
		std::format("Initialized {} checkpoints", mCheckpointEntities.size()),
		Vec4(0.0f, 1.0f, 1.0f, 1.0f)
	);
}

/// @brief ゴールの初期化e
void GameScene::InitializeGoal() {
	mGoalEntity = std::make_unique<Entity>("Goal");
	// エンティティ位置は地面（Y=0）の中心点とする
	mGoalEntity->GetTransform()->SetWorldPos(
		Vec3(-216.205f, -183.693f, 152.806f)
	);

	// AABBコライダーを追加（幅10m、高さ5mのトリガー）
	// 地面から上方向に伸びるAABBにする
	const float   width  = Math::HtoM(64.0f);
	const float   height = Math::HtoM(1920.0f);
	const float   depth  = Math::HtoM(896.0f);
	Unnamed::AABB aabb(
		Vec3(-width * 0.5f, 0.0f, -depth * 0.5f),
		Vec3(width * 0.5f, height, depth * 0.5f)
	);
	mGoalEntity->AddComponent<AABBCollider>(aabb, Vec3::zero);

	// ゴールコンポーネントを追加
	mGoalEntity->AddComponent<GoalComponent>();

	AddEntity(mGoalEntity.get());

	Console::Print(
		"Goal initialized",
		Vec4(1.0f, 0.0f, 1.0f, 1.0f)
	);
}

/// @brief メッシュリロードの処理
void GameScene::HandleMeshReload() {
	if (InputSystem::IsTriggered("+f5")) {
		mPendingMeshReload = true;
		mMeshReloadArmed   = false;
		Console::Print("Mesh reload requested...", kConTextColorCompleted);
	}

	if (!mPendingMeshReload) { return; }

	if (mMeshReloadArmed) {
		ReloadWorldMesh();
		mPendingMeshReload = false;
		mMeshReloadArmed   = false;
	} else { mMeshReloadArmed = true; }
}

/// @brief カメラルートの同期
void GameScene::SyncCameraRoot() const {
	if (!mEntCameraRoot || !mMovementComponent) { return; }

	mEntCameraRoot->GetTransform()->SetWorldPos(
		mMovementComponent->GetHeadPos()
	);
}

/// @brief 武器入力の処理
void GameScene::HandleWeaponInput() {
	if (!mWeaponComponent) { return; }

	float trigger = InputSystem::GetRightTrigger(0);

	if (InputSystem::IsPressed("+attack1") || trigger > 0.1f) {
		mWeaponComponent->PullTrigger();
	}
	if (InputSystem::IsReleased("+attack1") || trigger <= 0.1f) {
		mWeaponComponent->ReleaseTrigger();
	}
	if (InputSystem::IsPressed("+reload") && mEntPlayer) {
		// 最後のチェックポイントにリスポーン
		CheckpointManager::RespawnAtLastCheckpoint();
	}
}

/// @brief 武器発射の処理
void GameScene::HandleWeaponFire(
	const std::shared_ptr<CameraComponent>& camera
) {
	if (!mWeaponComponent || !mWeaponComponent->HasFiredThisFrame() || !
	    mUPhysicsEngine) { return; }

	if (!camera) { return; }

	Vec3 origin    = Vec3::zero;
	Vec3 direction = Vec3::forward;
	if (auto* owner = camera->GetOwner()) {
		if (auto* transform = owner->GetTransform()) {
			origin    = transform->GetWorldPos();
			direction = transform->GetWorldRot() * Vec3::forward;
		}
	} else {
		Mat4 inverseView = camera->GetViewMat().Inverse();
		origin           = inverseView.GetTranslate();
		direction        = inverseView.GetForward();
	}
	if (direction.SqrLength() > 1.0e-8f) { direction.Normalize(); } else {
		direction = Vec3::forward;
	}

	const Vec3 invDirection(
		direction.x != 0.0f ?
			1.0f / direction.x :
			std::numeric_limits<float>::infinity(),
		direction.y != 0.0f ?
			1.0f / direction.y :
			std::numeric_limits<float>::infinity(),
		direction.z != 0.0f ?
			1.0f / direction.z :
			std::numeric_limits<float>::infinity()
	);

	Unnamed::Ray ray{};
	ray.origin = origin;
	ray.dir    = direction;
	ray.invDir = invDirection;
	ray.tMin   = 0.0f;
	ray.tMax   = 100.0f;

	UPhysics::Hit hit{};
	if (!mUPhysicsEngine->RayCast(ray, &hit)) { return; }

	if (mExplosionEffect) {
		mExplosionEffect->TriggerExplosion(
			hit.pos + hit.normal * kExplosionNormalOffset,
			hit.normal,
			kExplosionParticleCount,
			kExplosionParticleLifetime
		);
	}

	if (!mEntPlayer || !mMovementComponent) { return; }

	const Vec3  playerPos   = mEntPlayer->GetTransform()->GetWorldPos();
	const float blastRadius = Math::HtoM(kBlastRadiusHu);
	const float blastPower  = Math::HtoM(kBlastPowerHu);
	const Vec3  toPlayer    = playerPos - hit.pos;
	const float distance    = toPlayer.Length();

	if (distance < blastRadius && distance > kBlastMinSafeDistance) {
		const Vec3  forceDir = toPlayer.Normalized();
		const float force    = blastPower * (1.0f - (distance / blastRadius));
		mMovementComponent->SetVelocity(
			mMovementComponent->GetVelocity() + forceDir * force
		);
	}
}

/// @brief スケルタルアニメーションの更新
void GameScene::UpdateSkeletalAnimation() {
	if (
		!mSkeletalMeshRenderer ||
		!mMovementComponent
	) { return; }

	// プレイヤーが前に進んでいるかチェック
	Vec3 forward = Vec3::forward;
	if (const auto camera = CameraManager::GetActiveCamera()) {
		if (auto* owner = camera->GetOwner()) {
			if (auto* transform = owner->GetTransform()) {
				forward = transform->GetWorldRot() * Vec3::forward;
			}
		} else { forward = camera->GetViewMat().Inverse().GetForward(); }
	}
	forward.y = 0.0f; // 上下成分は無視
	if (forward.SqrLength() > 1.0e-8f) { forward.Normalize(); } else {
		forward = Vec3::forward;
	}
	const float dot = forward.Dot(
		mMovementComponent->GetVelocity().Normalized()
	);
	constexpr float kMovingForwardThreshold = 0.125f;
	const bool      movingForward           = dot > kMovingForwardThreshold;

	constexpr float kTransitionDuration = 0.25f;

	// アニメーションの遷移

	// スライディング中 || しゃがみ中はしゃがみアニメーションに遷移
	if (mMovementComponent->IsSliding()
		/*|| mMovementComponent->IsDucking()*/) {
		mEntSkeletalMesh->SetVisible(true);
		if (mSkeletalMeshRenderer->GetCurrentAnimationName() != "Crouch") {
			mSkeletalMeshRenderer->TransitionToAnimation(
				"Crouch", kTransitionDuration, true
			);
			mSkeletalMeshRenderer->SetAnimationSpeed(1.0f);
		}
	} else if (mMovementComponent->IsGrounded() && movingForward) {
		mEntSkeletalMesh->SetVisible(true);
		if (mSkeletalMeshRenderer->GetCurrentAnimationName() != "Sprint") {
			mSkeletalMeshRenderer->TransitionToAnimation(
				"Sprint", kTransitionDuration, true
			);
		}
		const Vec3 velocity = mMovementComponent->GetVelocity();
		const Vec3 horizontalVelocity(velocity.x, 0.0f, velocity.z);
		mSkeletalMeshRenderer->SetAnimationSpeed(
			horizontalVelocity.Length() * 0.15f
		);
	} else if (mMovementComponent->IsWallRunning()) {
		// とりあえず走るアニメーションに遷移M
		mEntSkeletalMesh->SetVisible(true);
		if (mSkeletalMeshRenderer->GetCurrentAnimationName() != "Sprint") {
			mSkeletalMeshRenderer->TransitionToAnimation(
				"Sprint", kTransitionDuration, true
			);
			mSkeletalMeshRenderer->SetAnimationSpeed(1.0f);
		}
	} else {
		// 何もしていない場合はバインドポーズに戻す
		//mEntSkeletalMesh->SetVisible(false);
		if (mSkeletalMeshRenderer->GetCurrentAnimationName() != "BindPose") {
			mSkeletalMeshRenderer->TransitionToAnimation(
				"BindPose", kTransitionDuration, true
			);
		}
		//mSkeletalMeshRenderer->SetAnimationTime(0.67f * 0.25f);
	}
}

/// @brief プレイヤーの更新
/// @param deltaTime 経過時間
void GameScene::UpdatePlayer(const float deltaTime) {
	// スライディング中・ウォールラン中はFOVを広げる
	bool isSliding = mMovementComponent->IsSliding() || mMovementComponent->
	                 IsWallRunning();

	constexpr float defaultFov = 90.0f;
	float targetFov;
	float currentFov = CameraManager::GetActiveCamera()->GetFovVertical() *
	                   Math::rad2Deg;

	(void)currentFov;

	if (isSliding) { targetFov = defaultFov + 14.0f; } else {
		targetFov = defaultFov;
	}

	if (mCameraAnimator) {
		targetFov += mCameraAnimator->GetBlinkFovOffsetDeg();
	}

	targetFov = std::lerp(currentFov, targetFov, deltaTime * 10.0f);

	CameraManager::GetActiveCamera()->SetFovVertical(targetFov * Math::deg2Rad);

	// プレイヤーが高速で移動しているときは風の音を大きくする
	if (mMovementComponent && mWind) {
		auto            velocity       = mMovementComponent->GetVelocity();
		const float     speed          = Math::MtoH(velocity.Length());
		constexpr float maxSpeed       = 3500.0f;
		constexpr float kWindThreshold = 500.0f;
		static float    volume         = 0.0f;
		const float     target         = speed >= kWindThreshold ?
			                                 std::clamp(
				                                 speed / maxSpeed, 0.0f, 0.9f
			                                 ) :
			                                 0.0f;
		volume = std::lerp(volume, target, deltaTime * 10.0f);
		mWind->SetVolume(volume);
	}
}

/// @brief ポストプロセッシングの更新
void GameScene::UpdatePostProcessing(float deltaTime) {
	const float blurStrength = mMovementComponent ?
		                           mMovementComponent->GetVelocity().
		                           Length() * kBlurScale :
		                           0.0f;

	if (auto* engine = Unnamed::EngineServices::Get()) {
		engine->GetBlurStrengthInstance() = blurStrength;
	}

	if (mClearConVar && mClearConVar->GetValueAsBool() && mCubeMap) {
		mCubeMap->Update(deltaTime);
	}
}

/// @brief テレポートの更新
void GameScene::UpdateTeleport() {
	if (!mEntPlayer) { return; }

	const Vec3 playerPos = mEntPlayer->GetTransform()->GetWorldPos();

	if (mTeleportActive &&
	    playerPos.x >= mTeleportTriggerMin.x && playerPos.x <=
	    mTeleportTriggerMax.x &&
	    playerPos.y >= mTeleportTriggerMin.y && playerPos.y <=
	    mTeleportTriggerMax.y &&
	    playerPos.z >= mTeleportTriggerMin.z && playerPos.z <=
	    mTeleportTriggerMax.z) {
		mEntPlayer->GetTransform()->SetWorldPos(Vec3::zero);
		mTeleportActive = false;
		Console::Print("テレポートしました！");
	}

	Unnamed::AABB teleportTriggerAABB(mTeleportTriggerMin, mTeleportTriggerMax);
	DebugDraw::DrawBox(
		teleportTriggerAABB.Center(),
		Quaternion::identity,
		teleportTriggerAABB.Size(),
		Vec4(1.0f, 0.0f, 0.0f, 0.5f)
	);

	if (!mTeleportActive) {
		const bool outside =
			playerPos.x < mTeleportTriggerMin.x - kTeleportReenableBuffer ||
			playerPos.x > mTeleportTriggerMax.x + kTeleportReenableBuffer ||
			playerPos.y < mTeleportTriggerMin.y - kTeleportReenableBuffer ||
			playerPos.y > mTeleportTriggerMax.y + kTeleportReenableBuffer ||
			playerPos.z < mTeleportTriggerMin.z - kTeleportReenableBuffer ||
			playerPos.z > mTeleportTriggerMax.z + kTeleportReenableBuffer;

		if (outside) { mTeleportActive = true; }
	}
}

/// @brief パーティクルとエフェクトの更新
/// @param deltaTime 経過時間
void GameScene::UpdateParticlesAndEffects(float deltaTime) {
	if (auto* engine = Unnamed::EngineServices::Get()) {
		if (auto* particleManager = engine->GetParticleManagerInstance()) {
			particleManager->Update(deltaTime);
		}
	}

	if (mParticleEmitter) { mParticleEmitter->Update(deltaTime); }

	if (mWindEffect) { mWindEffect->Update(deltaTime); }

	if (mExplosionEffect) { mExplosionEffect->Update(deltaTime); }
}

void GameScene::UpdateReplayRecording(const float deltaTime) {
	auto& replayManager = ReplayManager::Get();
	if (mIsDemoPlayback || !replayManager.IsRecording()) {
		mRecordingTickAccumulatorSec = 0.0f;
		mPendingReplayEdgeButtons    = 0u;
		return;
	}

	const uint32_t tickRate = replayManager.GetRecordingTickRateOrDefault(
		kDefaultReplayTickRate
	);
	const float fixedTickSec     = 1.0f / static_cast<float>(tickRate);
	mRecordingTickAccumulatorSec += deltaTime;
	if (InputSystem::IsTriggered("blink")) {
		mPendingReplayEdgeButtons |= ReplayButton_Blink;
	}

	HumanPlayerInputController inputSampler;
	const PlayerInputFrame     sampledInput = inputSampler.SampleInput();

	const Vec2 lookAngles = mCameraRotator ?
		                        mCameraRotator->GetLookAnglesDegrees() :
		                        Vec2::zero;

	int stepCount = 0;
	while (mRecordingTickAccumulatorSec >= fixedTickSec &&
	       stepCount < kReplayCatchUpSteps) {
		ReplayUserCmdFrame frame;
		frame.moveX        = std::clamp(sampledInput.moveInput.x, -1.0f, 1.0f);
		frame.moveY        = std::clamp(sampledInput.moveInput.y, -1.0f, 1.0f);
		frame.viewPitchDeg = lookAngles.x;
		frame.viewYawDeg   = lookAngles.y;
		if (mEntPlayer && mMovementComponent) {
			const Vec3 playerPos = mEntPlayer->GetTransform()->GetWorldPos();
			const Vec3 playerVel = mMovementComponent->GetVelocity();
			frame.hasAuthoritativeState = true;
			frame.playerPosX = playerPos.x;
			frame.playerPosY = playerPos.y;
			frame.playerPosZ = playerPos.z;
			frame.playerVelX = playerVel.x;
			frame.playerVelY = playerVel.y;
			frame.playerVelZ = playerVel.z;

			frame.hasVaultState   = true;
			frame.isSpeedVaulting = mMovementComponent->IsSpeedVaulting();
			frame.vaultProgress   = mMovementComponent->GetVaultProgress();
			const Vec3 vaultStart = mMovementComponent->GetVaultStartPos();
			const Vec3 vaultApex  = mMovementComponent->GetVaultApexPos();
			const Vec3 vaultEnd   = mMovementComponent->GetVaultEndPos();
			frame.vaultStartX     = vaultStart.x;
			frame.vaultStartY     = vaultStart.y;
			frame.vaultStartZ     = vaultStart.z;
			frame.vaultApexX      = vaultApex.x;
			frame.vaultApexY      = vaultApex.y;
			frame.vaultApexZ      = vaultApex.z;
			frame.vaultEndX       = vaultEnd.x;
			frame.vaultEndY       = vaultEnd.y;
			frame.vaultEndZ       = vaultEnd.z;
		}

		if (sampledInput.wishJump) { frame.buttons |= ReplayButton_Jump; }
		if (sampledInput.wishCrouch) { frame.buttons |= ReplayButton_Crouch; }
		if ((mPendingReplayEdgeButtons & ReplayButton_Blink) != 0u) {
			frame.buttons             |= ReplayButton_Blink;
			mPendingReplayEdgeButtons &= ~ReplayButton_Blink;
		}
		if (InputSystem::IsPressed("+attack1")) {
			frame.buttons |= ReplayButton_Attack1;
		}
		if (InputSystem::IsPressed("+reload")) {
			frame.buttons |= ReplayButton_Reload;
		}

		replayManager.CaptureRecordingTick(frame);
		mRecordingTickAccumulatorSec -= fixedTickSec;
		++stepCount;
	}

	if (stepCount >= kReplayCatchUpSteps) {
		mRecordingTickAccumulatorSec = 0.0f;
	}
}

/// @brief エンティティの更新
/// @param deltaTime 経過時間
void GameScene::UpdateEntities(float deltaTime) {
	// 物理更新前の処理
	for (auto* entity : mEntities) {
		if (entity && !entity->GetParent()) { entity->PrePhysics(deltaTime); }
	}

	// ファンをSinCosで移動させる
	if (mFanEntity) {
		const auto newPos = Vec3(
			std::sin(mFanMovePhase) * 20.0f,
			mFanEntity->GetTransform()->GetWorldPos().y,
			mFanEntity->GetTransform()->GetWorldPos().z
		);

		mFanMovePhase += deltaTime;

		mFanEntity->GetTransform()->SetWorldPos(newPos);

		if (mUPhysicsEngine) {
			mUPhysicsEngine->RegisterEntity(mFanEntity.get());
		}
	}

	if (mRotateMesh1 && mRotateMesh2) {
		if (mUPhysicsEngine) {
			mUPhysicsEngine->RegisterEntity(mRotateMesh1.get());
			mUPhysicsEngine->RegisterEntity(mRotateMesh2.get());
		}
	}

	// 物理更新
	for (auto* entity : mEntities) {
		if (entity && !entity->GetParent()) { entity->Update(deltaTime); }
	}

	// 物理エンジンの更新
	if (mUPhysicsEngine) { mUPhysicsEngine->Update(deltaTime); }

	// 物理更新後の処理
	for (auto* entity : mEntities) {
		if (entity && !entity->GetParent()) { entity->PostPhysics(deltaTime); }
	}
}

void GameScene::UpdateOpeningSequence(const float deltaTime) {
	if (mIsDemoPlayback || mOpeningPhase == OpeningPhase::Gameplay) { return; }

	switch (mOpeningPhase) {
		case OpeningPhase::Tour: {
			if (InputSystem::IsTriggered("jump")) {
				EnterOpeningCountdown();
				return;
			}
			UpdateOpeningCameraTour(deltaTime);
			return;
		}
		case OpeningPhase::Countdown: {
			UpdateOpeningCountdown(deltaTime);
			return;
		}
		case OpeningPhase::Gameplay:
		default: return;
	}
}

void GameScene::UpdateOpeningCameraTour(const float deltaTime) {
	if (kOpeningCutsceneShots.empty()) {
		mOpeningFadeAlpha = 0.0f;
		EnterOpeningCountdown();
		return;
	}

	const CutsceneShot& shot     = kOpeningCutsceneShots[mOpeningShotIndex];
	const float         duration = std::max(0.01f, shot.durationSec);
	mOpeningShotElapsedSec       += deltaTime;

	const float rawT = std::clamp(
		mOpeningShotElapsedSec / duration, 0.0f, 1.0f
	);
	const float easedT = EvaluateCutsceneEase(rawT);

	Vec3 cameraPos = shot.startPos;
	Vec3 lookAtPos = shot.startLook;
	switch (shot.motion) {
		case CutsceneMotionType::Pan: lookAtPos = Math::Lerp(
			                              shot.startLook, shot.endLook, easedT
		                              );
			break;
		case CutsceneMotionType::Dolly: cameraPos = Math::Lerp(
			                                shot.startPos, shot.endPos, easedT
		                                );
			break;
		case CutsceneMotionType::PanDolly: cameraPos = Math::Lerp(
			                                   shot.startPos, shot.endPos,
			                                   easedT
		                                   );
			lookAtPos = Math::Lerp(shot.startLook, shot.endLook, easedT);
			break;
	}

	ApplyOpeningCameraPose(cameraPos, lookAtPos);

	const bool hasNextShot = (mOpeningShotIndex + 1 < kOpeningCutsceneShots.
	                          size());
	if (hasNextShot && !mOpeningShotFadeSwapped) {
		const float fadeOutDuration = std::max(0.01f, kOpeningShotFadeOutSec);
		const float fadeOutStartSec = std::max(
			0.0f, duration - fadeOutDuration
		);
		if (mOpeningShotElapsedSec >= fadeOutStartSec) {
			const float fadeOutT = std::clamp(
				(mOpeningShotElapsedSec - fadeOutStartSec) / fadeOutDuration,
				0.0f,
				1.0f
			);
			mOpeningShotFadeActive = true;
			mOpeningFadeAlpha      = EvaluateCutsceneEase(fadeOutT);
		} else if (!mOpeningShotFadeActive) { mOpeningFadeAlpha = 0.0f; }
	}

	if (rawT >= 1.0f) {
		if (hasNextShot) {
			++mOpeningShotIndex;
			mOpeningShotElapsedSec  = 0.0f;
			mOpeningShotFadeActive  = true;
			mOpeningShotFadeSwapped = true;
			mOpeningFadeAlpha       = 1.0f;

			const CutsceneShot& nextShot = kOpeningCutsceneShots[
				mOpeningShotIndex];
			ApplyOpeningCameraPose(nextShot.startPos, nextShot.startLook);
		} else {
			mOpeningFadeAlpha = 0.0f;
			EnterOpeningCountdown();
		}
	}

	if (mOpeningShotFadeActive && mOpeningShotFadeSwapped) {
		const float fadeInDuration = std::max(0.01f, kOpeningShotFadeInSec);
		const float fadeInT        = std::clamp(
			mOpeningShotElapsedSec / fadeInDuration, 0.0f, 1.0f
		);
		mOpeningFadeAlpha = 1.0f - EvaluateCutsceneEase(fadeInT);
		if (fadeInT >= 1.0f) {
			mOpeningFadeAlpha          = 0.0f;
			mOpeningShotFadeActive     = false;
			mOpeningShotFadeSwapped    = false;
			mOpeningShotFadeElapsedSec = 0.0f;
		}
	}
}

void GameScene::UpdateOpeningCountdown(const float deltaTime) {
	mCountdownElapsedSec += deltaTime;
	UpdateOpeningCountdownAudio();
	const float digitTotalDuration = kOpeningCountdownDigitDurationSec * 3.0f;
	if (!mOpeningGameplayStarted && mCountdownElapsedSec >=
	    digitTotalDuration) { StartGameplayFromCountdown(); }
	if (!mOpeningGameplayStarted && mCameraRotator) {
		mCameraRotator->SetLookAnglesDegrees(
			mOpeningFixedLookAngles.x,
			mOpeningFixedLookAngles.y
		);
	}
	UpdateOpeningCountdownSprites();

	const float totalDuration = digitTotalDuration +
	                            kOpeningCountdownStartDurationSec;
	if (mCountdownElapsedSec >= totalDuration) { CompleteOpeningSequence(); }
}

void GameScene::UpdateOpeningCountdownAudio() {
	const float digitTotalDuration = kOpeningCountdownDigitDurationSec * 3.0f;
	const float totalDuration      = digitTotalDuration +
	                                 kOpeningCountdownStartDurationSec;

	int cueStep = -1;
	if (mCountdownElapsedSec < digitTotalDuration) {
		const int digit = 3 - static_cast<int>(
			                  mCountdownElapsedSec /
			                  kOpeningCountdownDigitDurationSec
		                  );
		cueStep = std::clamp(digit, 1, 3);
	} else if (mCountdownElapsedSec < totalDuration) { cueStep = 0; }

	if (cueStep == mLastCountdownCueStep) { return; }
	mLastCountdownCueStep = cueStep;

	if (cueStep >= 1) {
		if (mCountdownCountSe) { mCountdownCountSe->Play(false); }
	} else if (cueStep == 0) {
		if (mCountdownStartSe) { mCountdownStartSe->Play(false); }
	}
}

void GameScene::StartGameplayFromCountdown() {
	if (mOpeningGameplayStarted) { return; }
	mOpeningGameplayStarted = true;
	SetPlayerGameplayActive(true);
	if (mTimer) { mTimer->StartGame(); }
	StartGameplayPresentation();
	mLastActivatedCheckpointCount = 0;
	mCheckpointSplits.clear();
}

void GameScene::StartGameplayPresentation() {
	if (mGameplayPresentationStarted) { return; }
	mGameplayPresentationStarted = true;

	if (mWind) {
		mWind->Play(true);
		mWind->SetVolume(1.0f);
	}

	if (mEntSkeletalMesh) { mEntSkeletalMesh->SetVisible(true); }
	UpdateSkeletalAnimation();
}

void GameScene::UpdateOpeningCountdownSprites() const {
	if (!mCountdownDigitSprite || !mCountdownStartSprite) { return; }

	const Vec2 viewport = Unnamed::EngineServices::Get() ?
		                      Unnamed::EngineServices::Get()->
		                      GetViewportSizeInstance() :
		                      Vec2(1280.0f, 720.0f);
	const float viewW   = std::max(1.0f, viewport.x);
	const float viewH   = std::max(1.0f, viewport.y);
	const float centerX = viewW * 0.5f;
	const float centerY = viewH * 0.44f;

	const float digitBaseWidth = std::max(
		1.0f, mCountdownDigitBaseSize.x * 0.1f
	);
	const float digitBaseHeight = std::max(1.0f, mCountdownDigitBaseSize.y);
	const float digitTarget     = std::clamp(
		std::min(viewW, viewH) * 0.24f, 96.0f, 320.0f
	);
	const float digitScale = digitTarget / std::max(
		                         digitBaseWidth, digitBaseHeight
	                         );

	float startScale =
		std::clamp(
			viewW * 0.42f / std::max(1.0f, mCountdownStartBaseSize.x), 0.45f,
			1.45f
		);
	float       startWidth  = mCountdownStartBaseSize.x * startScale;
	float       startHeight = mCountdownStartBaseSize.y * startScale;
	const float startY      = centerY + digitTarget * 0.05f;

	const float digitTotalDuration = kOpeningCountdownDigitDurationSec * 3.0f;
	if (mCountdownElapsedSec < digitTotalDuration) {
		const int digit = 3 - static_cast<int>(
			                  mCountdownElapsedSec /
			                  kOpeningCountdownDigitDurationSec);
		const float phase = std::clamp(
			mCountdownElapsedSec - std::floor(mCountdownElapsedSec),
			0.0f,
			1.0f
		);

		const float pulseScale = 1.0f + (1.0f - phase) * 0.24f;
		const float alpha      = std::clamp(1.0f - phase * 0.75f, 0.0f, 1.0f);
		mCountdownDigitSprite->SetPos({centerX, centerY, 91.0f});
		mCountdownDigitSprite->SetSize(
			{
				digitBaseWidth * digitScale * pulseScale,
				digitBaseHeight * digitScale * pulseScale,
				1.0f
			}
		);
		const float digitIndex = std::clamp(
			static_cast<float>(digit), 0.0f, 9.0f
		);
		mCountdownDigitSprite->SetTextureLeftTop(
			{digitIndex * kCountdownDigitWidthPx, 0.0f}
		);
		mCountdownDigitSprite->SetTextureSize(
			{kCountdownDigitWidthPx, kCountdownAtlasHeightPx}
		);
		mCountdownDigitSprite->SetColor({1.0f, 1.0f, 1.0f, alpha});

		mCountdownStartSprite->SetPos({centerX, startY, 91.0f});
		mCountdownStartSprite->SetSize({startWidth, startHeight, 1.0f});
		mCountdownStartSprite->SetColor({1.0f, 1.0f, 1.0f, 0.0f});
	} else {
		const float phase = std::clamp(
			(mCountdownElapsedSec - digitTotalDuration) /
			kOpeningCountdownStartDurationSec,
			0.0f,
			1.0f
		);
		const float alpha = std::sin(phase * Math::pi);
		const float pulse = 1.0f + 0.08f * std::sin(phase * Math::pi);
		startWidth        *= pulse;
		startHeight       *= pulse;

		mCountdownDigitSprite->SetPos({centerX, centerY, 91.0f});
		mCountdownDigitSprite->SetSize(
			{
				digitBaseWidth * digitScale,
				digitBaseHeight * digitScale,
				1.0f
			}
		);
		mCountdownDigitSprite->SetColor({1.0f, 1.0f, 1.0f, 0.0f});

		mCountdownStartSprite->SetPos({centerX, startY, 91.0f});
		mCountdownStartSprite->SetSize({startWidth, startHeight, 1.0f});
		mCountdownStartSprite->SetColor({1.0f, 1.0f, 1.0f, alpha});
	}

	mCountdownDigitSprite->Update();
	mCountdownStartSprite->Update();
}

void GameScene::UpdateOpeningFadeSprite() {
	if (!mOpeningFadeSprite) { return; }

	const Vec2 viewport = Unnamed::EngineServices::Get() ?
		                      Unnamed::EngineServices::Get()->
		                      GetViewportSizeInstance() :
		                      Vec2(1280.0f, 720.0f);
	const float viewW = std::max(1.0f, viewport.x);
	const float viewH = std::max(1.0f, viewport.y);
	const float alpha = std::clamp(mOpeningFadeAlpha, 0.0f, 1.0f);

	mOpeningFadeSprite->SetPos({0.0f, 0.0f, 95.0f});
	mOpeningFadeSprite->SetSize({viewW, viewH, 1.0f});
	mOpeningFadeSprite->SetColor({0.0f, 0.0f, 0.0f, alpha});
	mOpeningFadeSprite->Update();
}

void GameScene::UpdateCheckpointSplits() {
	if (mIsDemoPlayback || !mTimer || IsOpeningSequenceActive() ||
	    IsGameOverSequenceActive()) { return; }

	const int activatedCount = CheckpointManager::GetActivatedCheckpointCount();
	if (activatedCount > mLastActivatedCheckpointCount) {
		const double checkpointTime = mTimer->TotalTime();
		if (const auto* checkpoint =
			CheckpointManager::GetLastActivatedCheckpoint()) {
			const int  order = checkpoint->GetOrder();
			const auto dupIt = std::ranges::find_if(
				mCheckpointSplits,
				[order](const CheckpointSplitEntry& entry) {
					return entry.order == order;
				}
			);
			if (dupIt == mCheckpointSplits.end()) {
				mCheckpointSplits.push_back({order, checkpointTime});
			}
		}
	}

	mLastActivatedCheckpointCount = activatedCount;
}

void GameScene::UpdateRaceTimerSprites() {
	if (mIsDemoPlayback || !mTimer || IsOpeningSequenceActive() ||
	    IsGameOverSequenceActive()) {
		HideRaceTimerSprites();
		return;
	}

	const Vec2 viewport = Unnamed::EngineServices::Get() ?
		                      Unnamed::EngineServices::Get()->
		                      GetViewportSizeInstance() :
		                      Vec2(1280.0f, 720.0f);
	const float viewH = std::max(1.0f, viewport.y);

	const float timerHeight = std::clamp(viewH * 0.05f, 24.0f, 56.0f);
	const float digitWidth  = timerHeight * (
		                          kCountdownDigitWidthPx /
		                          kCountdownAtlasHeightPx);
	const float colonAspect = mRaceTimerColonBaseSize.y > 0.0f ?
		                          mRaceTimerColonBaseSize.x /
		                          mRaceTimerColonBaseSize.y :
		                          0.5f;
	const float dotAspect = mRaceTimerDotBaseSize.y > 0.0f ?
		                        mRaceTimerDotBaseSize.x / mRaceTimerDotBaseSize.
		                        y :
		                        0.35f;
	const float colonWidth = timerHeight * std::max(0.2f, colonAspect);
	const float dotWidth   = timerHeight * std::max(0.15f, dotAspect);
	const float spacing    = 0.0f;

	const std::string    timeText = FormatRaceTime(mTimer->TotalTime());
	std::array<float, 8> glyphWidths{};
	std::array<float, 8> glyphAdvances{};
	float                totalWidth = 0.0f;
	for (std::size_t i = 0; i < mRaceTimerSprites.size(); ++i) {
		const bool isDigitSlot = (i != 2 && i != 5);
		if (isDigitSlot) {
			glyphWidths[i]   = digitWidth;
			glyphAdvances[i] = digitWidth * 0.84f;
		} else if (i == 2) {
			glyphWidths[i]   = colonWidth;
			glyphAdvances[i] = colonWidth * 0.9f;
		} else {
			glyphWidths[i]   = dotWidth;
			glyphAdvances[i] = dotWidth * 0.95f;
		}
		totalWidth += glyphAdvances[i];
	}
	totalWidth += spacing * static_cast<float>(mRaceTimerSprites.size() - 1);

	float       cursorX = std::max(0.0f, (viewport.x - totalWidth) * 0.5f);
	const float posY    = std::clamp(viewH * 0.035f, 12.0f, 28.0f);
	for (std::size_t i = 0; i < mRaceTimerSprites.size(); ++i) {
		auto* sprite = mRaceTimerSprites[i].get();
		if (!sprite) { continue; }

		const bool  isDigitSlot = (i != 2 && i != 5);
		const float glyphWidth  = glyphWidths[i];
		if (isDigitSlot) {
			const char ch    = i < timeText.size() ? timeText[i] : '0';
			const int  digit = (ch >= '0' && ch <= '9') ? (ch - '0') : 0;
			sprite->SetTextureLeftTop(
				{static_cast<float>(digit) * kCountdownDigitWidthPx, 0.0f}
			);
			sprite->SetTextureSize(
				{kCountdownDigitWidthPx, kCountdownAtlasHeightPx}
			);
		}

		sprite->SetPos({cursorX, posY, 92.0f});
		sprite->SetSize({glyphWidth, timerHeight, 1.0f});
		sprite->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
		sprite->Update();
		cursorX += glyphAdvances[i] + spacing;
	}
}

void GameScene::HideRaceTimerSprites() {
	for (auto& sprite : mRaceTimerSprites) {
		if (!sprite) { continue; }
		sprite->SetPos(Vec3::min);
		sprite->SetColor({1.0f, 1.0f, 1.0f, 0.0f});
		sprite->Update();
	}
}

void GameScene::DrawGameplayHud() const {
	if (mIsDemoPlayback || !mTimer || IsOpeningSequenceActive() ||
	    IsGameOverSequenceActive()) { return; }

#ifdef _DEBUG
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (!viewport) { return; }

	ImDrawList* drawList = ImGui::GetBackgroundDrawList();
	if (!drawList) { return; }

	const float timerHeight = std::clamp(
		viewport->Size.y * 0.05f, 24.0f, 56.0f
	);
	const ImVec2 basePos(
		viewport->Pos.x + 28.0f,
		viewport->Pos.y + 24.0f + timerHeight + 12.0f
	);
	float y = basePos.y;
	for (const CheckpointSplitEntry& split : mCheckpointSplits) {
		const std::string splitText = std::format(
			"CP{}   {}",
			split.order + 1,
			FormatRaceTime(split.timeSec)
		);
		ImGuiUtil::TextOutlined(
			drawList,
			ImVec2(basePos.x, y),
			splitText.c_str(),
			ImVec4(0.82f, 0.95f, 1.0f, 0.98f),
			ImVec4(0.0f, 0.0f, 0.0f, 0.85f),
			1.0f
		);
		y += ImGui::GetFontSize() * 1.2f;
	}
#endif
}

void GameScene::StartSelfDestructGameOver() {
	if (mIsDemoPlayback || IsOpeningSequenceActive() ||
	    IsGameOverSequenceActive()) { return; }

	mGameOverPhase           = GameOverPhase::RedBlink;
	mGameOverPhaseElapsedSec = 0.0f;
	mGameOverBlinkCount      = 0;
	mGameOverReloadRequested = false;
	mGameOverOverlayColor    = Vec4(
		kGameOverBlinkColor.x,
		kGameOverBlinkColor.y,
		kGameOverBlinkColor.z,
		0.0f
	);

	if (mDenySe) { mDenySe->Play(false); }

	if (mNextCheckpointSprite) { mNextCheckpointSprite->SetPos(Vec3::min); }
	if (mNextCheckpointArrowSprite) {
		mNextCheckpointArrowSprite->SetPos(Vec3::min);
	}
	HideRaceTimerSprites();
}

void GameScene::UpdateGameOverSequence(const float deltaTime) {
	if (!IsGameOverSequenceActive()) { return; }

	mGameOverPhaseElapsedSec += std::max(0.0f, deltaTime);

	constexpr float kGameOverRedBlinkTotalSec =
		static_cast<float>(kGameOverBlinkCount) *
		(kGameOverBlinkOnSec + kGameOverBlinkOffSec);
	constexpr float kGameOverBgmFadeTotalSec =
		kGameOverRedBlinkTotalSec + kGameOverBlackFadeDurationSec;

	auto applyRunFade = [this](const float progress) {
		if (!mRun) { return; }
		const float clamped = std::clamp(progress, 0.0f, 1.0f);
		mRun->SetVolume(std::lerp(kRunBgmBaseVolume, 0.0f, clamped));
	};

	switch (mGameOverPhase) {
		case GameOverPhase::None: {
			mGameOverOverlayColor = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
			applyRunFade(0.0f);
			return;
		}
		case GameOverPhase::RedBlink: {
			const float cycleDuration =
				kGameOverBlinkOnSec + kGameOverBlinkOffSec;
			if (cycleDuration <= 0.0f) {
				mGameOverPhase           = GameOverPhase::BlackFade;
				mGameOverPhaseElapsedSec = 0.0f;
				mGameOverOverlayColor    = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
				break;
			}

			const int completedCycles = static_cast<int>(
				mGameOverPhaseElapsedSec / cycleDuration
			);
			mGameOverBlinkCount = completedCycles;
			if (completedCycles >= kGameOverBlinkCount) {
				mGameOverPhase           = GameOverPhase::BlackFade;
				mGameOverPhaseElapsedSec = 0.0f;
				mGameOverOverlayColor    = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
				break;
			}

			const float phaseTime = std::fmod(
				mGameOverPhaseElapsedSec, cycleDuration
			);
			const bool blinkOn    = phaseTime < kGameOverBlinkOnSec;
			mGameOverOverlayColor = Vec4(
				kGameOverBlinkColor.x,
				kGameOverBlinkColor.y,
				kGameOverBlinkColor.z,
				blinkOn ? kGameOverBlinkAlpha : 0.0f
			);
			applyRunFade(mGameOverPhaseElapsedSec / kGameOverBgmFadeTotalSec);
			break;
		}
		case GameOverPhase::BlackFade: {
			const float t = std::clamp(
				mGameOverPhaseElapsedSec /
				std::max(0.01f, kGameOverBlackFadeDurationSec),
				0.0f,
				1.0f
			);
			mGameOverOverlayColor = Vec4(
				0.0f,
				0.0f,
				0.0f,
				EvaluateCutsceneEase(t)
			);
			applyRunFade(
				(kGameOverRedBlinkTotalSec + mGameOverPhaseElapsedSec) /
				kGameOverBgmFadeTotalSec
			);
			if (t >= 1.0f) {
				mGameOverPhase           = GameOverPhase::HoldBlack;
				mGameOverPhaseElapsedSec = 0.0f;
				mGameOverOverlayColor    = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
			}
			break;
		}
		case GameOverPhase::HoldBlack: {
			mGameOverOverlayColor = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
			applyRunFade(1.0f);
			if (mGameOverPhaseElapsedSec >= kGameOverHoldBlackDurationSec &&
			    !mGameOverReloadRequested) {
				mGameOverReloadRequested = true;
				mGameOverPhase           = GameOverPhase::ReloadRequested;
				RequestCurrentSceneReload();
			}
			break;
		}
		case GameOverPhase::ReloadRequested: {
			mGameOverOverlayColor = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
			applyRunFade(1.0f);
			break;
		}
	}
}

void GameScene::UpdateGameOverOverlaySprite() {
	if (!mGameOverOverlaySprite) { return; }

	const Vec2 viewport = Unnamed::EngineServices::Get() ?
		                      Unnamed::EngineServices::Get()->
		                      GetViewportSizeInstance() :
		                      Vec2(1280.0f, 720.0f);
	const float viewW = std::max(1.0f, viewport.x);
	const float viewH = std::max(1.0f, viewport.y);
	Vec4        color = mGameOverOverlayColor;
	color.w           = std::clamp(color.w, 0.0f, 1.0f);

	mGameOverOverlaySprite->SetPos({0.0f, 0.0f, 96.0f});
	mGameOverOverlaySprite->SetSize({viewW, viewH, 1.0f});
	mGameOverOverlaySprite->SetColor(color);
	mGameOverOverlaySprite->Update();
}

void GameScene::ResetGameOverState() {
	mGameOverPhase           = GameOverPhase::None;
	mGameOverPhaseElapsedSec = 0.0f;
	mGameOverBlinkCount      = 0;
	mGameOverReloadRequested = false;
	mGameOverOverlayColor    = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

void GameScene::UpdateReturnToTitleTransition(const float deltaTime) {
	if (!IsReturnToTitleTransitionActive()) { return; }

	mReturnToTitleFadeElapsedSec += std::max(0.0f, deltaTime);
	const float fadeT            = std::clamp(
		mReturnToTitleFadeElapsedSec /
		std::max(0.01f, kReturnToTitleBgmFadeDurationSec),
		0.0f,
		1.0f
	);

	if (mRun) { mRun->SetVolume(std::lerp(kRunBgmBaseVolume, 0.0f, fadeT)); }

	if (fadeT >= 1.0f && !mReturnToTitleRequestSent) {
		mReturnToTitleRequestSent = true;
		RequestSceneChange("TestScene");
	}
}

bool GameScene::IsReturnToTitleTransitionActive() const {
	return mPendingReturnToTitle && !mReturnToTitleRequestSent;
}

void GameScene::EnterOpeningCountdown() {
	mOpeningPhase              = OpeningPhase::Countdown;
	mCountdownElapsedSec       = 0.0f;
	mOpeningShotElapsedSec     = 0.0f;
	mOpeningShotFadeElapsedSec = 0.0f;
	mOpeningFadeAlpha          = 0.0f;
	mOpeningShotFadeActive     = false;
	mOpeningShotFadeSwapped    = false;
	mLastCountdownCueStep      = -1;
	mOpeningGameplayStarted    = false;

	if (mCountdownDigitSprite) {
		mCountdownDigitSprite->SetTextureLeftTop({0.0f, 0.0f});
		mCountdownDigitSprite->SetTextureSize(
			{kCountdownDigitWidthPx, kCountdownAtlasHeightPx}
		);
	}
	SyncCameraRoot();
	if (mCameraRotator) {
		mCameraRotator->SetLookAnglesDegrees(
			mOpeningPlayerLookAngles.x,
			mOpeningPlayerLookAngles.y
		);
		mOpeningFixedLookAngles = mOpeningPlayerLookAngles;
	}
	UpdateOpeningCountdownAudio();
	UpdateOpeningCountdownSprites();
}

void GameScene::CompleteOpeningSequence() {
	if (mOpeningPhase == OpeningPhase::Gameplay) { return; }

	mOpeningPhase              = OpeningPhase::Gameplay;
	mOpeningFadeAlpha          = 0.0f;
	mOpeningShotFadeActive     = false;
	mOpeningShotFadeSwapped    = false;
	mOpeningShotFadeElapsedSec = 0.0f;
	mOpeningGameplayStarted    = true;
	SetPlayerGameplayActive(true);
	StartGameplayPresentation();
	SyncCameraRoot();
	mOpeningFixedLookAngles = Vec2::zero;
}

void GameScene::ApplyOpeningCameraPose(
	const Vec3& cameraPos, const Vec3& lookAtPos
) {
	if (!mEntCameraRoot) { return; }

	mEntCameraRoot->GetTransform()->SetWorldPos(cameraPos);

	Vec3 viewDir = lookAtPos - cameraPos;
	if (viewDir.SqrLength() <= 1.0e-8f) { return; }
	viewDir.Normalize();

	const Quaternion lookRotation = Quaternion::LookRotation(viewDir, Vec3::up);
	const Vec3       eulerDegrees = lookRotation.ToEulerDegrees();
	if (mCameraRotator) {
		mCameraRotator->SetLookAnglesDegrees(eulerDegrees.x, eulerDegrees.y);
	} else { mEntCameraRoot->GetTransform()->SetWorldRot(lookRotation); }
}

bool GameScene::IsOpeningSequenceActive() const {
	if (mIsDemoPlayback || mOpeningPhase == OpeningPhase::Gameplay) {
		return false;
	}
	if (mOpeningPhase == OpeningPhase::Countdown && mOpeningGameplayStarted) {
		return false;
	}
	return true;
}

bool GameScene::IsGameOverSequenceActive() const {
	return mGameOverPhase != GameOverPhase::None;
}

void GameScene::SetPlayerGameplayActive(const bool active) const {
	if (mEntPlayer) { mEntPlayer->SetActive(active); }
}

#ifdef _DEBUG
/// @brief デバッグHUDの描画
void GameScene::DrawDebugHud(
	const std::shared_ptr<CameraComponent>& camera
) const {
	if (mShowPosConVar) {
		const int flag = mShowPosConVar->GetValueAsInt();
		if (flag != 0) {
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
			constexpr ImGuiWindowFlags windowFlags =
				ImGuiWindowFlags_NoBackground |
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoDocking |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav;

			auto viewportLt = Unnamed::EngineServices::Get() ?
				                  Unnamed::EngineServices::Get()->
				                  GetViewportLTInstance() :
				                  Vec2{};
			const ImVec2 windowPos(viewportLt.x, viewportLt.y + 128.0f + 16.0f);
			ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);

			Mat4 inverseView = Mat4::identity;
			if (camera) { inverseView = camera->GetViewMat().Inverse(); }

			Vec3 cameraPosition = inverseView.GetTranslate();
			if (flag == 2) { cameraPosition = Math::MtoH(cameraPosition); }
			const Vec3 cameraRotation = inverseView.ToQuaternion().
			                                        ToEulerAngles();

			const std::string name = mNameConVar ?
				                         mNameConVar->GetValueAsString() :
				                         std::string("unknown");

			const std::string text = std::format(
				"name: {}\n"
				"pos : {:.2f} {:.2f} {:.2f}\n"
				"rot : {:.2f} {:.2f} {:.2f}\n"
				"vel : {:.2f}\n",
				name,
				cameraPosition.x, cameraPosition.y, cameraPosition.z,
				cameraRotation.x * Math::rad2Deg,
				cameraRotation.y * Math::rad2Deg,
				cameraRotation.z * Math::rad2Deg,
				mMovementComponent ?
					Math::MtoH(mMovementComponent->GetVelocity().Length()) :
					0.0f
			);

			const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
			ImGui::SetNextWindowSize(
				{textSize.x + 20.0f, textSize.y + 20.0f},
				ImGuiCond_Always
			);

			ImGui::Begin("##cl_showpos", nullptr, windowFlags);
			ImDrawList*  drawList = ImGui::GetWindowDrawList();
			const ImVec2 textPos  = ImGui::GetCursorPos();

			ImGuiUtil::TextOutlined(
				drawList,
				textPos,
				text.c_str(),
				ImGuiUtil::ToImVec4(kDebugHudTextColor),
				ImGuiUtil::ToImVec4(kDebugHudOutlineColor),
				1.0f
			);

			ImGui::PopStyleVar();
			ImGui::End();
		}
	}

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (!viewport) { return; }

	const ImVec2 windowCenter(
		viewport->Pos.x + viewport->Size.x * 0.5f,
		viewport->Pos.y + viewport->Size.y * 0.5f
	);

	ImDrawList*      drawList = ImGui::GetBackgroundDrawList();
	constexpr ImVec4 reticleColor(1.0f, 1.0f, 1.0f, 0.8f);
	constexpr ImVec4 outlineColor(0.0f, 0.0f, 0.0f, 0.5f);
	constexpr float  lineLength       = 10.0f;
	constexpr float  gapSize          = 3.0f;
	constexpr float  lineThickness    = 1.5f;
	constexpr float  outlineThickness = 0.5f;

	drawList->AddLine(
		{windowCenter.x - lineLength - gapSize, windowCenter.y},
		{windowCenter.x - gapSize, windowCenter.y},
		ImGui::ColorConvertFloat4ToU32(reticleColor),
		lineThickness
	);
	drawList->AddLine(
		{windowCenter.x + gapSize, windowCenter.y},
		{windowCenter.x + lineLength + gapSize, windowCenter.y},
		ImGui::ColorConvertFloat4ToU32(reticleColor),
		lineThickness
	);
	drawList->AddLine(
		{windowCenter.x, windowCenter.y - lineLength - gapSize},
		{windowCenter.x, windowCenter.y - gapSize},
		ImGui::ColorConvertFloat4ToU32(reticleColor),
		lineThickness
	);
	drawList->AddLine(
		{windowCenter.x, windowCenter.y + gapSize},
		{windowCenter.x, windowCenter.y + lineLength + gapSize},
		ImGui::ColorConvertFloat4ToU32(reticleColor),
		lineThickness
	);

	drawList->AddCircle(
		windowCenter,
		2.0f,
		ImGui::ColorConvertFloat4ToU32(outlineColor),
		0,
		outlineThickness + lineThickness
	);

	drawList->AddCircleFilled(
		windowCenter,
		1.0f,
		ImGui::ColorConvertFloat4ToU32(reticleColor)
	);
}
#endif

/// @brief シャットダウン
void GameScene::Shutdown() {
	// 先にCheckpointManagerを停止し、以降のコンポーネント破棄中に
	// Register/Unregisterが動かないようにする
	CheckpointManager::Shutdown();

	// オーディオの停止
	if (mWind) {
		mWind->Stop();
		mWind.reset();
	}
	if (mRun) {
		mRun->Stop();
		mRun.reset();
	}
	if (mDenySe) {
		mDenySe->Stop();
		mDenySe.reset();
	}

	// エフェクトの破棄
	mExplosionEffect.reset();
	mWindEffect.reset();

	// パーティクルの破棄
	mParticleObject.reset();
	mParticleEmitter.reset();

	// スプライトの破棄
	mGameOverOverlaySprite.reset();
	mOpeningFadeSprite.reset();
	mCountdownStartSprite.reset();
	mCountdownDigitSprite.reset();
	for (auto& timerSprite : mRaceTimerSprites) { timerSprite.reset(); }
	mNextCheckpointArrowSprite.reset();
	mNextCheckpointSprite.reset();

	// 物理エンジンからエンティティを登録解除してから破棄
	if (mUPhysicsEngine) {
		if (mEntWorldMesh) {
			mUPhysicsEngine->UnregisterEntity(mEntWorldMesh.get());
		}
		if (mFanEntity) { mUPhysicsEngine->UnregisterEntity(mFanEntity.get()); }
	}

	// エンティティリストをクリア（BaseScene側の生ポインタリスト）
	// unique_ptr で管理されているエンティティは delete しない
	mEntities.clear();

	// 各エンティティの破棄（unique_ptr で管理）
	// ※ CheckpointComponent / GoalComponent は Entity 破棄時にデストラクタで
	//    CheckpointManager::UnregisterCheckpoint() を呼ぶ
	mGoalEntity.reset();
	mCheckpointEntities.clear();
	mSpeedBoostAreaEntity2.reset();
	mSpeedBoostAreaEntity.reset();
	mJumpPadEntity2.reset();
	mJumpPadEntity.reset();
	mFanEntity.reset();
	mEntSkeletalMesh.reset();
	mEntViewmodelRoot.reset();
	mEntShakeRoot.reset();
	mEntWeapon.reset();
	mEntWorldMesh.reset();
	mEntPlayer.reset();
	mEntCameraRoot.reset();
	mCamera.reset();

	// コンポーネントの shared_ptr を解放
	mSkeletalMeshRenderer.reset();
	mFanMeshRenderer.reset();
	mWorldMeshRenderer.reset();
	mWeaponMeshRenderer.reset();
	mWeaponComponent.reset();
	mMovementComponent.reset();
	mCameraAnimator.reset();
	mCameraRotator = nullptr;

	// 物理エンジンの破棄
	mUPhysicsEngine.reset();

	// キューブマップの破棄
	mCubeMap.reset();

	// マネージャーポインタのクリア
	mResourceManager = nullptr;
	mSpriteCommon    = nullptr;
	mParticleManager = nullptr;
	mObject3DCommon  = nullptr;
	mModelCommon     = nullptr;
	mSrvManager      = nullptr;
	mAudioManager    = nullptr;
	mRenderer        = nullptr;
	mTimer           = nullptr;

	// ConVarポインタのクリア
	mShowPosConVar = nullptr;
	mNameConVar    = nullptr;
	mClearConVar   = nullptr;

	mRecordingTickAccumulatorSec  = 0.0f;
	mPendingReplayEdgeButtons     = 0u;
	mFanMovePhase                 = 100.0f;
	mOpeningPhase                 = OpeningPhase::Gameplay;
	mOpeningShotIndex             = 0;
	mOpeningShotElapsedSec        = 0.0f;
	mOpeningShotFadeElapsedSec    = 0.0f;
	mCountdownElapsedSec          = 0.0f;
	mOpeningFadeAlpha             = 0.0f;
	mOpeningFixedLookAngles       = Vec2::zero;
	mOpeningPlayerLookAngles      = Vec2::zero;
	mOpeningShotFadeActive        = false;
	mOpeningShotFadeSwapped       = false;
	mOpeningGameplayStarted       = false;
	mGameplayPresentationStarted  = false;
	mLastActivatedCheckpointCount = 0;
	mCheckpointSplits.clear();
	mPendingReturnToTitle        = false;
	mReturnToTitleRequestSent    = false;
	mReturnToTitleFadeElapsedSec = 0.0f;
	ResetGameOverState();
}

/// @brief ワールドメッシュのリロード
void GameScene::ReloadWorldMesh() {
	Console::Print("Starting world mesh reload...", kConTextColorCompleted);

	// リロード開始前にGPU処理の完了を待機
	if (mRenderer) {
		mRenderer->WaitPreviousFrame();
		Console::Print(
			"Initial GPU sync before mesh reload",
			kConTextColorCompleted
		);
	}

	try {
		// とりあえず安全な方法を試す
		SafeReloadWorldMesh();
	} catch (const std::exception& e) {
		Console::Print(
			std::string("Safe reload failed: ") + e.what(),
			kConTextColorError
		);
		Console::Print(
			"Attempting full entity recreation...",
			kConTextColorWarning
		);
		try { RecreateWorldMeshEntity(); } catch (...) {
			Console::Print("Full recreation also failed!", kConTextColorError);
		}
	} catch (...) {
		Console::Print(
			"Unknown exception during safe reload",
			kConTextColorError
		);
		Console::Print(
			"Attempting full entity recreation...",
			kConTextColorWarning
		);
		try { RecreateWorldMeshEntity(); } catch (...) {
			Console::Print("Full recreation also failed!", kConTextColorError);
		}
	}
}

/// @brief ワールドメッシュエンティティの再作成
void GameScene::RecreateWorldMeshEntity() {
	Console::Print("Recreating world mesh entity...", kConTextColorCompleted);

	// 古いエンティティを物理エンジンから登録解除
	if (mEntWorldMesh) {
		if (mUPhysicsEngine) {
			mUPhysicsEngine->UnregisterEntity(mEntWorldMesh.get());
		}
		RemoveEntity(mEntWorldMesh.get());
		Console::Print("Removed old world mesh entity", kConTextColorWarning);

		// shared_ptrをリセット
		mWorldMeshRenderer.reset();

		// unique_ptrをリセット
		mEntWorldMesh.reset();
	}

	// GPU処理の完了を待機（テクスチャロード前の同期）
	if (mRenderer) {
		mRenderer->WaitPreviousFrame();
		Console::Print(
			"Waited for GPU completion before entity recreation",
			kConTextColorCompleted
		);
	}

	// メッシュをリロード
	const std::string meshPath      = kWorldMeshReloadPath;
	bool              reloadSuccess = mResourceManager->GetMeshManager()->
		ReloadMeshFromFile(meshPath);

	if (!reloadSuccess) {
		Console::Print("Failed to reload mesh!", kConTextColorError);
		return;
	}

	// GPU処理の完了を再度待機（テクスチャロード後の同期）
	if (mRenderer) {
		mRenderer->WaitPreviousFrame();
		Console::Print(
			"Waited for GPU completion after mesh reload",
			kConTextColorCompleted
		);
	}

	// 新しいエンティティを作成
	mEntWorldMesh                  = std::make_unique<Entity>("worldMesh");
	StaticMeshRenderer* smRenderer = mEntWorldMesh->AddComponent<
		StaticMeshRenderer>();
	mWorldMeshRenderer = AdoptComponent(smRenderer);

	// 新しいメッシュを設定
	StaticMesh* newMesh = mResourceManager->GetMeshManager()->GetStaticMesh(
		meshPath
	);
	if (newMesh) {
		mWorldMeshRenderer->SetStaticMesh(newMesh);
		Console::Print("Set new mesh to new entity", kConTextColorCompleted);
	} else {
		Console::Print("Failed to get new mesh!", kConTextColorError);
		return;
	}

	// MeshColliderComponentを追加
	mEntWorldMesh->AddComponent<MeshColliderComponent>();
	AddEntity(mEntWorldMesh.get());

	// 物理エンジンに登録
	if (mUPhysicsEngine) {
		mUPhysicsEngine->RegisterEntity(mEntWorldMesh.get());
	}

	// 回転メッシュも物理エンジンに再登録（ワールド再生成で内部配列が変わる可能性に備える）
	ReRegisterRotateMeshes(
		mUPhysicsEngine.get(),
		mRotateMesh1.get(),
		mRotateMesh2.get()
	);

	Console::Print(
		"World mesh entity recreation completed!",
		kConTextColorCompleted
	);
}

/// @brief 安全なワールドメッシュのリロード
void GameScene::SafeReloadWorldMesh() {
	Console::Print("Safe reloading world mesh...", kConTextColorCompleted);

	if (!mEntWorldMesh) {
		Console::Print("World mesh entity does not exist!", kConTextColorError);
		return;
	}

	// 物理エンジンからエンティティの登録を解除
	if (mUPhysicsEngine) {
		mUPhysicsEngine->UnregisterEntity(mEntWorldMesh.get());
	}
	Console::Print(
		"Unregistered entity from physics engine",
		kConTextColorWarning
	);

	// MeshColliderComponentを削除
	if (mEntWorldMesh->HasComponent<MeshColliderComponent>()) {
		mEntWorldMesh->RemoveComponent<MeshColliderComponent>();
		Console::Print("Removed MeshColliderComponent", kConTextColorCompleted);
	}

	// GPU処理の完了を待機（テクスチャロード前の同期）
	if (mRenderer) {
		mRenderer->WaitPreviousFrame();
		Console::Print(
			"Waited for GPU completion before mesh reload",
			kConTextColorCompleted
		);
	}

	// メッシュをリロード
	const std::string meshPath      = kWorldMeshReloadPath;
	bool              reloadSuccess = mResourceManager->GetMeshManager()->
		ReloadMeshFromFile(meshPath);

	if (!reloadSuccess) {
		Console::Print("Failed to reload mesh!", kConTextColorError);
		// 失敗した場合は元のコンポーネントを復元
		mEntWorldMesh->AddComponent<MeshColliderComponent>();
		if (mUPhysicsEngine) {
			mUPhysicsEngine->RegisterEntity(mEntWorldMesh.get());
		}
		return;
	}

	// GPU処理の完了を再度待機（テクスチャロード後の同期）
	if (mRenderer) {
		mRenderer->WaitPreviousFrame();
		Console::Print(
			"Waited for GPU completion after mesh reload",
			kConTextColorCompleted
		);
	}

	// 新しいメッシュをレンダラーに設定
	StaticMesh* newMesh = mResourceManager->GetMeshManager()->GetStaticMesh(
		meshPath
	);
	if (newMesh && mWorldMeshRenderer) {
		mWorldMeshRenderer->SetStaticMesh(newMesh);
		Console::Print("Set new mesh to renderer", kConTextColorCompleted);
	} else {
		Console::Print(
			"Failed to get new mesh or renderer!",
			kConTextColorError
		);
		return;
	}

	// 新しいMeshColliderComponentを追加
	mEntWorldMesh->AddComponent<MeshColliderComponent>();
	Console::Print("Added new MeshColliderComponent", kConTextColorCompleted);

	// 物理エンジンに再登録
	if (mUPhysicsEngine) {
		mUPhysicsEngine->RegisterEntity(mEntWorldMesh.get());
	}

	// 回転メッシュも物理エンジンに再登録（ワールドの再登録後に実行しておく）
	ReRegisterRotateMeshes(
		mUPhysicsEngine.get(),
		mRotateMesh1.get(),
		mRotateMesh2.get()
	);
	Console::Print(
		"Re-registered entity to physics engine",
		kConTextColorCompleted
	);

	Console::Print("Safe world mesh reload completed!", kConTextColorCompleted);
}

void GameScene::QueueReturnToTitle() {
	if (mPendingReturnToTitle) { return; }

	mPendingReturnToTitle        = true;
	mReturnToTitleRequestSent    = false;
	mReturnToTitleFadeElapsedSec = 0.0f;
}
