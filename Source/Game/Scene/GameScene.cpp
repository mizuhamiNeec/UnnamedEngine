#include "GameScene.h"

#include <Camera/CameraManager.h>
#include <Components/CameraRotator.h>
#include <Engine.h>
#include <ImGuiManager/ImGuiManager.h>
#include <SubSystem/Console/ConVarManager.h>
#include <Lib/Math/MathLib.h>
#include <Lib/Math/Random/Random.h>
#include <Lib/Timer/EngineTimer.h>
#include <Model/ModelManager.h>
#include <Object3D/Object3D.h>
#include <Particle/ParticleManager.h>
#include <Sprite/SpriteCommon.h>
//#include <TextureManager/TextureManager.h>

#include "Debug/Debug.h"
#include "Input/InputSystem.h"
#include "Lib/DebugHud/DebugHud.h"
#include <Physics/Physics.h>

AABB GenerateRandomAABB(const Vec3& worldMin, const Vec3& worldMax, const Vec3& sizeRange) {
	Vec3 center = Random::Vec3Range(worldMin, worldMax);
	Vec3 halfSize = Random::Vec3Range(Vec3::zero, sizeRange);
	return { center - halfSize, center + halfSize };
}

std::vector<AABB> GenerateRandomAABBs(size_t count, const Vec3& worldMin, const Vec3& worldMax, const Vec3& sizeRange) {
	std::vector<AABB> aabbs;
	for (size_t i = 0; i < count; ++i) {
		aabbs.push_back(GenerateRandomAABB(worldMin, worldMax, sizeRange));
	}
	return aabbs;
}

void GameScene::Init() {
	renderer_ = Engine::GetRenderer();
	resourceManager_ = Engine::GetResourceManager();
	//spriteCommon_ = engine->GetSpriteCommon();
	//particleManager_ = engine->GetParticleManager();
	//object3DCommon_ = engine->GetObject3DCommon();
	//modelCommon_ = engine->GetModelCommon();
	//srvManager_ = engine->GetSrvManager();
	//timer_ = engine->GetEngineTimer();

#pragma region テクスチャ読み込み
	/*TextureManager::GetInstance()->LoadTexture("./Resources/Textures/empty.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/circle.png");*/
#pragma endregion

#pragma region スプライト類
	//sprite_ = std::make_unique<Sprite>();
	//sprite_->Init(spriteCommon_, "./Resources/Textures/uvChecker.png");
	//sprite_->SetAnchorPoint(Vec2::one * 0.5f);
	//sprite_->SetSize(Vec3(512.0f, 512.0f));
#pragma endregion

#pragma region 3Dオブジェクト類
#pragma endregion

#pragma region パーティクル類

#pragma endregion

#pragma region 物理エンジン
	physicsEngine_ = std::make_unique<PhysicsEngine>();
	physicsEngine_->Init();
#pragma endregion

#pragma region エンティティ
	camera_ = std::make_unique<Entity>("camera");
	entities_.push_back(camera_.get());
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

	entPlayer_ = std::make_unique<Entity>("player");
	entPlayer_->GetTransform()->SetLocalPos(Vec3::up * 1.0f); // 1m上に配置
	PlayerMovement* rawPlayerMovement = entPlayer_->AddComponent<PlayerMovement>();
	playerMovement_ = std::shared_ptr<PlayerMovement>(
		rawPlayerMovement, [](PlayerMovement*) {}
	);
	BoxColliderComponent* rawPlayerCollider = entPlayer_->AddComponent<BoxColliderComponent>();
	playerCollider_ = std::shared_ptr<BoxColliderComponent>(
		rawPlayerCollider, [](BoxColliderComponent*) {}
	);
	playerCollider_->SetSize(Math::HtoM(Vec3(33.0f, 73.0f, 33.0f)));
	entities_.push_back(entPlayer_.get());

	cameraRoot_ = std::make_unique<Entity>("cameraRoot");
	cameraRoot_->SetParent(entPlayer_.get());
	cameraRoot_->GetTransform()->SetLocalPos(Vec3::up * 1.7f);
	cameraRotator_ = cameraRoot_->AddComponent<CameraRotator>();
	entities_.push_back(cameraRoot_.get());

	// cameraRootにアタッチ
	camera_->SetParent(cameraRoot_.get());
	camera_->GetTransform()->SetLocalPos(Vec3::backward * 2.5f);
#pragma endregion

#pragma region コンソール変数/コマンド
	ConVarManager::RegisterConVar<Vec3>("testPos", Vec3::zero, "Test position");
#pragma endregion

#pragma region メッシュレンダラー
	resourceManager_->GetMeshManager()->LoadMeshFromFile("./Resources/Models/weaponNinjaSword.obj");
	auto mesh = resourceManager_->GetMeshManager()->GetStaticMesh("./Resources/Models/weaponNinjaSword.obj");
	testMeshEntity_ = std::make_unique<Entity>("testmesh");
	StaticMeshRenderer* rawTestMeshRenderer = testMeshEntity_->AddComponent<StaticMeshRenderer>();
	floatTestMR_ = std::shared_ptr<StaticMeshRenderer>(
		rawTestMeshRenderer, [](StaticMeshRenderer*) {}
	);
	floatTestMR_->SetStaticMesh(mesh);
	entities_.push_back(testMeshEntity_.get());

	//resourceManager_->GetMeshManager()->LoadMeshFromFile("./Resources/Models/ground.obj");
	//auto groundMesh = resourceManager_->GetMeshManager()->GetStaticMesh("./Resources/Models/ground.obj");
	//StaticMeshRenderer* groundMeshRenderer = entPlayer_->AddComponent<StaticMeshRenderer>();
	//groundMeshRenderer->SetStaticMesh(groundMesh);

	resourceManager_->GetMeshManager()->LoadMeshFromFile("./Resources/Models/lightWeight.obj");
	debugMesh = resourceManager_->GetMeshManager()->GetStaticMesh(
		"./Resources/Models/lightWeight.obj"
	);
	debugTestMeshEntity_ = std::make_unique<Entity>("debugTestMesh");
	StaticMeshRenderer* testMeshRenderer = debugTestMeshEntity_->AddComponent<StaticMeshRenderer>();
	debugTestMR_ = std::shared_ptr<StaticMeshRenderer>(
		testMeshRenderer, [](StaticMeshRenderer*) {}
	);
	debugTestMR_->SetStaticMesh(debugMesh);
	MeshColliderComponent* meshColliderComponent =
		debugTestMeshEntity_->AddComponent<MeshColliderComponent>();
	debugTestMeshCollider_ = std::shared_ptr<MeshColliderComponent>(
		meshColliderComponent,
		[](MeshColliderComponent*) {}
	);
	entities_.push_back(debugTestMeshEntity_.get());
#pragma endregion

	CameraManager::SetActiveCamera(camera);

	worldMesh_ = debugMesh->GetPolygons();

	//physicsEngine_->RegisterEntity(entPlayer_.get(), false);
	physicsEngine_->RegisterEntity(debugTestMeshEntity_.get(), true);
}

void GameScene::Update(const float deltaTime) {
	//if (InputSystem::IsPressed("+attack2")) {
	//	camera_->SetParent(nullptr);
	//	camera_->Update(deltaTime);
	//} else {
	//	camera_->SetParent(cameraRoot_.get());
	//}
	//// マウスホイールでカメラのローカルZ軸方向に移動
	//if (InputSystem::IsTriggered("+invprev")) {
	//	camera_->GetTransform()->SetLocalPos(camera_->GetTransform()->GetLocalPos() + Vec3::forward * 64.0f);
	//} else if (InputSystem::IsTriggered("+invnext")) {
	//	camera_->GetTransform()->SetLocalPos(camera_->GetTransform()->GetLocalPos() - Vec3::forward * 64.0f);
	//}
	//cameraRoot_->Update(deltaTime);

	//for (auto worldMesh : worldMesh_) {
	//	// メッシュのワイヤを描画
	//	Debug::DrawTriangle(worldMesh, Vec4::orange);
	//	Debug::DrawRay(worldMesh.GetCenter(), worldMesh.GetNormal(), Vec4::magenta);
	//}

	//physicsEngine_->Update(deltaTime);

	//プレイヤーの更新
	entPlayer_->Update(deltaTime);

	testMeshEntity_->Update(deltaTime);
	debugTestMeshEntity_->Update(deltaTime);

	/*particleManager_->Update(deltaTime);
	static bool isOffscreen = false;
	static float rotation = 0.0f;
	Vec3 pos = Math::WorldToScreen(
		ConVarManager::GetConVar("testPos")->GetValueAsVec3(),
		true,
		32.0f,
		isOffscreen,
		rotation
	);*/

	//sprite_->SetPos(Vec3(pos.x, pos.y));
	//sprite_->SetRot(Vec3::forward * rotation);
	//sprite_->Update();

#ifdef _DEBUG
#pragma region cl_showpos
	if (int flag = ConVarManager::GetConVar("cl_showpos")->GetValueAsString() != "0") {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
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
		windowPos.x = ImGui::GetMainViewport()->Pos.x + windowPos.x;
		windowPos.y = ImGui::GetMainViewport()->Pos.y + windowPos.y;
		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);

		Mat4 invViewMat = Mat4::identity;
		if (CameraManager::GetActiveCamera()) {
			invViewMat = CameraManager::GetActiveCamera()->GetViewMat().Inverse();
		}

		Vec3 camPos = invViewMat.GetTranslate();
		const Vec3 camRot = invViewMat.ToQuaternion().ToEulerAngles();

		// テキストのサイズを取得
		ImGuiIO io = ImGui::GetIO();
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
			0.0f
		);

		//Console::Print(text);

		ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

		// ウィンドウサイズをテキストサイズに基づいて設定
		ImVec2 windowSize = ImVec2(textSize.x + 20.0f, textSize.y + 20.0f); // 余白を追加
		ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

		ImGui::Begin("##cl_showpos", nullptr, windowFlags);

		ImVec2 textPos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		float outlineSize = 1.0f;

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
}

void GameScene::Render() {
	//----------------------------------------
	// オブジェクト3D共通描画設定
	//object3DCommon_->Render();
	//----------------------------------------

	//----------------------------------------
	// パーティクル共通描画設定
	//particleManager_->Render();
	//----------------------------------------

	//----------------------------------------
	// スプライト共通描画設定
	//spriteCommon_->Render();
	//----------------------------------------
	//sprite_->Draw();

	debugTestMeshEntity_->Render(renderer_->GetCommandList());
	testMeshEntity_->Render(renderer_->GetCommandList());
}

void GameScene::Shutdown() {
	//spriteCommon_->Shutdown();
	//object3DCommon_->Shutdown();
	//particleManager_->Shutdown();
}
