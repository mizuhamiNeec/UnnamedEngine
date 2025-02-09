#include "Engine.h"

#ifdef _DEBUG
#include <imgui_internal.h>
#endif

#include <Camera/CameraManager.h>

#include <Debug/Debug.h>

#include <Input/InputSystem.h>

#include <SubSystem/Console/ConCommand.h>
#include <SubSystem/Console/ConVarManager.h>
#include <SubSystem/Console/Console.h>
#include <Lib/DebugHud/DebugHud.h>
#include <Lib/Utils/ClientProperties.h>
#include <Lib/Utils/StrUtils.h>

#include <Model/ModelManager.h>

#include <Object3D/Object3DCommon.h>

#include <Particle/ParticleManager.h>

#include <Renderer/D3D12.h>

#include <Scene/GameScene.h>

#include <Sprite/SpriteCommon.h>

#include <Window/Window.h>
#include <Window/WindowsUtils.h>

#include "Components/MeshRenderer/StaticMeshRenderer.h"

Engine::Engine() = default;

void Engine::Run() {
	Init();
	while (!bWishShutdown_) {
		if (Window::ProcessMessage()) {
			PostQuitMessage(0);
			break;
		}
		Update();
	}
	Shutdown();
}

void Engine::Init() {
	// ウィンドウの作成
	window_ = std::make_unique<Window>(
		StrUtils::ToString(kWindowTitle),
		kClientWidth, kClientHeight
	);
	window_->Create(nullptr);

	// レンダラ
	renderer_ = std::make_unique<D3D12>();
	renderer_->Init();

	// 入力システム
	InputSystem::Init();

	RegisterConsoleCommandsAndVariables();

	resourceManager_ = std::make_unique<ResourceManager>(renderer_.get());

#ifdef _DEBUG
	imGuiManager_ = std::make_unique<ImGuiManager>(renderer_.get(), resourceManager_->GetShaderResourceViewManager());
#endif

	console_ = std::make_unique<Console>();

	resourceManager_->Init();

	//// モデル
	//modelCommon_ = std::make_unique<ModelCommon>();
	//modelCommon_->Init(renderer_.get());

	//// オブジェクト3D
	//object3DCommon_ = std::make_unique<Object3DCommon>();
	//object3DCommon_->Init(renderer_.get());

	//// スプライト
	//spriteCommon_ = std::make_unique<SpriteCommon>();
	//spriteCommon_->Init(renderer_.get());

	//// パーティクル
	//particleManager_ = std::make_unique<ParticleManager>();
	//particleManager_->Init(object3DCommon_->GetD3D12(), srvManager_.get());

	// ライン
	lineCommon_ = std::make_unique<LineCommon>();
	lineCommon_->Init(renderer_.get());

	Debug::Init(lineCommon_.get());

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

	Console::SubmitCommand("neofetch");

	resourceManager_->GetTextureManager()->InitErrorTexture();

	// シーンマネージャ/ファクトリーの作成
	sceneFactory_ = std::make_unique<SceneFactory>();
	sceneManager_ = std::make_shared<SceneManager>(*sceneFactory_);
	// ゲームシーンを登録
	sceneFactory_->RegisterScene<GameScene>("GameScene");
	// シーンの初期化
	sceneManager_->ChangeScene("GameScene");

	// エディターの初期化
	CheckEditorMode();

	hr = renderer_->GetCommandList()->Close();
	assert(SUCCEEDED(hr));
}

void Engine::Update() {
#ifdef _DEBUG
	ImGuiManager::NewFrame();
	Console::Update();

	ImGui::Text("Hello World!!");
#endif

	time_->StartFrame();

	// 前のフレームとeditorModeが違う場合はエディターモードを切り替える
	static bool bPrevEditorMode = bIsEditorMode_;
	if (bPrevEditorMode != bIsEditorMode_) {
		CheckEditorMode();
		bPrevEditorMode = bIsEditorMode_;
	}

	/* ----------- 更新処理 ---------- */

	if (IsEditorMode()) {
		if (editor_) {
			editor_->Update(EngineTimer::GetDeltaTime());
		}
	} else {
		sceneManager_->Update(EngineTimer::GetScaledDeltaTime());
	}

#ifdef _DEBUG
	DebugHud::Update();
#endif

	InputSystem::Update();

	//-------------------------------------------------------------------------
	// --- PreRender↓ ---
	renderer_->PreRender();
	//-------------------------------------------------------------------------

#ifdef _DEBUG
	Debug::Update();
#endif
	CameraManager::Update(EngineTimer::GetDeltaTime());

	if (IsEditorMode()) {
		lineCommon_->Render();
		Debug::Draw();
		if (editor_) {
			editor_->Render();
		}
	} else {
		sceneManager_->Render();
	}

	//------------------------------------------------------------------------
	// --- PostRender↓ ---
#ifdef _DEBUG
	imGuiManager_->EndFrame();
#endif
	renderer_->PostRender();
	//-------------------------------------------------------------------------

	time_->EndFrame();
}

void Engine::Shutdown() const {

	resourceManager_->Shutdown();

	Debug::Shutdown();

	renderer_->Shutdown();

#ifdef _DEBUG
	// ImGuiManagerのシャットダウンは最後に行う
	if (imGuiManager_) {
		imGuiManager_->Shutdown();
	}
#endif
}

void Engine::RegisterConsoleCommandsAndVariables() {
	// コンソールコマンドを登録
	ConCommand::RegisterCommand("exit", Quit, "Exit the engine.");
	ConCommand::RegisterCommand("quit", Quit, "Exit the engine.");

	ConCommand::RegisterCommand(
		"toggleeditor",
		[]([[maybe_unused]] const std::vector<std::string>& args) {
			bIsEditorMode_ = !bIsEditorMode_;
			Console::Print("Editor mode is now " + std::to_string(bIsEditorMode_) + "\n", kConFgColorDark);
		},
		"Toggle editor mode."
	);

	// コンソール変数を登録
	ConVarManager::RegisterConVar<int>(
		"cl_showpos", 1, "Draw current position at top of screen (1 = meter, 2 = hammer)"
	);
	ConVarManager::RegisterConVar<int>("cl_showfps", 2, "Draw fps meter (1 = fps, 2 = smooth)");
	ConVarManager::RegisterConVar<int>("cl_fpsmax", kMaxFps, "Frame rate limiter");
	ConVarManager::RegisterConVar<std::string>("name", "unnamed", "Current user name", ConVarFlags::ConVarFlags_Notify);
	Console::SubmitCommand("name " + WindowsUtils::GetWindowsUserName());
	ConVarManager::RegisterConVar<float>("sensitivity", 2.0f, "Mouse sensitivity.");
	ConVarManager::RegisterConVar<float>("host_timescale", 1.0f, "Prescale the clock by this amount.");
	ConVarManager::RegisterConVar<float>("sv_gravity", 800.0f, "World gravity.");
	ConVarManager::RegisterConVar<float>(
		"sv_maxvelocity", 3500.0f, "Maximum speed any ballistically moving object is allowed to attain per axis."
	);
	ConVarManager::RegisterConVar<float>("sv_accelerate", 10.0f, "Linear acceleration amount (old value is 5.6)");
	ConVarManager::RegisterConVar<float>("sv_airaccelerate", 12.0f);
	ConVarManager::RegisterConVar<float>("sv_maxspeed", 320.0f, "Maximum speed a player can move.");
	ConVarManager::RegisterConVar<float>("sv_stopspeed", 100.0f, "Minimum stopping speed when on ground.");
	ConVarManager::RegisterConVar<float>("sv_friction", 4.0f, "World friction.");

	// デバッグ用にエンティティのaxisを表示するためのコンソール変数
	ConVarManager::RegisterConVar<int>("ent_axis", 0, "Show entity axis");

	// デフォルトのバインド
	Console::SubmitCommand("bind esc togglelockcursor");
	Console::SubmitCommand("bind w +forward");
	Console::SubmitCommand("bind s +back");
	Console::SubmitCommand("bind a +moveleft");
	Console::SubmitCommand("bind d +moveright");
	Console::SubmitCommand("bind e +moveup");
	Console::SubmitCommand("bind q +movedown");
	Console::SubmitCommand("bind space +jump");
	Console::SubmitCommand("bind mouse1 +attack1");
	Console::SubmitCommand("bind mouse2 +attack2");
	Console::SubmitCommand("bind mousewheelup +invprev");
	Console::SubmitCommand("bind mousewheeldown +invnext");
	Console::SubmitCommand("bind f1 toggleeditor");
}

void Engine::Quit([[maybe_unused]] const std::vector<std::string>& args) {
	bWishShutdown_ = true;
}

void Engine::CheckEditorMode() {
	if (bIsEditorMode_) {
		editor_ = std::make_unique<Editor>(*sceneManager_);
	} else {
		editor_.reset();
	}
}

bool Engine::bWishShutdown_ = false;
std::unique_ptr<D3D12> Engine::renderer_ = nullptr;
std::unique_ptr<ResourceManager> Engine::resourceManager_ = nullptr;

#ifdef _DEBUG
bool Engine::bIsEditorMode_ = true;
#else
bool Engine::bIsEditorMode_ = false;
#endif
