#include "GameScene.h"

#include <Engine.h>
#include <Camera/CameraManager.h>
#include <Components/CameraRotator.h>
#include <ImGuiManager/ImGuiManager.h>
#include <Lib/Console/ConVarManager.h>
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

void GameScene::Init(Engine* engine) {
	renderer_ = engine->GetRenderer();
	window_ = engine->GetWindow();
	spriteCommon_ = engine->GetSpriteCommon();
	particleManager_ = engine->GetParticleManager();
	object3DCommon_ = engine->GetObject3DCommon();
	modelCommon_ = engine->GetModelCommon();
	//srvManager_ = engine->GetSrvManager();
	timer_ = engine->GetEngineTimer();

#pragma region テクスチャ読み込み
	/*TextureManager::GetInstance()->LoadTexture("./Resources/Textures/empty.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/circle.png");*/
#pragma endregion

#pragma region スプライト類
	sprite_ = std::make_unique<Sprite>();
	sprite_->Init(spriteCommon_, "./Resources/Textures/uvChecker.png");
	sprite_->SetAnchorPoint(Vec2::one * 0.5f);
	sprite_->SetSize(Vec3(512.0f, 512.0f));
#pragma endregion

#pragma region 3Dオブジェクト類
#pragma endregion

#pragma region パーティクル類

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
	cameraRoot_->GetTransform()->SetLocalPos(Vec3::up * 1.7f);
	cameraRotator_ = camera_->AddComponent<CameraRotator>();

	// プレイヤーにカメラをアタッチ
	//camera_->SetParent(cameraRoot_.get());
	camera_->GetTransform()->SetLocalPos(Vec3::up * 2.0f + Vec3::forward * -2.5f);
#pragma endregion

#pragma region コンソール変数/コマンド
	ConVarManager::RegisterConVar<Vec3>("testPos", Vec3::zero, "Test position");
#pragma endregion
}

void GameScene::Update(const float deltaTime) {
	camera_->Update(deltaTime);

	// マウスホイールでカメラのローカルZ軸方向に移動
	if (InputSystem::IsTriggered("+invprev")) {
		camera_->GetTransform()->SetLocalPos(camera_->GetTransform()->GetLocalPos() + Vec3::forward * 0.1f);
	} else if (InputSystem::IsTriggered("+invnext")) {
		camera_->GetTransform()->SetLocalPos(camera_->GetTransform()->GetLocalPos() - Vec3::forward * 0.1f);
	}

	particleManager_->Update(deltaTime);
	static bool isOffscreen = false;
	static float rotation = 0.0f;
	Vec3 pos = Math::WorldToScreen(
		ConVarManager::GetConVar("testPos")->GetValueAsVec3(),
		true,
		32.0f,
		isOffscreen,
		rotation
	);

	sprite_->SetPos(Vec3(pos.x, pos.y));
	sprite_->SetRot(Vec3::forward * rotation);
	sprite_->Update();

	Debug::DrawAxisWithCharacter(ConVarManager::GetConVar("testPos")->GetValueAsVec3(), Quaternion::Euler(0.0f, 0.0f, 0.0f));

#ifdef _DEBUG
#pragma region cl_showpos
	if (int flag = ConVarManager::GetConVar("cl_showpos")->GetValueAsString() != "0") {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
		constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking;
		ImVec2 windowPos = ImVec2(0.0f, 128.0f + 16.0f);
		windowPos.x = ImGui::GetMainViewport()->Pos.x + windowPos.x;
		windowPos.y = ImGui::GetMainViewport()->Pos.y + windowPos.y;
		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);

		Mat4 invViewMat = CameraManager::GetActiveCamera()->GetViewMat().Inverse();
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
	object3DCommon_->Render();
	//----------------------------------------

	//----------------------------------------
	// パーティクル共通描画設定
	particleManager_->Render();
	//----------------------------------------

	//----------------------------------------
	// スプライト共通描画設定
	spriteCommon_->Render();
	//----------------------------------------
	sprite_->Draw();
}

void GameScene::Shutdown() {
	spriteCommon_->Shutdown();
	object3DCommon_->Shutdown();
	particleManager_->Shutdown();
}
