#include "GameScene.h"

#include <format>

#include <engine/Engine.h>
#include <engine/Camera/CameraManager.h>
#include <engine/Components/ColliderComponent/MeshColliderComponent.h>
#include <engine/Debug/Debug.h>
#include <engine/Debug/DebugHud.h>
#include <engine/ImGui/ImGuiUtil.h>
#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/ConVarManager.h>
#include <engine/TextureManager/TexManager.h>

#include "core/guidgenerator/GUIDGenerator.h"
#include "core/json/JsonWriter.h"

#include "engine/gameframework/component/Transform/TransformComponent.h"
#include "engine/gameframework/entity/UEntity/UEntity.h"
#include "engine/subsystem/console/Log.h"

#include "game/components/CameraRotator.h"

namespace Unnamed {
	class UEntity;
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

#pragma region テクスチャ読み込み
	TexManager::GetInstance()->LoadTexture(
		"./content/core/textures/dev_measure.png"
	);

	TexManager::GetInstance()->LoadTexture(
		"./content/core/textures/uvChecker.png"
	);

	TexManager::GetInstance()->LoadTexture(
		"./content/core/textures/wave.dds", true
	);

	TexManager::GetInstance()->LoadTexture(
		"./content/core/textures/smoke.png"
	);

#pragma endregion

#pragma region スプライト類
#pragma endregion

#pragma region 3Dオブジェクト類
	mResourceManager->GetMeshManager()->LoadMeshFromFile(
		"./content/core/models/reflectionTest.obj");

	mResourceManager->GetMeshManager()->LoadMeshFromFile(
		"./content/core/models/weapon.obj");

	mCubeMap = std::make_unique<CubeMap>(
		mRenderer->GetDevice(),
		mSrvManager,
		"./resources/textures/wave.dds"
	);

	mResourceManager->GetMeshManager()->LoadSkeletalMeshFromFile(
		"./content/parkour/models/man/man.gltf"
	);
#pragma endregion

#pragma region パーティクル類
	// パーティクルグループの作成
	Unnamed::Engine::GetParticleManager()->CreateParticleGroup(
		"wind", "./resources/textures/circle.png");

	mParticleEmitter = std::make_unique<ParticleEmitter>();
	mParticleEmitter->Init(Unnamed::Engine::GetParticleManager(), "wind");

	mParticleObject = std::make_unique<ParticleObject>();
	mParticleObject->Init(Unnamed::Engine::GetParticleManager(),
	                      "./resources/textures/circle.png");

#pragma endregion

#pragma region 物理エンジン
	// mPhysicsEngine = std::make_unique<PhysicsEngine>();
	// mPhysicsEngine->Init();
	mUPhysicsEngine = std::make_unique<UPhysics::Engine>();
	mUPhysicsEngine->Init();
#pragma endregion

#pragma region エンティティ
	mCamera = std::make_unique<Entity>("camera");
	AddEntity(mCamera.get());
	// 生ポインタを取得
	CameraComponent* rawCameraPtr = mCamera->AddComponent<CameraComponent>();
	// 生ポインタを std::shared_ptr に変換
	const auto camera = std::shared_ptr<CameraComponent>(
		rawCameraPtr, [](CameraComponent*) {
		}
	);

	// カメラを CameraManager に追加
	CameraManager::AddCamera(camera);
	// アクティブカメラに設定
	CameraManager::SetActiveCamera(camera);

	// プレイヤー
	mEntPlayer = std::make_unique<Entity>("player");
	mEntPlayer->GetTransform()->SetLocalPos(Vec3::up * 4.0f); // 1m上に配置
	GameMovementComponent* rawPlayerMovement = mEntPlayer->AddComponent<
		GameMovementComponent>();
	mGameMovementComponent = std::shared_ptr<GameMovementComponent>(
		rawPlayerMovement, [](GameMovementComponent*) {
		}
	);
	//mPlayerMovementUPhysics->SetVelocity(Vec3::forward * 1.0f);
	//mPlayerMovementUPhysics->AddCameraShakeEntity(mCamera.get());
	mGameMovementComponent->SetUPhysicsEngine(mUPhysicsEngine.get());
	BoxColliderComponent* rawPlayerCollider = mEntPlayer->AddComponent<
		BoxColliderComponent>();
	mPlayerCollider = std::shared_ptr<BoxColliderComponent>(
		rawPlayerCollider, [](BoxColliderComponent*) {
		}
	);
	mPlayerCollider->SetSize(Math::HtoM(Vec3(33.0f, 73.0f, 33.0f)));
	mPlayerCollider->SetOffset(Math::HtoM(Vec3::up * 73.0f * 0.5f));
	AddEntity(mEntPlayer.get());

	mEntWeapon                         = std::make_unique<Entity>("weapon");
	StaticMeshRenderer* weaponRenderer = mEntWeapon->AddComponent<
		StaticMeshRenderer>();
	mWeaponMeshRenderer = std::shared_ptr<StaticMeshRenderer>(
		weaponRenderer, [](StaticMeshRenderer*) {
		}
	);
	WeaponComponent* rawWeaponComponent = mEntWeapon->AddComponent<
		WeaponComponent>("./content/parkour/scripts/weapon_handgun.json");
	mWeaponComponent = std::shared_ptr<WeaponComponent>(
		rawWeaponComponent, [](WeaponComponent*) {
		}
	);

	mEntWeapon->AddComponent<BoxColliderComponent>();

	mWeaponMeshRenderer->SetStaticMesh(
		mResourceManager->GetMeshManager()->GetStaticMesh(
			"./resources/models/weapon.obj"));
	WeaponSway* rawWeaponSway = mEntWeapon->AddComponent<WeaponSway>();
	mWeaponSway               = std::shared_ptr<WeaponSway>(
		rawWeaponSway, [](WeaponSway*) {
		}
	);
	AddEntity(mEntWeapon.get());

	mEntShakeRoot = std::make_unique<Entity>("shakeRoot");
	//mPlayerMovementUPhysics->AddCameraShakeEntity(mEntShakeRoot.get(), 3.0f);

	// テスト用メッシュ
	mEntWorldMesh                  = std::make_unique<Entity>("testMesh");
	StaticMeshRenderer* smRenderer = mEntWorldMesh->AddComponent<
		StaticMeshRenderer>();
	mWorldMeshRenderer = std::shared_ptr<StaticMeshRenderer>(
		smRenderer, [](StaticMeshRenderer*) {
		}
	);
	smRenderer->SetStaticMesh(
		mResourceManager->GetMeshManager()->GetStaticMesh(
			"./resources/models/reflectionTest.obj"));
	mEntWorldMesh->AddComponent<MeshColliderComponent>();
	AddEntity(mEntWorldMesh.get());

	// カメラの親エンティティ
	mEntCameraRoot = std::make_unique<Entity>("cameraRoot");
	//cameraRoot_->SetParent(entPlayer_.get());
	mEntCameraRoot->GetTransform()->SetLocalPos(Vec3::up * 1.7f);
	mCameraRotator = mEntCameraRoot->AddComponent<CameraRotator>();
	AddEntity(mEntCameraRoot.get());

	// cameraRootにアタッチ
	mCamera->SetParent(mEntCameraRoot.get());
	mCamera->GetTransform()->SetLocalPos(Vec3::zero); // FPS

	mEntShakeRoot->SetParent(mCamera.get());

	mEntWeapon->SetParent(mEntShakeRoot.get());
	mEntShakeRoot->GetTransform()->SetLocalPos(Vec3(0.08f, -0.1f, 0.18f));
	mEntWeapon->GetTransform()->SetLocalPos(Vec3::zero);


	mEntSkeletalMesh = std::make_unique<Entity>("SkeletalMeshEntity");
	auto sklMesh = mEntSkeletalMesh->AddComponent<SkeletalMeshRenderer>();
	mSkeletalMeshRenderer = std::shared_ptr<SkeletalMeshRenderer>(
		sklMesh, [](SkeletalMeshRenderer*) {
		}
	);

	auto skeletalMesh = mResourceManager->GetMeshManager()->GetSkeletalMesh(
		"./resources/models/man/man.gltf"
	);
	sklMesh->SetSkeletalMesh(skeletalMesh);

	AddEntity(mEntSkeletalMesh.get());

	mEntSkeletalMesh->SetParent(mEntCameraRoot.get());
	mEntSkeletalMesh->GetTransform()->SetLocalPos(
		mEntSkeletalMesh->GetTransform()->GetLocalPos() + Vec3::up * -0.2f);
	mEntSkeletalMesh->GetTransform()->SetLocalRot(
		Quaternion::EulerDegrees(0.0f, 180.0f, 0.0f));

#pragma endregion

	// 風
	mWindEffect = std::make_unique<WindEffect>();
	mWindEffect->Init(Unnamed::Engine::GetParticleManager(),
	                  mGameMovementComponent.get());

	// 爆発
	mExplosionEffect = std::make_unique<ExplosionEffect>();
	mExplosionEffect->Init(Unnamed::Engine::GetParticleManager(),
	                       "./resources/textures/smoke.png");
	mExplosionEffect->SetColorGradient(
		Vec4(0.78f, 0.29f, 0.05f, 1.0f), Vec4(0.04f, 0.04f, 0.05f, 1.0f));

#pragma region コンソール変数/コマンド
#pragma endregion

#pragma region メッシュレンダラー
#pragma endregion

	CameraManager::SetActiveCamera(camera);

	//	mPhysicsEngine->RegisterEntity(mEntWorldMesh.get(), true);
	mUPhysicsEngine->RegisterEntity(mEntWorldMesh.get());

	// // 物理エンジンにプレイヤーエンティティを登録
	// mPhysicsEngine->RegisterEntity(mEntPlayer.get());
	//
	// mPhysicsEngine->RegisterEntity(mEntWeapon.get());

	Console::SubmitCommand(
		"sv_airaccelerate 100000000000000000"
	);

	// F5キーをバインド
	Console::SubmitCommand("bind f5 +f5", true);

	// テレポートトリガー領域の設定
	Vec3 triggerCenter(19.5072f, -29.2608f, 260.096f); // トリガーの中心位置
	Vec3 triggerSize(Vec3::one * 13.0048f * 2.0f);     // トリガーのサイズ
	mTeleportTriggerMin = triggerCenter - triggerSize * 0.5f;
	mTeleportTriggerMax = triggerCenter + triggerSize * 0.5f;

	{
		auto testEntity = std::make_unique<
			Unnamed::UEntity>("TestEntity");

		auto testRootEntity = std::make_unique<
			Unnamed::UEntity>("TestRootEntity");

		auto transform = testEntity->AddComponent<
			Unnamed::TransformComponent>();

		auto testRootTransform = testRootEntity->AddComponent<
			Unnamed::TransformComponent>();

		transform->SetParent(testRootTransform);

		testRootTransform->SetPosition(Vec3(114.0f, 514.0f, 1919.0f));
		testRootTransform->SetRotation(
			Quaternion::EulerDegrees(0.0f, 45.0f, 0.0f)
		);
		testRootTransform->SetScale(Vec3(1.0f, 1.0f, 1.0f));

		transform->SetPosition(Vec3(123.0f, -456.0f, 789.0f));
		transform->SetRotation(Quaternion::EulerDegrees(0.0f, 90.0f, 0.0f));
		transform->SetScale(Vec3(1.0f, 2.0f, 3.0f));

		Msg(
			testEntity->GetName(),
			"TransformComponent のデータ: \n {} {} {},\n {} {} {},\n {} {} {}",
			transform->Position().x,
			transform->Position().y,
			transform->Position().z,
			transform->Rotation().ToEulerAngles().x,
			transform->Rotation().ToEulerAngles().y,
			transform->Rotation().ToEulerAngles().z,
			transform->Scale().x,
			transform->Scale().y,
			transform->Scale().z
		);

		JsonWriter writer("test");

		writer.BeginObject();
		transform->Serialize(writer);
		writer.EndObject();

		Msg(
			"JsonWriter",
			"TransformComponent のシリアライズ結果: \n{}",
			writer.ToString()
		);

		auto guidGenerator = std::make_unique<
			GuidGenerator>(GuidGenerator::MODE::RANDOM64);
		for (int i = 0; i < 128; ++i) {
			Msg(
				"GuidGenerator",
				"Generated GUID: {}",
				guidGenerator->Alloc()
			);
		}
	}
}

void GameScene::Update(const float deltaTime) {
	// F5キーでメッシュリロードをリクエスト
	if (InputSystem::IsTriggered("+f5")) {
		mPendingMeshReload = true;
		Console::Print("Mesh reload requested...", kConTextColorCompleted);
	}

	if (mPendingMeshReload) {
		static bool reloadNextFrame = false;
		if (reloadNextFrame) {
			ReloadWorldMesh();
			mPendingMeshReload = false;
			reloadNextFrame    = false;
		} else {
			reloadNextFrame = true;
		}
	}

	mEntCameraRoot->GetTransform()->SetWorldPos(
		mGameMovementComponent->GetHeadPos());
	// cameraRoot_->Update(EngineTimer::GetScaledDeltaTime());
	// camera_->Update(EngineTimer::GetScaledDeltaTime());

	if (InputSystem::IsPressed("+attack1")) {
		mWeaponComponent->PullTrigger();
	}
	if (InputSystem::IsReleased("+attack1")) {
		mWeaponComponent->ReleaseTrigger();
	}
	if (InputSystem::IsPressed("+reload")) {
		mEntPlayer->GetTransform()->SetWorldPos(Vec3::up * 2.0f);
	}

	//mEntWeapon->Update(EngineTimer::GetScaledDeltaTime());
	//mWeaponComponent->Update(EngineTimer::GetScaledDeltaTime());
	if (mWeaponComponent->HasFiredThisFrame()) {
		auto camera = CameraManager::GetActiveCamera();
		auto cameraForward = camera->GetViewMat().Inverse().GetForward();
		Unnamed::Ray ray = {
			camera->GetViewMat().Inverse().GetTranslate(),
			cameraForward,
			1.0f / cameraForward,
			0.0f,
			100.0f
		};
		UPhysics::Hit hit;

		bool isHit = mUPhysicsEngine->RayCast(
			ray,
			&hit
		);
		if (isHit) {
			Vec3 hitPos    = hit.pos;
			Vec3 hitNormal = hit.normal;

			// 爆発エフェクト
			mExplosionEffect->TriggerExplosion(hitPos + hitNormal * 2.0f,
			                                   hitNormal,
			                                   32, 30.0f);

			// プレイヤーの位置と爆心地の距離を計算
			Vec3  playerPos   = mEntPlayer->GetTransform()->GetWorldPos();
			float blastRadius = Math::HtoM(512);     // 球状ゾーンの半径（例: 5m）
			float blastPower  = Math::HtoM(1024.0f); // 吹き飛ばしの強さ

			Vec3  toPlayer = playerPos - hitPos;
			float distance = toPlayer.Length();

			if (distance < blastRadius && distance > 0.5f) {
				Vec3  forceDir = toPlayer.Normalized();
				float force    = blastPower * (1.0f - (distance / blastRadius));
				// プレイヤーに吹き飛ばしベクトルを加える
				mGameMovementComponent->SetVelocity(
					mGameMovementComponent->Velocity() + forceDir * force);
			}

			// カメラシェイク
			// mGameMovementComponent->StartCameraShake(
			// 	0.1f, 0.1f, 5.0f, Vec3::forward, 0.025f, 3.0f,
			// 	Vec3::right
			// );
		}
	}

	auto horizontalplayervel = Vec3(mGameMovementComponent->Velocity().x,
	                                0.0f,
	                                mGameMovementComponent->Velocity().z);

	mSkeletalMeshRenderer->SetAnimationSpeed(
		horizontalplayervel.Length() * 0.1f
	);

	Unnamed::Engine::blurStrength = mGameMovementComponent->Velocity().
		Length() *
		0.01f;
	// mWeaponMeshRenderer->Update(EngineTimer::GetScaledDeltaTime());
	// mWeaponSway->Update(EngineTimer::GetScaledDeltaTime());
	//
	// mEntPlayer->Update(EngineTimer::GetScaledDeltaTime());
	// mEntShakeRoot->Update(EngineTimer::GetScaledDeltaTime());
	//
	// mEntWorldMesh->Update(deltaTime);

#ifdef _DEBUG
#pragma region cl_showpos
	if (
		int flag = ConVarManager::GetConVar("cl_showpos")->GetValueAsInt();
		flag != 0
	) {
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
		auto viewportLt = Unnamed::Engine::GetViewportLT();
		auto windowPos  = ImVec2(viewportLt.x, viewportLt.y + 128.0f + 16.0f);
		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);

		Mat4 invViewMat = Mat4::identity;
		if (CameraManager::GetActiveCamera()) {
			invViewMat = CameraManager::GetActiveCamera()->GetViewMat().
				Inverse();
		}

		Vec3 camPos = invViewMat.GetTranslate();
		if (flag == 2) {
			camPos = Math::MtoH(camPos);
		}
		const Vec3 camRot = invViewMat.ToQuaternion().ToEulerAngles();

		// テキストのサイズを取得
		std::string text = std::format(
			"name: {}\n"
			"pos : {:.2f} {:.2f} {:.2f}\n"
			"rot : {:.2f} {:.2f} {:.2f}\n"
			"vel : {:.2f}\n",
			ConVarManager::GetConVar("name")->GetValueAsString(),
			camPos.x, camPos.y, camPos.z,
			camRot.x * Math::rad2Deg,
			camRot.y * Math::rad2Deg,
			camRot.z * Math::rad2Deg,
			Math::MtoH(mGameMovementComponent->Velocity().Length())
		);

		//Console::Print(text);

		ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

		// ウィンドウサイズをテキストサイズに基づいて設定
		ImVec2 windowSize = ImVec2(textSize.x + 20.0f, textSize.y + 20.0f);
		// 余白を追加
		ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

		ImGui::Begin("##cl_showpos", nullptr, windowFlags);

		ImVec2      textPos     = ImGui::GetCursorPos();
		ImDrawList* drawList    = ImGui::GetWindowDrawList();
		float       outlineSize = 1.0f;

		ImGuiManager::TextOutlined(
			drawList,
			textPos,
			text.c_str(),
			ImGuiUtil::ToImVec4(kDebugHudTextColor),
			ImGuiUtil::ToImVec4(kDebugHudOutlineColor),
			outlineSize
		);

		ImGui::PopStyleVar();
		ImGui::End();
	}
#pragma endregion
#endif

	// プレイヤーの位置を取得
	Vec3 playerPos = mEntPlayer->GetTransform()->GetWorldPos();

	// AABBの内部にいるかチェック
	if (mTeleportActive &&
		playerPos.x >= mTeleportTriggerMin.x && playerPos.x <=
		mTeleportTriggerMax.x &&
		playerPos.y >= mTeleportTriggerMin.y && playerPos.y <=
		mTeleportTriggerMax.y &&
		playerPos.z >= mTeleportTriggerMin.z && playerPos.z <=
		mTeleportTriggerMax.z) {
		// プレイヤーを原点にテレポート
		mEntPlayer->GetTransform()->SetWorldPos(Vec3::zero);

		// 連続テレポートを防ぐための一時的な無効化（オプション）
		mTeleportActive = false;

		// デバッグ情報の出力
		Console::Print("テレポートしました！");
	}

	Unnamed::AABB teleportTriggerAABB(
		mTeleportTriggerMin, mTeleportTriggerMax);

	Debug::DrawBox(
		teleportTriggerAABB.Center(),
		Quaternion::identity,
		teleportTriggerAABB.Size(),
		Vec4(1.0f, 0.0f, 0.0f, 0.5f) // 赤色、半透明
	);

	// テレポート機能の再有効化（一定時間後または条件付きで）
	if (!mTeleportActive) {
		// 例: プレイヤーがトリガー領域から十分離れたら再度有効化
		if (playerPos.x < mTeleportTriggerMin.x - 1.0f || playerPos.x >
			mTeleportTriggerMax.x + 1.0f ||
			playerPos.y < mTeleportTriggerMin.y - 1.0f || playerPos.y >
			mTeleportTriggerMax.y + 1.0f ||
			playerPos.z < mTeleportTriggerMin.z - 1.0f || playerPos.z >
			mTeleportTriggerMax.z + 1.0f) {
			mTeleportActive = true;
		}
	}

	Unnamed::Engine::GetParticleManager()->Update(deltaTime);
	mParticleEmitter->Update(deltaTime);

#ifdef _DEBUG
	// レティクルの描画
	ImVec2 windowCenter = ImVec2(
		ImGui::GetMainViewport()->Pos.x + ImGui::GetMainViewport()->Size.x *
		0.5f,
		ImGui::GetMainViewport()->Pos.y + ImGui::GetMainViewport()->Size.y *
		0.5f);

	ImDrawList* drawList = ImGui::GetBackgroundDrawList();

	// レティクルの設定
	const ImVec4 reticleColor     = ImVec4(1.0f, 1.0f, 1.0f, 0.8f); // 白色、少し透明
	const ImVec4 outlineColor     = ImVec4(0.0f, 0.0f, 0.0f, 0.5f); // 黒の縁取り
	const float  lineLength       = 10.0f;                          // 線の長さ
	const float  gapSize          = 3.0f;                           // 中心の隙間
	const float  lineThickness    = 1.5f;                           // 線の太さ
	const float  outlineThickness = 0.5f;                           // 縁取りの太さ

	// 内側の線を描画（横線）
	drawList->AddLine(
		ImVec2(windowCenter.x - lineLength - gapSize, windowCenter.y),
		ImVec2(windowCenter.x - gapSize, windowCenter.y),
		ImGui::ColorConvertFloat4ToU32(reticleColor),
		lineThickness
	);
	drawList->AddLine(
		ImVec2(windowCenter.x + gapSize, windowCenter.y),
		ImVec2(windowCenter.x + lineLength + gapSize, windowCenter.y),
		ImGui::ColorConvertFloat4ToU32(reticleColor),
		lineThickness
	);

	// 内側の線を描画（縦線）
	drawList->AddLine(
		ImVec2(windowCenter.x, windowCenter.y - lineLength - gapSize),
		ImVec2(windowCenter.x, windowCenter.y - gapSize),
		ImGui::ColorConvertFloat4ToU32(reticleColor),
		lineThickness
	);
	drawList->AddLine(
		ImVec2(windowCenter.x, windowCenter.y + gapSize),
		ImVec2(windowCenter.x, windowCenter.y + lineLength + gapSize),
		ImGui::ColorConvertFloat4ToU32(reticleColor),
		lineThickness
	);

	// 縁取り（中心に小さな点）
	drawList->AddCircle(
		windowCenter,
		2.0f,
		ImGui::ColorConvertFloat4ToU32(outlineColor),
		0,
		outlineThickness + lineThickness
	);

	// 中心点
	drawList->AddCircleFilled(
		windowCenter,
		1.0f,
		ImGui::ColorConvertFloat4ToU32(reticleColor)
	);
#endif

	mWindEffect->Update(deltaTime);
	mExplosionEffect->Update(deltaTime);

	if (ConVarManager::GetConVar("r_clear")->GetValueAsBool()) {
		mCubeMap->Update(deltaTime);
	}

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
	mUPhysicsEngine->Update(deltaTime);

	for (auto entity : mEntities) {
		if (entity && !entity->GetParent()) {
			entity->PostPhysics(deltaTime);
		}
	}
}

void GameScene::Render() {
	if (ConVarManager::GetConVar("r_clear")->GetValueAsBool()) {
		mCubeMap->Render(
			mRenderer->GetCommandList()
		);
	}

	for (auto entity : mEntities) {
		if (entity) {
			entity->Render(mRenderer->GetCommandList());
		}
	}

	Unnamed::Engine::GetParticleManager()->Render();
	mParticleObject->Draw();
	mWindEffect->Draw();
	mExplosionEffect->Draw();
}

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
		//mPhysicsEngine->UnregisterEntity(mEntWorldMesh.get());
		mUPhysicsEngine->UnregisterEntity(mEntWorldMesh.get());
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
	const std::string meshPath      = "./resources/models/reflectionTest.obj";
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
	mWorldMeshRenderer = std::shared_ptr<StaticMeshRenderer>(
		smRenderer, [](StaticMeshRenderer*) {
		}
	);

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
	mUPhysicsEngine->RegisterEntity(mEntWorldMesh.get());

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
	//mPhysicsEngine->UnregisterEntity(mEntWorldMesh.get());
	mUPhysicsEngine->UnregisterEntity(mEntWorldMesh.get());
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
	const std::string meshPath      = "./resources/models/reflectionTest.obj";
	bool              reloadSuccess = mResourceManager->GetMeshManager()->
	                                       ReloadMeshFromFile(meshPath);

	if (!reloadSuccess) {
		Console::Print("Failed to reload mesh!", kConTextColorError);
		// 失敗した場合は元のコンポーネントを復元
		mEntWorldMesh->AddComponent<MeshColliderComponent>();
		//mPhysicsEngine->RegisterEntity(mEntWorldMesh.get(), true);
		mUPhysicsEngine->RegisterEntity(mEntWorldMesh.get());
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
	//mPhysicsEngine->RegisterEntity(mEntWorldMesh.get(), true);
	mUPhysicsEngine->RegisterEntity(mEntWorldMesh.get());
	Console::Print("Re-registered entity to physics engine",
	               kConTextColorCompleted);

	Console::Print("Safe world mesh reload completed!", kConTextColorCompleted);
}
