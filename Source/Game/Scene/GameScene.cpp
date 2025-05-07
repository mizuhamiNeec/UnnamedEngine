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

void GameScene::Init() {
	size_t workerCount = (std::thread::hardware_concurrency() > 1) ? std::thread::hardware_concurrency() - 1 : 1;
	JobSystem jobSystem("AssetLoad", workerCount);

	AssetManager assetManager(jobSystem);

	std::shared_ptr<TextureAsset> texture;

	for (int i = 0; i < 10; ++i) {
		auto textureFuture = jobSystem.SubmitJob(
			0, [&assetManager, i]() -> std::shared_ptr<TextureAsset> {
				return assetManager.LoadAsset<TextureAsset>("texture" + std::to_string(i));
			}
		);

		// なんかする
		texture = textureFuture.get();
	}

	Console::Print("Loaded asset : " + texture->GetID());

	assetManager.PrintCacheStatus();

	renderer_ = Engine::GetRenderer();
	resourceManager_ = Engine::GetResourceManager();

#pragma region テクスチャ読み込み
#pragma endregion

#pragma region スプライト類
#pragma endregion

#pragma region 3Dオブジェクト類
	resourceManager_->GetMeshManager()->LoadMeshFromFile("./Resources/Models/reflectionTest.obj");
#pragma endregion

#pragma region パーティクル類
	// パーティクルグループの作成
	Engine::GetParticleManager()->CreateParticleGroup("wind", "./Resources/Textures/circle.png");

	mParticleEmitter = std::make_unique<ParticleEmitter>();
	mParticleEmitter->Init(Engine::GetParticleManager(), "wind");

	mParticleObject = std::make_unique<ParticleObject>();
	mParticleObject->Init(Engine::GetParticleManager(), "./Resources/Textures/circle.png");

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
	entPlayer_ = std::make_unique<Entity>("player");
	entPlayer_->GetTransform()->SetLocalPos(Vec3::up * 4.0f); // 1m上に配置
	PlayerMovement* rawPlayerMovement = entPlayer_->AddComponent<PlayerMovement>();
	playerMovement_ = std::shared_ptr<PlayerMovement>(
		rawPlayerMovement, [](PlayerMovement*) {
		}
	);
	BoxColliderComponent* rawPlayerCollider = entPlayer_->AddComponent<BoxColliderComponent>();
	playerCollider_ = std::shared_ptr<BoxColliderComponent>(
		rawPlayerCollider, [](BoxColliderComponent*) {
		}
	);
	playerCollider_->SetSize(Math::HtoM(Vec3(33.0f, 73.0f, 33.0f)));
	playerCollider_->SetOffset(Math::HtoM(Vec3::up * 73.0f * 0.5f));
	AddEntity(entPlayer_.get());

	// 風
	windEffect_ = std::make_unique<WindEffect>();
	windEffect_->Init(Engine::GetParticleManager(), playerMovement_.get());

	// テスト用メッシュ
	entTestMesh_ = std::make_unique<Entity>("testMesh");
	StaticMeshRenderer* smRenderer = entTestMesh_->AddComponent<StaticMeshRenderer>();
	smrTestMesh_ = std::shared_ptr<StaticMeshRenderer>(
		smRenderer, [](StaticMeshRenderer*) {
		}
	);
	smRenderer->SetStaticMesh(resourceManager_->GetMeshManager()->GetStaticMesh("./Resources/Models/reflectionTest.obj"));
	entTestMesh_->AddComponent<MeshColliderComponent>();
	AddEntity(entTestMesh_.get());

	// カメラの親エンティティ
	cameraRoot_ = std::make_unique<Entity>("cameraRoot");
	//cameraRoot_->SetParent(entPlayer_.get());
	cameraRoot_->GetTransform()->SetLocalPos(Vec3::up * 1.7f);
	cameraRotator_ = cameraRoot_->AddComponent<CameraRotator>();
	AddEntity(cameraRoot_.get());

	// cameraRootにアタッチ
	camera_->SetParent(cameraRoot_.get());
	camera_->GetTransform()->SetLocalPos(Vec3::zero); // FPS
#pragma endregion

#pragma region コンソール変数/コマンド
#pragma endregion

#pragma region メッシュレンダラー
#pragma endregion

	CameraManager::SetActiveCamera(camera);

	physicsEngine_->RegisterEntity(entTestMesh_.get(), true);

	// 物理エンジンにプレイヤーエンティティを登録
	physicsEngine_->RegisterEntity(entPlayer_.get());
}

void GameScene::Update(const float deltaTime) {
	static float controlPoints[4];

	ImGui::Begin("CubicBezier Visualization");
	static Vec3 test = Vec3::one;
	ImGuiWidgets::DragVec3("hello Vec4", test, 0.01f, "X %.2f m/s");
	ImGui::DragFloat3("Hello Vec4", &test.x, 0.01f, 0.0f, 1.0f);

	ImGuiWidgets::EditCubicBezier("mesh", controlPoints[0], controlPoints[1], controlPoints[2], controlPoints[3]);

	ImGui::End();

	for (const auto& triangle : smrTestMesh_->GetStaticMesh()->GetPolygons()) {
		if (triangle.GetCenter().Distance(camera_->GetTransform()->GetWorldPos()) < Math::HtoM(1024.0f)) {
			Triangle tri = triangle;
			for (int i = 0; i < 3; ++i) {
				float distance = triangle.GetCenter().Distance(camera_->GetTransform()->GetWorldPos());
				float progress = std::clamp((distance - Math::HtoM(512.0f)) / 10.0f, 0.0f, 1.0f);
				tri.SetVertex(
					i,
					Math::Lerp(
						tri.GetVertex(i),
						triangle.GetCenter() + triangle.GetNormal(),
						Math::CubicBezier(
							progress,
							controlPoints[0],
							controlPoints[1],
							controlPoints[2],
							controlPoints[3]
						)
					)
				);
			}
			Debug::DrawTriangle(tri, Vec4(0.0f, 1.0f, 1.0f, 1.0f));
		}
	}

	cameraRoot_->GetTransform()->SetWorldPos(playerMovement_->GetHeadPos());
	cameraRoot_->Update(EngineTimer::GetScaledDeltaTime());
	camera_->Update(EngineTimer::GetScaledDeltaTime());

	entPlayer_->Update(EngineTimer::GetScaledDeltaTime());
	entTestMesh_->Update(deltaTime);
	physicsEngine_->Update(deltaTime);

#ifdef _DEBUG
#pragma region cl_showpos
	if (int flag = ConVarManager::GetConVar("cl_showpos")->GetValueAsString() != "0") {
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
			Math::MtoH(playerMovement_->GetVelocity().Length())
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

	Engine::GetParticleManager()->Update(deltaTime);
	mParticleEmitter->Update(deltaTime);
	//	mParticleObject->Update(deltaTime);

	if (InputSystem::IsTriggered("attack1")) {
		mParticleEmitter->Emit();
	}

	windEffect_->Update(EngineTimer::ScaledDelta());
}

void GameScene::Render() {
	entTestMesh_->Render(renderer_->GetCommandList());

	Engine::GetParticleManager()->Render();
	mParticleObject->Draw();
	windEffect_->Draw();
}

void GameScene::Shutdown() {
}
