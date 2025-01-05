#include "GameScene.h"

#include "Lib/Console/ConVarManager.h"
#include "Lib/Math/Random/Random.h"
#include "Lib/Timer/EngineTimer.h"
#include "Model/ModelManager.h"
#include "Camera/Camera.h"
#include "Debug/Debug.h"
#include "Engine.h"
#include "ImGuiManager/ImGuiManager.h"
#include "Lib/Math/MathLib.h"
#include "Object3D/Object3D.h"
#include "Particle/ParticleManager.h"
#include "Sprite/SpriteCommon.h"
#include "TextureManager/TextureManager.h"

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
	sprite_ = std::make_unique<Sprite>();
	sprite_->Init(spriteCommon_, "./Resources/Textures/uvChecker.png");
	sprite_->SetSize({ 512.0f, 512.0f, 0.0f });
#pragma endregion

#pragma region 3Dオブジェクト類
	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("reflectionTest.obj");

	object3D_ = std::make_unique<Object3D>();
	object3D_->Init(object3DCommon_);
	// 初期化済みの3Dオブジェクトにモデルを紐づける
	object3D_->SetModel("reflectionTest.obj");
	object3D_->SetPos(Vec3::zero);
#pragma endregion

#pragma region パーティクル類
	particleManager_->CreateParticleGroup("circle", "./Resources/Textures/circle.png");

	particle_ = std::make_unique<ParticleObject>();
	particle_->Init(particleManager_, "./Resources/Textures/circle.png");
#pragma endregion

#pragma region エンティティ
	testEnt_ = std::make_unique<Entity>("testEnt");
	testEnt2_ = std::make_unique<Entity>("testEnt2");

	testEnt_->GetTransform()->SetLocalPos(Vec3(0.0f, 1.0f, 0.0f));
	testEnt_->GetTransform()->SetLocalRot(Quaternion::Euler(Vec3(0.0f, 45.0f * 3.14159f / 180.0f, 0.0f)));
	testEnt2_->GetTransform()->SetLocalPos(Vec3(1.0f, 0.0f, 0.0f));

	testEnt2_->SetParent(testEnt_.get());
#pragma endregion
}

void GameScene::Update(const float deltaTime) {
	sprite_->Update();
	object3D_->Update();

   // particleManager_->Update(EngineTimer::GetScaledDeltaTime());
   // particle_->Update(EngineTimer::GetScaledDeltaTime());

	testEnt_->Update(deltaTime);

#ifdef _DEBUG
#pragma region cl_showpos
	if (ConVarManager::GetConVar("cl_showpos")->GetValueAsString() == "1") {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
		constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking;
		ImVec2 windowPos = ImVec2(0.0f, 128.0f + 16.0f);
		windowPos.x = ImGui::GetMainViewport()->Pos.x + windowPos.x;
		windowPos.y = ImGui::GetMainViewport()->Pos.y + windowPos.y;
		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);

		Camera* cam = object3DCommon_->GetDefaultCamera();

		// テキストのサイズを取得
		ImGuiIO io = ImGui::GetIO();
		std::string text = std::format(
			"name: {}\n"
			"pos : {:.2f} {:.2f} {:.2f}\n"
			"rot : {:.2f} {:.2f} {:.2f}\n"
			"vel : {:.2f}\n",
			ConVarManager::GetConVar("name")->GetValueAsString(),
			cam->GetPos().x, cam->GetPos().y, cam->GetPos().z,
			cam->GetRotate().x * Math::rad2Deg,
			cam->GetRotate().y * Math::rad2Deg,
			cam->GetRotate().z * Math::rad2Deg,
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
	object3D_->Draw();

	//----------------------------------------
	// パーティクル共通描画設定
	particleManager_->Render();
	//----------------------------------------
	// particle_->Draw();

	//----------------------------------------
	// スプライト共通描画設定
	spriteCommon_->Render();
	//----------------------------------------
	// sprite_->Draw();
}

void GameScene::Shutdown() {
	spriteCommon_->Shutdown();
	object3DCommon_->Shutdown();
	particleManager_->Shutdown();
}
