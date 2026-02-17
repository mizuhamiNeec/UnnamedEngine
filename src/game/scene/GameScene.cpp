#include "GameScene.h"

#include <array>
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

#include <game/components/CameraRotator.h>
#include <game/components/RotateComponent.h>
#include <game/components/checkpoint/CheckpointComponent.h>
#include <game/components/checkpoint/CheckpointManager.h>
#include <game/components/checkpoint/GoalComponent.h>

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
		"./content/parkour/textures/uvChecker.png";
	constexpr char kColonTexturePath[] =
		"./content/parkour/textures/colon.png";
	constexpr char kDotTexturePath[] =
		"./content/parkour/textures/dot.png";
	constexpr char kWindParticleTexturePath[] =
		"./content/core/textures/circle.png";
	constexpr char kWeaponMeshPath[]   = "./content/core/models/weapon.obj";
	constexpr char kWeaponScriptPath[] =
		"./content/parkour/scripts/weapon_handgun.json";
	constexpr char kSkeletalMeshPath[] =
		"./content/parkour/models/hand/hand.gltf";
	constexpr char kFanMeshPath[] =
		"./content/core/models/fan.obj";
	constexpr char kWorldMeshInitialPath[] =
		"./content/parkour/models/map/sp_city.obj";
	constexpr char kWorldMeshReloadPath[] =
		"./content/parkour/models/map/sp_city.obj";
	constexpr char kAirAccelerateCommand[] =
		"sv_airaccelerate 100000000000000000";
	constexpr char  kMeshReloadBindCommand[] = "bind f5 +f5";
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

	template <typename T>
	std::shared_ptr<T> AdoptComponent(T* raw) {
		return std::shared_ptr<T>(raw, [](T*) {});
	}
}

/// @brief コンストラクタ
GameScene::~GameScene() {
	mResourceManager = nullptr;
	mSpriteCommon    = nullptr;
	mParticleManager = nullptr;
	mObject3DCommon  = nullptr;
	mModelCommon     = nullptr;
	mUPhysicsEngine  = nullptr;
	mTimer           = nullptr;
}

/// @brief 初期化
void GameScene::Init() {
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
	InitializeWorldMesh();
	InitializeFanMesh();
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

	const auto run = mAudioManager->GetAudio(
		"./content/parkour/sounds/bgm/Run.wav"
	);

#ifndef _DEBUG
	run->Play(true);
	run->SetVolume(0.125f);
#endif

	mWind = mAudioManager->GetAudio(
		"./content/parkour/sounds/amb/wind.wav"
	);
	mWind->Play(true);
	mWind->SetVolume(1.0f);

	// ゲームタイマー開始
	mTimer->StartGame();
}

/// @brief 更新
/// @param deltaTime 経過時間
void GameScene::Update(const float deltaTime) {
	HandleMeshReload();
	
	if (InputSystem::IsTriggered("escape")) { QueueReturnToTitle(); }

	// ファンを物理エンジンから登録解除
	if (mUPhysicsEngine) {
		mUPhysicsEngine->UnregisterEntity(mFanEntity.get());
	}
	if (mFanEntity->HasComponent<MeshColliderComponent>()) {
		mFanEntity->RemoveComponent<MeshColliderComponent>();
	}

	SyncCameraRoot();
	HandleWeaponInput();

	const auto camera = CameraManager::GetActiveCamera();
	HandleWeaponFire(camera);
	UpdateSkeletalAnimation();
	UpdatePlayer(deltaTime);
	UpdatePostProcessing(deltaTime);
	UpdateTeleport();
	UpdateParticlesAndEffects(deltaTime);
	UpdateEntities(deltaTime);

	auto* nextCheckpoint = CheckpointManager::GetNextCheckpoint();
	auto* goal           = mGoalEntity->GetComponent<GoalComponent>();

	if (!goal->IsReached()) {
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

	mNextCheckpointSprite->Update();
	mNextCheckpointArrowSprite->Update();

#ifdef _DEBUG
	// チェックポイントのデバッグ表示
	if (!mCheckpointEntities.empty()) {
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

	// if (mNextCheckpointSprite) {
	// 	if (mGoalEntity) {
	// 		auto* goal = mGoalEntity->GetComponent<GoalComponent>();
	// 		if (goal && goal->IsReached()) { return; }
	// 	}
	// 	mNextCheckpointSprite->Draw();
	// }
	//
	// if (mNextCheckpointArrowSprite) { mNextCheckpointArrowSprite->Draw(); }

	if (mPendingReturnToTitle) { mPendingReturnToTitle = false; }
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
		const char* path    = nullptr;
		bool        useSrgb = false;
	};

	constexpr std::array<TextureRequest, 9> requests{
		{
			{kDevMeasureTexturePath, false},
			{kUvCheckerTexturePath, false},
			{kWaveTexturePath, true},
			{kSmokeTexturePath, false},
			{kPingTexturePath, false},
			{kArrowTexturePath, false},
			{kDigitsAtlasPath, true},
			{kColonTexturePath, true},
			{kDotTexturePath, true},
		}
	};

	for (const auto& request : requests) {
		if (!request.path) { continue; }

		if (request.useSrgb) {
			texManager->LoadTexture(request.path, true);
		} else { texManager->LoadTexture(request.path); }
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

	const auto moveData = MovementData(
		32.0f, 72.0f
	);

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

	AddEntity(mFanEntity.get());
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
	}
}

/// @brief コンソールの設定
void GameScene::ConfigureConsole() {
	Console::SubmitCommand(kAirAccelerateCommand);
	Console::SubmitCommand(kMeshReloadBindCommand, true);

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

	Mat4       inverseView = camera->GetViewMat().Inverse();
	const Vec3 origin      = inverseView.GetTranslate();
	const Vec3 direction   = inverseView.GetForward();

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
	Mat4 cameraInvViewMat = CameraManager::GetActiveCamera()->GetViewMat().
		Inverse();
	Vec3 forward = cameraInvViewMat.GetForward();
	forward.y    = 0.0f; // 上下成分は無視
	forward.Normalize();
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

/// @brief エンティティの更新
/// @param deltaTime 経過時間
void GameScene::UpdateEntities(float deltaTime) {
	// 物理更新前の処理
	for (auto* entity : mEntities) {
		if (entity && !entity->GetParent()) { entity->PrePhysics(deltaTime); }
	}

	// ファンをSinCosで移動させる
	if (mFanEntity) {
		static float moveSpeed = 100.0f;

		const auto newPos = Vec3(
			std::sin(moveSpeed) * 20.0f,
			mFanEntity->GetTransform()->GetWorldPos().y,
			mFanEntity->GetTransform()->GetWorldPos().z
		);

		moveSpeed += deltaTime;

		mFanEntity->GetTransform()->SetWorldPos(newPos);
	}

	// 回転が適用された後に再登録
	mFanEntity->AddComponent<MeshColliderComponent>();
	if (mUPhysicsEngine) { mUPhysicsEngine->RegisterEntity(mFanEntity.get()); }

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

	if (ImGui::Begin("Time")) {
		auto time = mTimer->TotalTime();

		ImGui::Text(std::format("TotalTime: {:.2f}s", time).c_str());
	}
	ImGui::End();
}
#endif

/// @brief シャットダウン
void GameScene::Shutdown() {
	// CheckpointManagerをシャットダウン
	CheckpointManager::Shutdown();
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
	//	mPhysicsEngine->RegisterEntity(mEntWorldMesh.get(), true);
	if (mUPhysicsEngine) {
		mUPhysicsEngine->RegisterEntity(mEntWorldMesh.get());
	}

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
	Console::Print(
		"Re-registered entity to physics engine",
		kConTextColorCompleted
	);

	Console::Print("Safe world mesh reload completed!", kConTextColorCompleted);
}

void GameScene::QueueReturnToTitle() {
	if (mPendingReturnToTitle) { return; }

	mPendingReturnToTitle = true;
}
