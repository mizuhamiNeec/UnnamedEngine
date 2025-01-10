#include "GameScene.h"

#include <Camera/CameraManager.h>
#include <Components/CameraRotator.h>
#include <Components/PlayerMovement.h>
#include <Debug/Debug.h>
#include <Engine.h>
#include <ImGuiManager/ImGuiManager.h>
#include <Lib/Console/ConVarManager.h>
#include <Lib/Math/MathLib.h>
#include <Lib/Math/Random/Random.h>
#include <Lib/Timer/EngineTimer.h>
#include <Model/ModelManager.h>
#include <Object3D/Object3D.h>
#include <Particle/ParticleManager.h>
#include <Sprite/SpriteCommon.h>
#include <TextureManager/TextureManager.h>
#include <Components/EnemyMovement.h>

#include "Input/InputSystem.h"

constexpr int enemyCount = 24;

void GameScene::Init(Engine* engine) {
	renderer_ = engine->GetRenderer();
	window_ = engine->GetWindow();
	spriteCommon_ = engine->GetSpriteCommon();
	particleManager_ = engine->GetParticleManager();
	object3DCommon_ = engine->GetObject3DCommon();
	modelCommon_ = engine->GetModelCommon();
	srvManager_ = engine->GetSrvManager();
	timer_ = engine->GetEngineTimer();

#pragma region テクスチャ読み込み
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/empty.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/circle.png");
#pragma endregion

#pragma region スプライト類
#pragma endregion

#pragma region 3Dオブジェクト類
	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("player.obj");
#pragma endregion

#pragma region パーティクル類
	particleManager_->CreateParticleGroup("doublestar", "./Resources/Textures/doublestar.png");

	particle_ = std::make_unique<ParticleObject>();
	particle_->Init(particleManager_, "./Resources/Textures/doublestar.png");
#pragma endregion

#pragma region エンティティ
	camera_ = std::make_unique<Entity>("camera");
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

	cameraRoot_ = std::make_unique<Entity>("cameraBase");
	cameraRotator_ = cameraRoot_->AddComponent<CameraRotator>();
	cameraRoot_->GetTransform()->SetLocalPos(Vec3::up * 1.7f);

	// プレイヤーにカメラをアタッチ
	camera_->SetParent(cameraRoot_.get());
	camera_->GetTransform()->SetLocalPos(Vec3::forward * -2.5f);

#pragma endregion

#pragma region コンソール変数/コマンド
#pragma endregion
}

void GameScene::Update(const float deltaTime) {
	cameraRoot_->Update(deltaTime);

	particleManager_->Update(deltaTime);
	particle_->Update(deltaTime);

#ifdef _DEBUG
#pragma region cl_showpos
	if (ConVarManager::GetConVar("cl_showpos")->GetValueAsString() == "1")
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
		constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking;
		ImVec2 windowPos = ImVec2(0.0f, 128.0f + 16.0f);
		windowPos.x = ImGui::GetMainViewport()->Pos.x + windowPos.x;
		windowPos.y = ImGui::GetMainViewport()->Pos.y + windowPos.y;
		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);

		Vec3 camPos = CameraManager::GetActiveCamera()->GetViewMat().Inverse().GetTranslate();
		Vec3 camRot = CameraManager::GetActiveCamera()->GetViewMat().Inverse().GetRotate();

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
		ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

		// ウィンドウサイズをテキストサイズに基づいて設定
		ImVec2 windowSize = ImVec2(textSize.x + 20.0f, textSize.y + 20.0f); // 余白を追加
		ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

		ImGui::Begin("##cl_showpos", nullptr, windowFlags);

		ImVec2 textPos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		float outlineSize = 1.0f;

		ImU32 textColor = IM_COL32(255, 255, 255, 255);
		ImU32 outlineColor = IM_COL32(0, 0, 0, 94);
		ImGuiManager::TextOutlined(drawList, textPos, text.c_str(), textColor, outlineColor, outlineSize);

		ImGui::PopStyleVar();
		ImGui::End();
	}
#pragma endregion
#endif
}

void GameScene::Render() {
	//----------------------------------------
	// オブジェクト3D共通描画設定
	object3DCommon_->Render();
	//----------------------------------------

	//----------------------------------------
	// パーティクル共通描画設定
	particleManager_->Render();
	//----------------------------------------
	particle_->Draw();

	//----------------------------------------
	// スプライト共通描画設定
	spriteCommon_->Render();
	//----------------------------------------
}

void GameScene::Shutdown() {
	spriteCommon_->Shutdown();
	object3DCommon_->Shutdown();
	particleManager_->Shutdown();
}
