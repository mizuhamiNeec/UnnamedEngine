#include "GameScene.h"

#include "../Engine.h"
#include "../Engine/Lib/Console/ConVarManager.h"
#include "../Engine/Lib/Math/Quaternion/Quaternion.h"
#include "../Engine/Lib/Math/Random/Random.h"
#include "../Engine/Lib/Timer/EngineTimer.h"
#include "../Engine/Model/ModelManager.h"
#include "../ImGuiManager/ImGuiManager.h"
#include "../Lib/Math/MathLib.h"
#include "../Object3D/Object3D.h"
#include "../Particle/ParticleCommon.h"
#include "../Sprite/SpriteCommon.h"
#include "../TextureManager/TextureManager.h"

void GameScene::Init(
	D3D12* renderer, Window* window,
	SpriteCommon* spriteCommon, Object3DCommon* object3DCommon,
	ModelCommon* modelCommon, ParticleCommon* particleCommon,
	EngineTimer* engineTimer
) {
	renderer_ = renderer;
	window_ = window;
	spriteCommon_ = spriteCommon;
	object3DCommon_ = object3DCommon;
	modelCommon_ = modelCommon;
	particleCommon_ = particleCommon;
	timer_ = engineTimer;

#pragma region テクスチャ読み込み
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/empty.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/circle.png");
#pragma endregion

#pragma region スプライト類
	sprite_ = std::make_unique<Sprite>();
	sprite_->Init(spriteCommon_, "./Resources/Textures/uvChecker.png");
	sprite_->SetSize({512.0f, 512.0f, 0.0f});
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
	particle_ = std::make_unique<ParticleObject>();
	particle_->Init(particleCommon_, "./Resources/Textures/circle.png");
#pragma endregion
}

void GameScene::Update() {
	sprite_->Update();
	object3D_->Update();
	particle_->Update(EngineTimer::GetScaledDeltaTime());

#ifdef _DEBUG
	const std::vector<float> powerOfTwoValues = {
		0.125f, 0.25f, 0.5f, 1,
		2, 4, 8, 16,
		32, 64, 128, 256,
		512, 1024, 2048, 4096,
		8192, 16384
	};

	const std::vector<float> regularGridValues = {
		1, 5, 10, 50, 100, 500, 1000, 5000, 10000
	};

	static int selectedIndex = 3; // 最初の選択肢を選んでおく
	static int selectedGridType = 0; // 0: 2のべき乗グリッド, 1: 通常のグリッド

	static float gridSize = 1.0f; // 最初のグリッドサイズは1.0f

	ImGui::Begin("Game Scene");

	// グリッドの種類を選択
	const char* gridTypeNames[] = {"Power of 2", "Regular"};
	if (ImGui::Combo("Grid Type", &selectedGridType, gridTypeNames, IM_ARRAYSIZE(gridTypeNames))) {
		// グリッドタイプが変更された場合、最初の選択肢を設定
		if (selectedGridType == 0) {
			gridSize = powerOfTwoValues[selectedIndex]; // 2のべき乗グリッドサイズを設定
		} else {
			gridSize = regularGridValues[selectedIndex]; // 通常グリッドサイズを設定
		}
	}

	// グリッドサイズを変更
	if (selectedGridType == 0) {
		// 2のべき乗グリッド
		if (ImGui::BeginCombo("Grid Size", std::to_string(powerOfTwoValues[selectedIndex]).c_str())) {
			for (int i = 0; i < powerOfTwoValues.size(); ++i) {
				const bool isSelected = (selectedIndex == i);
				if (ImGui::Selectable(std::to_string(powerOfTwoValues[i]).c_str(), isSelected)) {
					selectedIndex = i;
					gridSize = powerOfTwoValues[selectedIndex];
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	} else {
		// 通常のグリッド
		if (ImGui::BeginCombo("Grid Size", std::to_string(regularGridValues[selectedIndex]).c_str())) {
			for (int i = 0; i < regularGridValues.size(); ++i) {
				const bool isSelected = (selectedIndex == i);
				if (ImGui::Selectable(std::to_string(regularGridValues[i]).c_str(), isSelected)) {
					selectedIndex = i;
					gridSize = regularGridValues[selectedIndex];
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}
	ImGui::End();

	// ライン描画
	{
		Engine::DrawGrid(100.0f, 4, 16, gridSize);

		static Vec3 pos;
		static Vec3 rot;

		ImGui::Begin("Axis");
		ImGui::DragFloat3("pos##axis", &pos.x, 0.01f);
		ImGui::DragFloat3("rot##axis", &rot.x, 0.01f);
		ImGui::End();

		Engine::DrawAxis(pos, Quaternion::Euler(rot));
	}

#endif

#ifdef _DEBUG
#pragma region cl_showpos
	if (ConVarManager::GetConVar("cl_showpos")->GetValueAsString() == "1") {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
		constexpr ImGuiWindowFlags windowFlags =
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking;
		ImVec2 windowPos = ImVec2(0.0f, 128.0f + 16.0f);
		windowPos.x = ImGui::GetMainViewport()->Pos.x + windowPos.x;
		windowPos.y = ImGui::GetMainViewport()->Pos.y + windowPos.y;
		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);

		// テキストのサイズを取得
		ImGuiIO io = ImGui::GetIO();
		std::string text = std::format(
			"name: {}\n"
			"pos : {:.2f} {:.2f} {:.2f}\n"
			"rot : {:.2f} {:.2f} {:.2f}\n"
			"vel : {:.2f}\n",
			ConVarManager::GetConVar("name")->GetValueAsString(),
			object3DCommon_->GetDefaultCamera()->GetPos().x, object3DCommon_->GetDefaultCamera()->GetPos().y,
			object3DCommon_->GetDefaultCamera()->GetPos().z,
			object3DCommon_->GetDefaultCamera()->GetRotate().x * Math::rad2Deg,
			object3DCommon_->GetDefaultCamera()->GetRotate().y * Math::rad2Deg,
			object3DCommon_->GetDefaultCamera()->GetRotate().z * Math::rad2Deg,
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
		TextOutlined(
			drawList,
			textPos,
			text.c_str(),
			textColor,
			outlineColor,
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
	object3DCommon_->Render();
	//----------------------------------------

	object3D_->Draw();

	//----------------------------------------
	// パーティクル共通描画設定
	particleCommon_->Render();
	//----------------------------------------
	//particle_->Draw();

	//----------------------------------------
	// スプライト共通描画設定
	spriteCommon_->Render();
	//----------------------------------------
	//sprite_->Draw();
}

void GameScene::Shutdown() {
	spriteCommon_->Shutdown();
	object3DCommon_->Shutdown();
	particleCommon_->Shutdown();
}
