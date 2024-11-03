#include "GameScene.h"

#include <format>

#ifdef _DEBUG
#include "imgui/imgui.h"
#endif

#include "WindowsUtils.h"

#include "../Engine/Model/ModelManager.h"

#include "../ImGuiManager/ImGuiManager.h"

#include "../Lib/Console/ConVar.h"
#include "../Lib/Console/ConVars.h"
#include "../Lib/Math/MathLib.h"

#include "../Object3D/Object3D.h"

#include "../Sprite/SpriteCommon.h"

#include "../TextureManager/TextureManager.h"

#ifdef _DEBUG
#endif

void GameScene::Init(D3D12* renderer, Window* window, SpriteCommon* spriteCommon, Object3DCommon* object3DCommon,
                     ModelCommon* modelCommon) {
	renderer_ = renderer;
	window_ = window;
	spriteCommon_ = spriteCommon;
	object3DCommon_ = object3DCommon;
	modelCommon_ = modelCommon;

#pragma region テクスチャ読み込み
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/empty.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/uvChecker.png");
#pragma endregion

#pragma region スプライト類
#pragma endregion

#pragma region 3Dオブジェクト類
	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("axis.obj");

	object3D_ = std::make_unique<Object3D>();
	object3D_->Init(object3DCommon_, modelCommon_);
	// 初期化済みの3Dオブジェクトにモデルを紐づける
	object3D_->SetModel("axis.obj");
	object3D_->SetPos({1.0f, -0.3f, 0.6f});
#pragma endregion

#pragma region パーティクル類

#pragma endregion
}

void GameScene::Update() {
	object3D_->Update();
#ifdef _DEBUG
#pragma region cl_showpos
	if (ConVars::GetInstance().GetConVar("cl_showpos")->GetInt() == 1) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
		const ImGuiWindowFlags windowFlags =
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings;
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
			WindowsUtils::GetWindowsUserName(),
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
	// オブジェクト3Dの描画設定
	object3DCommon_->Render();
	//----------------------------------------

	object3D_->Draw();

	//----------------------------------------
	// スプライト共通描画設定
	spriteCommon_->Render();
	//----------------------------------------
}

void GameScene::Shutdown() {
	spriteCommon_->Shutdown();
	object3DCommon_->Shutdown();
}
