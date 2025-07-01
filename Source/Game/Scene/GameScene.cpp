#include "GameScene.h"

#include <Engine.h>
#include <Async/JobSystem.h>
#include <Camera/CameraManager.h>
#include <Components/CameraRotator.h>
#include <Debug/Debug.h>
#include <ImGuiManager/ImGuiManager.h>
#include <Input/InputSystem.h>
#include <Lib/DebugHud/DebugHud.h>
#include <Lib/Math/MathLib.h>
#include <Lib/Math/Random/Random.h>
#include <Lib/Timer/EngineTimer.h>
#include <Model/ModelManager.h>
#include <Object3D/Object3D.h>
#include <Particle/ParticleManager.h>
#include <Physics/Physics.h>
#include <Sprite/SpriteCommon.h>
#include <SubSystem/Console/ConVarManager.h>
#include <format>

#include "Assets/Manager/AssetManager.h"
#include "ImGuiManager/ImGuiWidgets.h"
#include "TextureManager/TexManager.h"

GameScene::~GameScene() {
	resourceManager_ = nullptr;
	spriteCommon_    = nullptr;
	particleManager_ = nullptr;
	object3DCommon_  = nullptr;
	modelCommon_     = nullptr;
	physicsEngine_   = nullptr;
	timer_           = nullptr;
}

void GameScene::Init() {
	{
		// size_t workerCount = (std::thread::hardware_concurrency() > 1)
		// 	                     ? std::thread::hardware_concurrency() - 1
		// 	                     : 1;
		// JobSystem jobSystem("AssetLoad", workerCount);
		//
		// AssetManager assetManager(jobSystem);
		//
		// std::shared_ptr<TextureAsset> texture;
		//
		// for (int i = 0; i < 10; ++i) {
		// 	auto textureFuture = jobSystem.SubmitJob(
		// 		0, [&assetManager, i]() -> std::shared_ptr<TextureAsset> {
		// 			return assetManager.LoadAsset<TextureAsset>(
		// 				"texture" + std::to_string(i));
		// 		}
		// 	);
		//
		// 	// なんかする
		// 	texture = textureFuture.get();
		// }
		//
		// Console::Print("Loaded asset : " + texture->GetID());
		//
		// assetManager.PrintCacheStatus();
	}

	renderer_        = Engine::GetRenderer();
	resourceManager_ = Engine::GetResourceManager();

#pragma region テクスチャ読み込み
	resourceManager_->GetTextureManager()->GetTexture(
		"./Resources/Textures/wave.dds");

	TexManager::GetInstance()->LoadTexture(
		"./Resources/Textures/smoke.png"
	);

#pragma endregion

#pragma region スプライト類
#pragma endregion

#pragma region 3Dオブジェクト類
	resourceManager_->GetMeshManager()->LoadMeshFromFile(
		"./Resources/Models/reflectionTest.obj");

	resourceManager_->GetMeshManager()->LoadMeshFromFile(
		"./Resources/Models/weapon.obj");

	cubeMap_ = std::make_unique<CubeMap>(renderer_->GetDevice());
#pragma endregion

#pragma region パーティクル類
	// パーティクルグループの作成
	Engine::GetParticleManager()->CreateParticleGroup(
		"wind", "./Resources/Textures/circle.png");

	mParticleEmitter = std::make_unique<ParticleEmitter>();
	mParticleEmitter->Init(Engine::GetParticleManager(), "wind");

	mParticleObject = std::make_unique<ParticleObject>();
	mParticleObject->Init(Engine::GetParticleManager(),
	                      "./Resources/Textures/circle.png");

#pragma endregion

#pragma region 物理エンジン
	physicsEngine_ = std::make_unique<PhysicsEngine>();
	physicsEngine_->Init();
#pragma endregion

#pragma region エンティティ
	camera_ = std::make_unique<Entity>("camera");
	AddEntity(camera_.get());
	// 生ポインタを取得
	CameraComponent* rawCameraPtr = camera_->AddComponent<CameraComponent>();
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
	PlayerMovement* rawPlayerMovement = mEntPlayer->AddComponent<
		PlayerMovement>();
	mPlayerMovement = std::shared_ptr<PlayerMovement>(
		rawPlayerMovement, [](PlayerMovement*) {
		}
	);
	mPlayerMovement->SetVelocity(Vec3::forward * 1.0f);
	mPlayerMovement->AddCameraShakeEntity(camera_.get());
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
		WeaponComponent>("./Resources/Json/rifle.json");
	mWeaponComponent = std::shared_ptr<WeaponComponent>(
		rawWeaponComponent, [](WeaponComponent*) {
		}
	);

	mEntWeapon->AddComponent<BoxColliderComponent>();

	mWeaponMeshRenderer->SetStaticMesh(
		resourceManager_->GetMeshManager()->GetStaticMesh(
			"./Resources/Models/weapon.obj"));
	WeaponSway* rawWeaponSway = mEntWeapon->AddComponent<WeaponSway>();
	mWeaponSway               = std::shared_ptr<WeaponSway>(
		rawWeaponSway, [](WeaponSway*) {
		}
	);
	AddEntity(mEntWeapon.get());

	mEntShakeRoot = std::make_unique<Entity>("shakeRoot");
	mPlayerMovement->AddCameraShakeEntity(mEntShakeRoot.get(), 3.0f);

	// テスト用メッシュ
	mEntWorldMesh                  = std::make_unique<Entity>("testMesh");
	StaticMeshRenderer* smRenderer = mEntWorldMesh->AddComponent<
		StaticMeshRenderer>();
	mWorldMeshRenderer = std::shared_ptr<StaticMeshRenderer>(
		smRenderer, [](StaticMeshRenderer*) {
		}
	);
	smRenderer->SetStaticMesh(
		resourceManager_->GetMeshManager()->GetStaticMesh(
			"./Resources/Models/reflectionTest.obj"));
	mEntWorldMesh->AddComponent<MeshColliderComponent>();
	AddEntity(mEntWorldMesh.get());

	// カメラの親エンティティ
	cameraRoot_ = std::make_unique<Entity>("cameraRoot");
	//cameraRoot_->SetParent(entPlayer_.get());
	cameraRoot_->GetTransform()->SetLocalPos(Vec3::up * 1.7f);
	cameraRotator_ = cameraRoot_->AddComponent<CameraRotator>();
	AddEntity(cameraRoot_.get());

	// cameraRootにアタッチ
	camera_->SetParent(cameraRoot_.get());
	camera_->GetTransform()->SetLocalPos(Vec3::zero); // FPS

	mEntShakeRoot->SetParent(camera_.get());

	mEntWeapon->SetParent(mEntShakeRoot.get());
	mEntShakeRoot->GetTransform()->SetLocalPos(Vec3(0.08f, -0.1f, 0.18f));
	mEntWeapon->GetTransform()->SetLocalPos(Vec3::zero);

#pragma endregion

	// 風
	windEffect_ = std::make_unique<WindEffect>();
	windEffect_->Init(Engine::GetParticleManager(), mPlayerMovement.get());

	// 爆発
	explosionEffect_ = std::make_unique<ExplosionEffect>();
	explosionEffect_->Init(Engine::GetParticleManager(),
	                       "./Resources/Textures/smoke.png");
	explosionEffect_->SetColorGradient(
		Vec4(0.78f, 0.29f, 0.05f, 1.0f), Vec4(0.04f, 0.04f, 0.05f, 1.0f));

#pragma region コンソール変数/コマンド
#pragma endregion

#pragma region メッシュレンダラー
#pragma endregion

	CameraManager::SetActiveCamera(camera);

	physicsEngine_->RegisterEntity(mEntWorldMesh.get(), true);

	// 物理エンジンにプレイヤーエンティティを登録
	physicsEngine_->RegisterEntity(mEntPlayer.get());

	physicsEngine_->RegisterEntity(mEntWeapon.get());

	Console::SubmitCommand(
		"sv_airaccelerate 100000000000000000"
	);

	// テレポートトリガー領域の設定
	Vec3 triggerCenter(19.5072f, -29.2608f, 260.096f); // トリガーの中心位置
	Vec3 triggerSize(Vec3::one * 13.0048f * 2.0f);     // トリガーのサイズ
	mTeleportTriggerMin = triggerCenter - triggerSize * 0.5f;
	mTeleportTriggerMax = triggerCenter + triggerSize * 0.5f;
}

void GameScene::Update(const float deltaTime) {
	physicsEngine_->Update(deltaTime);
	//
	cameraRoot_->GetTransform()->SetWorldPos(mPlayerMovement->GetHeadPos());
	// cameraRoot_->Update(EngineTimer::GetScaledDeltaTime());
	// camera_->Update(EngineTimer::GetScaledDeltaTime());

	if (InputSystem::IsPressed("+attack1")) {
		mWeaponComponent->PullTrigger();
	}
	if (InputSystem::IsReleased("+attack1")) {
		mWeaponComponent->ReleaseTrigger();
	}
	if (InputSystem::IsPressed("+reload")) {
		mEntPlayer->GetTransform()->SetWorldPos(Vec3::zero);
	}

	//mEntWeapon->Update(EngineTimer::GetScaledDeltaTime());
	//mWeaponComponent->Update(EngineTimer::GetScaledDeltaTime());
	if (mWeaponComponent->HasFiredThisFrame()) {
		Vec3 hitPos    = mWeaponComponent->GetHitPosition();
		Vec3 hitNormal = mWeaponComponent->GetHitNormal();

		// 爆発エフェクト
		explosionEffect_->TriggerExplosion(hitPos + hitNormal * 2.0f, hitNormal,
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
			mPlayerMovement->SetVelocity(
				mPlayerMovement->GetVelocity() + forceDir * force);
		}

		// カメラシェイク
		mPlayerMovement->StartCameraShake(
			0.1f, 0.1f, 5.0f, Vec3::forward, 0.025f, 3.0f,
			Vec3::right
		);
	}
	// mWeaponMeshRenderer->Update(EngineTimer::GetScaledDeltaTime());
	// mWeaponSway->Update(EngineTimer::GetScaledDeltaTime());
	//
	// mEntPlayer->Update(EngineTimer::GetScaledDeltaTime());
	// mEntShakeRoot->Update(EngineTimer::GetScaledDeltaTime());
	//
	// mEntWorldMesh->Update(deltaTime);

#ifdef _DEBUG
#pragma region cl_showpos
	if (int flag = ConVarManager::GetConVar("cl_showpos")->GetValueAsString() !=
		"0") {
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
		ImVec2 windowPos = ImVec2(0.0f, 128.0f + 16.0f);
		windowPos.x      = ImGui::GetMainViewport()->Pos.x + windowPos.x;
		windowPos.y      = ImGui::GetMainViewport()->Pos.y + windowPos.y;
		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);

		Mat4 invViewMat = Mat4::identity;
		if (CameraManager::GetActiveCamera()) {
			invViewMat = CameraManager::GetActiveCamera()->GetViewMat().
				Inverse();
		}

		Vec3       camPos = invViewMat.GetTranslate();
		const Vec3 camRot = invViewMat.ToQuaternion().ToEulerAngles();

		// テキストのサイズを取得
		//ImGuiIO io = ImGui::GetIO();
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
			Math::MtoH(mPlayerMovement->GetVelocity().Length())
		);

		//Console::Print(text);

		ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

		// ウィンドウサイズをテキストサイズに基づいて設定
		ImVec2 windowSize = ImVec2(textSize.x + 20.0f, textSize.y + 20.0f);
		// 余白を追加
		ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

		ImGui::Begin("cl_showpos", nullptr, windowFlags);

		ImVec2      textPos     = ImGui::GetCursorPos();
		ImDrawList* drawList    = ImGui::GetWindowDrawList();
		float       outlineSize = 1.0f;

		ImGuiManager::TextOutlined(
			drawList,
			textPos,
			text.c_str(),
			ToImVec4(kDebugHudTextColor),
			ToImVec4(kDebugHudOutlineColor),
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

	AABB teleportTriggerAABB(
		mTeleportTriggerMin, mTeleportTriggerMax);

	Debug::DrawBox(
		teleportTriggerAABB.GetCenter(),
		Quaternion::identity,
		teleportTriggerAABB.GetSize(),
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

	Engine::GetParticleManager()->Update(deltaTime);
	mParticleEmitter->Update(deltaTime);

	if (InputSystem::IsTriggered("attack1")) {
		//mParticleEmitter->Emit();
	}

	windEffect_->Update(EngineTimer::ScaledDelta());
	explosionEffect_->Update(EngineTimer::ScaledDelta());

	cubeMap_->Update(deltaTime);

#ifdef _DEBUG
	// レティクルの描画
	ImGuiIO& io           = ImGui::GetIO();
	ImVec2   windowCenter = ImVec2(io.DisplaySize.x * 0.5f,
	                             io.DisplaySize.y * 0.5f);

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


	for (auto entity : entities_) {
		if (entity) {
			if (!entity->GetParent()) {
				entity->Update(deltaTime);
			}
		}
	}
}

void GameScene::Render() {
	cubeMap_->Render(
		renderer_->GetCommandList(),
		resourceManager_->GetShaderResourceViewManager(),
		resourceManager_->GetTextureManager()->GetTexture(
			                "./Resources/Textures/wave.dds")
		                ->GetResource());

	for (auto entity : entities_) {
		if (entity) {
			entity->Render(renderer_->GetCommandList());
		}
	}

	Engine::GetParticleManager()->Render();
	mParticleObject->Draw();
	windEffect_->Draw();
	explosionEffect_->Draw();
}

void GameScene::Shutdown() {
	//cubeMap_.reset();
}
