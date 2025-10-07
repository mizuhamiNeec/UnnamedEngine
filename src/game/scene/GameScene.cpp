#include "GameScene.h"

#include <array>
#include <format>
#include <limits>
#include <string>

#include <engine/Engine.h>
#include <engine/Camera/CameraManager.h>
#include <engine/Components/Camera/CameraComponent.h>
#include <engine/Components/ColliderComponent/MeshColliderComponent.h>
#include <engine/Debug/Debug.h>
#include <engine/Debug/DebugHud.h>
#include <engine/ImGui/ImGuiUtil.h>
#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/ConVarManager.h>
#include <engine/subsystem/console/Log.h>
#include <engine/TextureManager/TexManager.h>

#include <game/components/CameraRotator.h>

namespace {
	constexpr char kDevMeasureTexturePath[] =
		"./content/core/textures/dev_measure.png";
	constexpr char kUvCheckerTexturePath[] =
		"./content/core/textures/uvChecker.png";
	constexpr char kWaveTexturePath[] =
		"./content/core/textures/wave.dds";
	constexpr char kSmokeTexturePath[] =
		"./content/core/textures/smoke.png";
	constexpr char kWindParticleTexturePath[] =
		"./resources/textures/circle.png";
	constexpr char kWeaponMeshPath[]   = "./content/core/models/weapon.obj";
	constexpr char kWeaponScriptPath[] =
		"./content/parkour/scripts/weapon_handgun.json";
	constexpr char kSkeletalMeshPath[] =
		"./content/parkour/models/man/man.gltf";
	constexpr char kWorldMeshInitialPath[] =
		"./content/core/models/reflectionTest.obj";
	constexpr char kWorldMeshReloadPath[] =
		"./resources/models/reflectionTest.obj";
	constexpr char kAirAccelerateCommand[] =
		"sv_airaccelerate 100000000000000000";
	constexpr char  kMeshReloadBindCommand[] = "bind f5 +f5";
	const Vec3      kShakeRootOffset(0.08f, -0.1f, 0.18f);
	constexpr float kCameraRootHeight  = 1.7f;
	constexpr float kPlayerSpawnHeight = 4.0f;
	const Vec3      kTeleportTriggerCenter(19.5072f, -29.2608f, 260.096f);
	const Vec3      kTeleportTriggerExtent(Vec3::one * 13.0048f);
	constexpr float kTeleportReenableBuffer    = 1.0f;
	constexpr float kExplosionNormalOffset     = 2.0f;
	constexpr int   kExplosionParticleCount    = 32;
	constexpr float kExplosionParticleLifetime = 30.0f;
	constexpr float kBlastMinSafeDistance      = 0.5f;
	constexpr float kBlurScale                 = 0.01f;
	constexpr float kBlastRadiusHu             = 512.0f;
	constexpr float kBlastPowerHu              = 1024.0f;

	template <typename T>
	std::shared_ptr<T> AdoptComponent(T* raw) {
		return std::shared_ptr<T>(raw, [](T*) {
		});
	}
}

GameScene::~GameScene() {
	mResourceManager = nullptr;
	mSpriteCommon    = nullptr;
	mParticleManager = nullptr;
	mObject3DCommon  = nullptr;
	mModelCommon     = nullptr;
	mUPhysicsEngine  = nullptr;
	mTimer           = nullptr;
}

void GameScene::Init() {
	mRenderer        = Unnamed::Engine::GetRenderer();
	mResourceManager = Unnamed::Engine::GetResourceManager();
	mSrvManager      = Unnamed::Engine::GetSrvManager();

	LoadCoreTextures();
	InitializeCubeMap();
	InitializeParticles();
	InitializePhysics();
	InitializeCamera();
	InitializePlayer();
	InitializeWeapon();
	InitializeWorldMesh();
	InitializeCameraRoot();
	InitializeShakeRoot();
	InitializeSkeletalMesh();
	ConfigureEntityHierarchy();
	InitializeEffects();
	ConfigureConsole();
	InitializeTeleportTrigger();
}

void GameScene::Update(const float deltaTime) {
	HandleMeshReload();
	SyncCameraRoot();
	HandleWeaponInput();

	const auto camera = CameraManager::GetActiveCamera();
	HandleWeaponFire(camera);
	UpdateSkeletalAnimation();
	UpdatePostProcessing(deltaTime);
	UpdateTeleport();
	UpdateParticlesAndEffects(deltaTime);
	UpdateEntities(deltaTime);

#ifdef _DEBUG
	DrawDebugHud(camera);
#endif
}

void GameScene::Render() {
	if (!mRenderer) {
		return;
	}

	auto* commandList = mRenderer->GetCommandList();

	if (mClearConVar && mClearConVar->GetValueAsBool() && mCubeMap) {
		mCubeMap->Render(commandList);
	}

	for (auto entity : mEntities) {
		if (entity) {
			entity->Render(commandList);
		}
	}

	if (auto* particleManager = Unnamed::Engine::GetParticleManager()) {
		particleManager->Render();
	}

	if (mParticleObject) {
		mParticleObject->Draw();
	}

	if (mWindEffect) {
		mWindEffect->Draw();
	}

	if (mExplosionEffect) {
		mExplosionEffect->Draw();
	}
}

void GameScene::LoadCoreTextures() const {
	auto* texManager = TexManager::GetInstance();
	if (!texManager) {
		return;
	}

	struct TextureRequest {
		const char* path    = nullptr;
		bool        useSrgb = false;
	};

	const std::array<TextureRequest, 4> requests{
		{
			{kDevMeasureTexturePath, false},
			{kUvCheckerTexturePath, false},
			{kWaveTexturePath, true},
			{kSmokeTexturePath, false},
		}
	};

	for (const auto& request : requests) {
		if (!request.path) {
			continue;
		}

		if (request.useSrgb) {
			texManager->LoadTexture(request.path, true);
		} else {
			texManager->LoadTexture(request.path);
		}
	}
}

void GameScene::InitializeCubeMap() {
	if (!mRenderer || !mSrvManager) {
		return;
	}

	mCubeMap = std::make_unique<CubeMap>(
		mRenderer->GetDevice(),
		mSrvManager,
		kWaveTexturePath
	);
}

void GameScene::InitializeParticles() {
	auto* particleManager = Unnamed::Engine::GetParticleManager();
	if (!particleManager) {
		return;
	}

	particleManager->CreateParticleGroup("wind", kWindParticleTexturePath);

	mParticleEmitter = std::make_unique<ParticleEmitter>();
	mParticleEmitter->Init(particleManager, "wind");

	mParticleObject = std::make_unique<ParticleObject>();
	mParticleObject->Init(particleManager, kWindParticleTexturePath);
}

void GameScene::InitializeEffects() {
	auto* particleManager = Unnamed::Engine::GetParticleManager();
	if (!particleManager) {
		return;
	}

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

void GameScene::InitializePhysics() {
	mUPhysicsEngine = std::make_unique<UPhysics::Engine>();
	mUPhysicsEngine->Init();
}

void GameScene::InitializeCamera() {
	mCamera = std::make_unique<Entity>("camera");
	AddEntity(mCamera.get());

	auto* rawCamera = mCamera->AddComponent<CameraComponent>();
	if (!rawCamera) {
		return;
	}

	const auto camera = AdoptComponent(rawCamera);
	CameraManager::AddCamera(camera);
	CameraManager::SetActiveCamera(camera);
}

void GameScene::InitializePlayer() {
	mEntPlayer = std::make_unique<Entity>("player");
	mEntPlayer->GetTransform()->SetLocalPos(Vec3::up * kPlayerSpawnHeight);

	auto* movement     = mEntPlayer->AddComponent<Movement>();
	mMovementComponent = AdoptComponent(movement);

	const auto moveData = MovementData(
		32.0f, 72.0f
	);

	if (mMovementComponent && mUPhysicsEngine) {
		mMovementComponent->Init(mUPhysicsEngine.get(), moveData);
	}

	AddEntity(mEntPlayer.get());
}

void GameScene::InitializeWeapon() {
	auto* meshManager = mResourceManager ?
		                    mResourceManager->GetMeshManager() :
		                    nullptr;
	if (meshManager) {
		meshManager->LoadMeshFromFile(kWeaponMeshPath);
	}

	mEntWeapon          = std::make_unique<Entity>("weapon");
	auto* renderer      = mEntWeapon->AddComponent<StaticMeshRenderer>();
	mWeaponMeshRenderer = AdoptComponent(renderer);
	if (mWeaponMeshRenderer && meshManager) {
		if (auto* mesh = meshManager->GetStaticMesh(kWeaponMeshPath)) {
			mWeaponMeshRenderer->SetStaticMesh(mesh);
		}
	}

	auto* weaponComponent = mEntWeapon->AddComponent<WeaponComponent>(
		kWeaponScriptPath);
	mWeaponComponent = AdoptComponent(weaponComponent);

	mEntWeapon->AddComponent<BoxColliderComponent>();

	auto* weaponSway = mEntWeapon->AddComponent<WeaponSway>();
	mWeaponSway      = AdoptComponent(weaponSway);

	AddEntity(mEntWeapon.get());
}

void GameScene::InitializeWorldMesh() {
	auto* meshManager = mResourceManager ?
		                    mResourceManager->GetMeshManager() :
		                    nullptr;
	if (meshManager) {
		meshManager->LoadMeshFromFile(kWorldMeshInitialPath);
	}

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

void GameScene::InitializeCameraRoot() {
	mEntCameraRoot = std::make_unique<Entity>("cameraRoot");
	mEntCameraRoot->GetTransform()->SetLocalPos(Vec3::up * kCameraRootHeight);
	mCameraRotator = mEntCameraRoot->AddComponent<CameraRotator>();
	AddEntity(mEntCameraRoot.get());
}

void GameScene::InitializeShakeRoot() {
	mEntShakeRoot = std::make_unique<Entity>("shakeRoot");
	
	// CameraAnimatorコンポーネントを追加
	auto* animator = mEntShakeRoot->AddComponent<CameraAnimator>();
	mCameraAnimator = AdoptComponent(animator);
	
	if (mCameraAnimator && mMovementComponent && mCameraRotator) {
		mCameraAnimator->Init(mMovementComponent.get(), mCameraRotator);
	}
}

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

	if (mSkeletalMeshRenderer && meshManager) {
		if (auto* mesh = meshManager->GetSkeletalMesh(kSkeletalMeshPath)) {
			mSkeletalMeshRenderer->SetSkeletalMesh(mesh);
		}
	}

	AddEntity(mEntSkeletalMesh.get());
}

void GameScene::ConfigureEntityHierarchy() {
	// 階層構造: CameraRoot -> ShakeRoot -> Camera -> Weapon
	// ShakeRootがカメラの親になることで、シェイクがカメラに伝わる
	
	if (mEntShakeRoot && mEntCameraRoot) {
		mEntShakeRoot->SetParent(mEntCameraRoot.get());
		mEntShakeRoot->GetTransform()->SetLocalPos(Vec3::zero);
		AddEntity(mEntShakeRoot.get()); // ShakeRootをシーンに追加
	}
	
	if (mCamera && mEntShakeRoot) {
		mCamera->SetParent(mEntShakeRoot.get());
		mCamera->GetTransform()->SetLocalPos(Vec3::zero);
	}

	if (mEntWeapon && mCamera) {
		mEntWeapon->SetParent(mCamera.get());
		mEntWeapon->GetTransform()->SetLocalPos(kShakeRootOffset);
	}

	if (mEntSkeletalMesh && mEntCameraRoot) {
		auto* transform = mEntSkeletalMesh->GetTransform();
		transform->SetLocalPos(transform->GetLocalPos() + Vec3::up * -0.2f);
		transform->SetLocalRot(Quaternion::EulerDegrees(0.0f, 180.0f, 0.0f));
		mEntSkeletalMesh->SetParent(mEntCameraRoot.get());
	}
}

void GameScene::ConfigureConsole() {
	Console::SubmitCommand(kAirAccelerateCommand);
	Console::SubmitCommand(kMeshReloadBindCommand, true);

	mShowPosConVar = ConVarManager::GetConVar("cl_showpos");
	mNameConVar    = ConVarManager::GetConVar("name");
	mClearConVar   = ConVarManager::GetConVar("r_clear");
}

void GameScene::InitializeTeleportTrigger() {
	const Vec3 triggerSize = kTeleportTriggerExtent * 2.0f;
	mTeleportTriggerMin    = kTeleportTriggerCenter - triggerSize * 0.5f;
	mTeleportTriggerMax    = kTeleportTriggerCenter + triggerSize * 0.5f;
	mTeleportActive        = true;
}

void GameScene::HandleMeshReload() {
	if (InputSystem::IsTriggered("+f5")) {
		mPendingMeshReload = true;
		mMeshReloadArmed   = false;
		Console::Print("Mesh reload requested...", kConTextColorCompleted);
	}

	if (!mPendingMeshReload) {
		return;
	}

	if (mMeshReloadArmed) {
		ReloadWorldMesh();
		mPendingMeshReload = false;
		mMeshReloadArmed   = false;
	} else {
		mMeshReloadArmed = true;
	}
}

void GameScene::SyncCameraRoot() const {
	if (!mEntCameraRoot || !mMovementComponent) {
		return;
	}

	mEntCameraRoot->GetTransform()->SetWorldPos(
		mMovementComponent->GetHeadPos());
}

void GameScene::HandleWeaponInput() {
	if (!mWeaponComponent) {
		return;
	}

	if (InputSystem::IsPressed("+attack1")) {
		mWeaponComponent->PullTrigger();
	}
	if (InputSystem::IsReleased("+attack1")) {
		mWeaponComponent->ReleaseTrigger();
	}
	if (InputSystem::IsPressed("+reload") && mEntPlayer) {
		mEntPlayer->GetTransform()->SetWorldPos(Vec3::up * 2.0f);
	}
}

void GameScene::HandleWeaponFire(
	const std::shared_ptr<CameraComponent>& camera) {
	if (!mWeaponComponent || !mWeaponComponent->HasFiredThisFrame() || !
		mUPhysicsEngine) {
		return;
	}

	if (!camera) {
		return;
	}

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
	if (!mUPhysicsEngine->RayCast(ray, &hit)) {
		return;
	}

	if (mExplosionEffect) {
		mExplosionEffect->TriggerExplosion(
			hit.pos + hit.normal * kExplosionNormalOffset,
			hit.normal,
			kExplosionParticleCount,
			kExplosionParticleLifetime
		);
	}

	if (!mEntPlayer || !mMovementComponent) {
		return;
	}

	const Vec3  playerPos   = mEntPlayer->GetTransform()->GetWorldPos();
	const float blastRadius = Math::HtoM(kBlastRadiusHu);
	const float blastPower  = Math::HtoM(kBlastPowerHu);
	const Vec3  toPlayer    = playerPos - hit.pos;
	const float distance    = toPlayer.Length();

	if (distance < blastRadius && distance > kBlastMinSafeDistance) {
		const Vec3  forceDir = toPlayer.Normalized();
		const float force    = blastPower * (1.0f - (distance / blastRadius));
		mMovementComponent->SetVelocity(
			mMovementComponent->GetVelocity() + forceDir * force);
	}
}

void GameScene::UpdateSkeletalAnimation() {
	if (!mSkeletalMeshRenderer || !mMovementComponent) {
		return;
	}

	const Vec3 velocity = mMovementComponent->GetVelocity();
	const Vec3 horizontalVelocity(velocity.x, 0.0f, velocity.z);
	mSkeletalMeshRenderer->
		SetAnimationSpeed(horizontalVelocity.Length() * 0.1f);
}

void GameScene::UpdatePostProcessing(float deltaTime) {
	const float blurStrength = mMovementComponent ?
		                           mMovementComponent->GetVelocity().
		                           Length() * kBlurScale :
		                           0.0f;

	Unnamed::Engine::blurStrength = blurStrength;

	if (mClearConVar && mClearConVar->GetValueAsBool() && mCubeMap) {
		mCubeMap->Update(deltaTime);
	}
}

void GameScene::UpdateTeleport() {
	if (!mEntPlayer) {
		return;
	}

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
	Debug::DrawBox(
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

		if (outside) {
			mTeleportActive = true;
		}
	}
}

void GameScene::UpdateParticlesAndEffects(float deltaTime) {
	if (auto* particleManager = Unnamed::Engine::GetParticleManager()) {
		particleManager->Update(deltaTime);
	}

	if (mParticleEmitter) {
		mParticleEmitter->Update(deltaTime);
	}

	if (mWindEffect) {
		mWindEffect->Update(deltaTime);
	}

	if (mExplosionEffect) {
		mExplosionEffect->Update(deltaTime);
	}
}

void GameScene::UpdateEntities(float deltaTime) {
	for (auto entity : mEntities) {
		if (entity && !entity->GetParent()) {
			entity->PrePhysics(deltaTime);
		}
	}

	for (auto entity : mEntities) {
		if (entity && !entity->GetParent()) {
			entity->Update(deltaTime);
		}
	}

	if (mUPhysicsEngine) {
		mUPhysicsEngine->Update(deltaTime);
	}

	for (auto entity : mEntities) {
		if (entity && !entity->GetParent()) {
			entity->PostPhysics(deltaTime);
		}
	}
}

#ifdef _DEBUG
void GameScene::DrawDebugHud(
	const std::shared_ptr<CameraComponent>& camera) const {
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

			auto         viewportLt = Unnamed::Engine::GetViewportLT();
			const ImVec2 windowPos(viewportLt.x, viewportLt.y + 128.0f + 16.0f);
			ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);

			Mat4 inverseView = Mat4::identity;
			if (camera) {
				inverseView = camera->GetViewMat().Inverse();
			}

			Vec3 cameraPosition = inverseView.GetTranslate();
			if (flag == 2) {
				cameraPosition = Math::MtoH(cameraPosition);
			}
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
			ImGui::SetNextWindowSize({textSize.x + 20.0f, textSize.y + 20.0f},
			                         ImGuiCond_Always);

			ImGui::Begin("##cl_showpos", nullptr, windowFlags);
			ImDrawList*  drawList = ImGui::GetWindowDrawList();
			const ImVec2 textPos  = ImGui::GetCursorPos();

			ImGuiManager::TextOutlined(
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
	if (!viewport) {
		return;
	}

	const ImVec2 windowCenter(
		viewport->Pos.x + viewport->Size.x * 0.5f,
		viewport->Pos.y + viewport->Size.y * 0.5f
	);

	ImDrawList*     drawList = ImGui::GetBackgroundDrawList();
	const ImVec4    reticleColor(1.0f, 1.0f, 1.0f, 0.8f);
	const ImVec4    outlineColor(0.0f, 0.0f, 0.0f, 0.5f);
	constexpr float lineLength       = 10.0f;
	constexpr float gapSize          = 3.0f;
	constexpr float lineThickness    = 1.5f;
	constexpr float outlineThickness = 0.5f;

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

void GameScene::Shutdown() {
	//cubeMap_.reset();
}

void GameScene::ReloadWorldMesh() {
	Console::Print("Starting world mesh reload...", kConTextColorCompleted);

	// リロード開始前にGPU処理の完了を待機
	if (mRenderer) {
		mRenderer->WaitPreviousFrame();
		Console::Print("Initial GPU sync before mesh reload",
		               kConTextColorCompleted);
	}

	try {
		// まず安全な方法を試す
		SafeReloadWorldMesh();
	} catch (const std::exception& e) {
		Console::Print(std::string("Safe reload failed: ") + e.what(),
		               kConTextColorError);
		Console::Print("Attempting full entity recreation...",
		               kConTextColorWarning);
		try {
			RecreateWorldMeshEntity();
		} catch (...) {
			Console::Print("Full recreation also failed!", kConTextColorError);
		}
	} catch (...) {
		Console::Print("Unknown exception during safe reload",
		               kConTextColorError);
		Console::Print("Attempting full entity recreation...",
		               kConTextColorWarning);
		try {
			RecreateWorldMeshEntity();
		} catch (...) {
			Console::Print("Full recreation also failed!", kConTextColorError);
		}
	}
}

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
		Console::Print("Waited for GPU completion before entity recreation",
		               kConTextColorCompleted);
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
		Console::Print("Waited for GPU completion after mesh reload",
		               kConTextColorCompleted);
	}

	// 新しいエンティティを作成
	mEntWorldMesh                  = std::make_unique<Entity>("worldMesh");
	StaticMeshRenderer* smRenderer = mEntWorldMesh->AddComponent<
		StaticMeshRenderer>();
	mWorldMeshRenderer = AdoptComponent(smRenderer);

	// 新しいメッシュを設定
	StaticMesh* newMesh = mResourceManager->GetMeshManager()->GetStaticMesh(
		meshPath);
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

	Console::Print("World mesh entity recreation completed!",
	               kConTextColorCompleted);
}

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
	Console::Print("Unregistered entity from physics engine",
	               kConTextColorWarning);

	// MeshColliderComponentを削除
	if (mEntWorldMesh->HasComponent<MeshColliderComponent>()) {
		mEntWorldMesh->RemoveComponent<MeshColliderComponent>();
		Console::Print("Removed MeshColliderComponent", kConTextColorCompleted);
	}

	// GPU処理の完了を待機（テクスチャロード前の同期）
	if (mRenderer) {
		mRenderer->WaitPreviousFrame();
		Console::Print("Waited for GPU completion before mesh reload",
		               kConTextColorCompleted);
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
		Console::Print("Waited for GPU completion after mesh reload",
		               kConTextColorCompleted);
	}

	// 新しいメッシュをレンダラーに設定
	StaticMesh* newMesh = mResourceManager->GetMeshManager()->GetStaticMesh(
		meshPath);
	if (newMesh && mWorldMeshRenderer) {
		mWorldMeshRenderer->SetStaticMesh(newMesh);
		Console::Print("Set new mesh to renderer", kConTextColorCompleted);
	} else {
		Console::Print("Failed to get new mesh or renderer!",
		               kConTextColorError);
		return;
	}

	// 新しいMeshColliderComponentを追加
	mEntWorldMesh->AddComponent<MeshColliderComponent>();
	Console::Print("Added new MeshColliderComponent", kConTextColorCompleted);

	// 物理エンジンに再登録
	if (mUPhysicsEngine) {
		mUPhysicsEngine->RegisterEntity(mEntWorldMesh.get());
	}
	Console::Print("Re-registered entity to physics engine",
	               kConTextColorCompleted);

	Console::Print("Safe world mesh reload completed!", kConTextColorCompleted);
}
