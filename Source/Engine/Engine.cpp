#include "Engine.h"

#include <thread>

#include "../Input/Input.h"

#include "../ImGuiManager/ImGuiManager.h"

#include "Camera/Camera.h"

#include "Lib/Console/Console.h"
#include "Lib/Console/ConVar.h"
#include "Lib/Console/ConVars.h"
#include "Lib/Utils/ClientProperties.h"

#include "Model/ModelManager.h"

#include "Object3D/Object3DCommon.h"

#include "Particle/ParticleCommon.h"

#include "Renderer/D3D12.h"
#include "Renderer/SrvManager.h"

#include "TextureManager/TextureManager.h"

#include "Window/Window.h"

Engine::Engine() = default;

void Engine::Run() {
	Init();
	while (true) {
		if (window_->ProcessMessage()) break; // ゲームループを抜ける
		Update();
	}
	Shutdown();
}

void Engine::Init() {
	// ウィンドウの作成
	window_ = std::make_unique<Window>(L"Window", kClientWidth, kClientHeight);
	// ウィンドウの作成を試みる
	if (!window_->Create(nullptr)) {
		assert(false && "ウィンドウの作成に失敗しました。");
	}

	// レンダラ
	renderer_ = std::make_unique<D3D12>();
	renderer_->Init(window_.get());

	// SRVマネージャの初期化
	srvManager_ = std::make_unique<SrvManager>();
	srvManager_->Init(renderer_.get());

#ifdef _DEBUG
	imGuiManager_ = std::make_unique<ImGuiManager>();
	imGuiManager_->Init(renderer_.get(), window_.get(), srvManager_.get());

	console_ = std::make_unique<Console>();
#endif

	// テクスチャマネージャ
	TextureManager::GetInstance()->Init(renderer_.get(), srvManager_.get());

	// 3Dモデルマネージャ
	ModelManager::GetInstance()->Init(renderer_.get());

	// カメラの作成
	camera_ = std::make_unique<Camera>();
	camera_->SetPos({ 0.0f, 0.0f, -10.0f });

	// モデル
	modelCommon_ = std::make_unique<ModelCommon>();
	modelCommon_->Init(renderer_.get());

	// オブジェクト3D
	object3DCommon_ = std::make_unique<Object3DCommon>();
	object3DCommon_->Init(renderer_.get());
	object3DCommon_->SetDefaultCamera(camera_.get());

	// スプライト
	spriteCommon_ = std::make_unique<SpriteCommon>();
	spriteCommon_->Init(renderer_.get());

	// パーティクル
	particleCommon_ = std::make_unique<ParticleCommon>();
	particleCommon_->Init(renderer_.get(), srvManager_.get());
	particleCommon_->SetDefaultCamera(camera_.get());

	// 入力
	Input::GetInstance()->Init(window_.get());

	//-------------------------------------------------------------------------
	// コマンドのリセット
	//-------------------------------------------------------------------------
	HRESULT hr = renderer_->GetCommandAllocator()->Reset();
	assert(SUCCEEDED(hr));
	hr = renderer_->GetCommandList()->Reset(
		renderer_->GetCommandAllocator(),
		nullptr
	);
	assert(SUCCEEDED(hr));

	time_ = std::make_unique<EngineTimer>();

	// シーン
	gameScene_ = std::make_unique<GameScene>();
	gameScene_->Init(
		renderer_.get(),
		window_.get(),
		spriteCommon_.get(),
		object3DCommon_.get(),
		modelCommon_.get(),
		particleCommon_.get(),
		time_.get()
	);

	hr = renderer_->GetCommandList()->Close();
	assert(SUCCEEDED(hr));
}

void Engine::Update() {
	while (true) {
		/* ----------- 更新処理 ---------- */

		Input::GetInstance()->Update();

		// コンソール表示切り替え
		if (Input::GetInstance()->TriggerKey(DIK_GRAVE)) {
			Console::ToggleConsole();
		}
    
#ifdef _DEBUG
	imGuiManager_->NewFrame();
	console_->Update();
#endif

	time_->Update();

	/* ----------- 更新処理 ---------- */
	Input::GetInstance()->Update();

	// コンソール表示切り替え
	if (Input::GetInstance()->TriggerKey(DIK_GRAVE)) {
		Console::ToggleConsole();
	}

	camera_->SetAspectRatio(
		static_cast<float>(window_->GetClientWidth()) / static_cast<float>(window_->GetClientHeight())
	);
	camera_->Update();

	// ゲームシーンの更新
	gameScene_->Update();

#ifdef _DEBUG // cl_showfps
	if (ConVars::GetInstance().GetConVar("cl_showfps")->GetInt() == 1) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

		ImGuiWindowFlags windowFlags =
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoBringToFrontOnFocus
			;

		ImVec2 windowPos = ImVec2(0.0f, 128.0f);

		windowPos.x = ImGui::GetMainViewport()->Pos.x + windowPos.x;
		windowPos.y = ImGui::GetMainViewport()->Pos.y + windowPos.y;

		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
		// テキストのサイズを取得
		ImGuiIO io = ImGui::GetIO();
		std::string text = std::format("{:.2f} fps", io.Framerate);
		ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

		// ウィンドウサイズをテキストサイズに基づいて設定
		ImVec2 windowSize = ImVec2(textSize.x + 20.0f, textSize.y + 20.0f); // 余白を追加
		ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

		ImGui::Begin("##cl_showfps", nullptr, windowFlags);

		ImVec2 textPos = ImGui::GetCursorScreenPos();

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		float outlineSize = 1.0f;

		ImU32 textColor = ImGui::ColorConvertFloat4ToU32(kConsoleColorError);
		if (io.Framerate >= 59.9f) {
			textColor = ImGui::ColorConvertFloat4ToU32(kConsoleColorFloat);
		} else if (io.Framerate >= 29.9f) {
			textColor = ImGui::ColorConvertFloat4ToU32(kConsoleColorWarning);
		}

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
#endif

	/* ---------- Pre ----------- */
	renderer_->PreRender();
	srvManager_->PreDraw();
	/* ---------- コマンド積み ----------- */

	gameScene_->Render();

#ifdef _DEBUG
	imGuiManager_->EndFrame();
#endif

	/* ---------- Post ----------- */
	renderer_->PostRender();
	/* ---------- ゲームループ終了 ---------- */
}

void Engine::Shutdown() const {
	gameScene_->Shutdown();

	ModelManager::Shutdown();
	TextureManager::Shutdown();

#ifdef _DEBUG
	// ImGuiManagerのシャットダウンは最後に行う
	if (imGuiManager_) {
		imGuiManager_->Shutdown();
	}
#endif
}
