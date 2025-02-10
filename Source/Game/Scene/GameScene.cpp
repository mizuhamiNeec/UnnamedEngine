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

void GameScene::Init() {

	size_t workerCount = (std::thread::hardware_concurrency() > 1) ? std::thread::hardware_concurrency() - 1 : 1;
	JobSystem jobSystem("AssetLoad",workerCount);

	AssetManager assetManager(jobSystem);

	std::shared_ptr<TextureAsset> texture;

	for(int i = 0; i < 10; ++i) {
		auto textureFuture = jobSystem.SubmitJob(0,[&assetManager,i]() -> std::shared_ptr<TextureAsset> {
			return assetManager.LoadAsset<TextureAsset>("texture" + std::to_string(i));
		});

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
	#pragma endregion

	#pragma region パーティクル類

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
		rawCameraPtr,[](CameraComponent*) {
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
		rawPlayerMovement,[](PlayerMovement*) {}
	);
	BoxColliderComponent* rawPlayerCollider = entPlayer_->AddComponent<BoxColliderComponent>();
	playerCollider_ = std::shared_ptr<BoxColliderComponent>(
		rawPlayerCollider,[](BoxColliderComponent*) {}
	);
	playerCollider_->SetSize(Math::HtoM(Vec3(33.0f,73.0f,33.0f)));
	playerCollider_->SetOffset(Math::HtoM(Vec3::up * 73.0f * 0.5f));
	AddEntity(entPlayer_.get());

	cameraRoot_ = std::make_unique<Entity>("cameraRoot");
	cameraRoot_->SetParent(entPlayer_.get());
	cameraRoot_->GetTransform()->SetLocalPos(Vec3::up * 1.7f);
	cameraRotator_ = cameraRoot_->AddComponent<CameraRotator>();
	AddEntity(cameraRoot_.get());

	// cameraRootにアタッチ
	camera_->SetParent(cameraRoot_.get());
	camera_->GetTransform()->SetLocalPos(Vec3::backward * 2.5f);
	#pragma endregion

	#pragma region コンソール変数/コマンド
	#pragma endregion

	#pragma region メッシュレンダラー

	#pragma endregion

	CameraManager::SetActiveCamera(camera);

	// 物理エンジンにプレイヤーエンティティを登録
	physicsEngine_->RegisterEntity(entPlayer_.get(),false);
}

void GameScene::Update(const float deltaTime) {
	entPlayer_->Update(EngineTimer::GetScaledDeltaTime());
	physicsEngine_->Update(deltaTime);

	#ifdef _DEBUG
	#pragma region cl_showpos
	if(int flag = ConVarManager::GetConVar("cl_showpos")->GetValueAsString() != "0") {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0.0f,0.0f});
		constexpr ImGuiWindowFlags windowFlags =
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav;
		ImVec2 windowPos = ImVec2(0.0f,128.0f + 16.0f);
		windowPos.x = ImGui::GetMainViewport()->Pos.x + windowPos.x;
		windowPos.y = ImGui::GetMainViewport()->Pos.y + windowPos.y;
		ImGui::SetNextWindowPos(windowPos,ImGuiCond_Always);

		Mat4 invViewMat = Mat4::identity;
		if(CameraManager::GetActiveCamera()) {
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
			camPos.x,camPos.y,camPos.z,
			camRot.x * Math::rad2Deg,
			camRot.y * Math::rad2Deg,
			camRot.z * Math::rad2Deg,
			Math::MtoH(playerMovement_->GetVelocity().Length())
		);

		//Console::Print(text);

		ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

		// ウィンドウサイズをテキストサイズに基づいて設定
		ImVec2 windowSize = ImVec2(textSize.x + 20.0f,textSize.y + 20.0f); // 余白を追加
		ImGui::SetNextWindowSize(windowSize,ImGuiCond_Always);

		ImGui::Begin("##cl_showpos",nullptr,windowFlags);

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

void GameScene::Render() {}

void GameScene::Shutdown() {}
