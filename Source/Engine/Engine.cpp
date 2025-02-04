#include "Engine.h"

#ifdef _DEBUG
#include "imgui/imgui_internal.h"
#endif

#include <Camera/Camera.h>
#include <Camera/CameraManager.h>
#include <Debug/Debug.h>
#include <Input/InputSystem.h>
#include <Lib/Console/ConCommand.h>
#include <Lib/Console/Console.h>
#include <Lib/Console/ConVarManager.h>
#include <Lib/DebugHud/DebugHud.h>
#include <Lib/Utils/ClientProperties.h>
#include <Model/ModelManager.h>
#include <Object3D/Object3DCommon.h>
#include <Particle/ParticleManager.h>
#include <Renderer/D3D12.h>
#include <Renderer/SrvManager.h>
#include <Scene/GameScene.h>
#include <Sprite/SpriteCommon.h>
#include <TextureManager/TextureManager.h>
#include <Window/Window.h>
#include <Window/WindowsUtils.h>


Engine::Engine() = default;

void Engine::Run() {
	Init();
	while (true) {
		if (Window::ProcessMessage() || bWishShutdown_)
			break; // ゲームループを抜ける
		Update();
	}
	Shutdown();
}


void Engine::ChangeScene(const std::shared_ptr<Scene>& newScene) {
	currentScene_ = std::move(newScene);
	currentScene_->Init(this);
}

void Engine::Init() {
	RegisterConsoleCommandsAndVariables();

	// ウィンドウの作成
	window_ = std::make_unique<Window>(L"LE2A_18_ミズサワ_ハミル", kClientWidth, kClientHeight);
	window_->Create(nullptr);

	InputSystem::Init();

	// レンダラ
	renderer_ = std::make_unique<D3D12>();
	renderer_->Init();

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
	cameraEntity_ = std::make_unique<Entity>("editorcamera");
	cameraEntity_->GetTransform()->SetLocalPos(Vec3::forward * -5.0f + Vec3::up * 2.0f);
	cameraEntity_->GetTransform()->SetLocalRot(Quaternion::Euler(Vec3::right * 15.0f * Math::deg2Rad));
	rot_ = cameraEntity_->GetTransform()->GetLocalRot().ToEulerAngles();

	// 生ポインタを取得
	CameraComponent* rawCameraPtr = cameraEntity_->AddComponent<CameraComponent>();

	// 生ポインタを std::shared_ptr に変換
	std::shared_ptr<CameraComponent> camera = std::shared_ptr<CameraComponent>(rawCameraPtr, [](CameraComponent*) {});

	// カメラを CameraManager に追加
	CameraManager::AddCamera(camera);
	// アクティブカメラに設定
	CameraManager::SetActiveCamera(camera);

	// モデル
	modelCommon_ = std::make_unique<ModelCommon>();
	modelCommon_->Init(renderer_.get());

	// オブジェクト3D
	object3DCommon_ = std::make_unique<Object3DCommon>();
	object3DCommon_->Init(renderer_.get());

	// スプライト
	spriteCommon_ = std::make_unique<SpriteCommon>();
	spriteCommon_->Init(renderer_.get());

	// パーティクル
	particleManager_ = std::make_unique<ParticleManager>();
	particleManager_->Init(object3DCommon_->GetD3D12(), srvManager_.get());

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

	std::shared_ptr<Scene> gameScene = std::make_shared<GameScene>();
	ChangeScene(gameScene);
	CheckEditorMode();

	hr = renderer_->GetCommandList()->Close();
	assert(SUCCEEDED(hr));
}

void Engine::Update() {
#ifdef _DEBUG
	ImGuiManager::NewFrame();
	Console::Update();
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
		if (currentScene_) {
			currentScene_->Update(EngineTimer::GetScaledDeltaTime());
		}
	}

#ifdef _DEBUG
	DebugHud::Update();
#endif

	InputSystem::Update();

	//-------------------------------------------------------------------------
	// --- PreRender↓ ---
	renderer_->PreRender();
	srvManager_->PreDraw();
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
		if (currentScene_) {
			currentScene_->Render();
		}
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
	currentScene_->Shutdown();

	ModelManager::Shutdown();
	TextureManager::Shutdown();

	Debug::Shutdown();

#ifdef _DEBUG
	// ImGuiManagerのシャットダウンは最後に行う
	if (imGuiManager_) {
		imGuiManager_->Shutdown();
	}
#endif
}

void Engine::RegisterConsoleCommandsAndVariables() {
	// コンソールコマンドを登録
	ConCommand::RegisterCommand(
		"bind",
		[](const std::vector<std::string>& args) {
			if (args.size() < 2) {
				Console::Print("Usage: bind <key> <command>\n", kConsoleColorWarning, Channel::kInputSystem);
				return;
			}
			std::string key = args[0];
			std::string command = args[1];
			InputSystem::BindKey(key, command);
		},
		"Bind a key to a command."
	);
	ConCommand::RegisterCommand("clear", Console::Clear, "Clear all console output");
	ConCommand::RegisterCommand("cls", Console::Clear, "Clear all console output");
	ConCommand::RegisterCommand("echo", Console::Echo, "Echo text to console.");
	ConCommand::RegisterCommand("exit", Quit, "Exit the engine.");
	ConCommand::RegisterCommand("help", Console::Help, "Find help about a convar/concommand.");
	ConCommand::RegisterCommand("neofetch", Console::NeoFetch, "Show system info.");
	ConCommand::RegisterCommand("quit", Quit, "Exit the engine.");
	ConCommand::RegisterCommand("toggleconsole", Console::ToggleConsole, "Show/hide the console.");

	ConCommand::RegisterCommand(
		"toggleeditor",
		[]([[maybe_unused]] const std::vector<std::string>& args) {
			bIsEditorMode_ = !bIsEditorMode_;
			Console::Print("Editor mode is now " + std::to_string(bIsEditorMode_) + "\n", kConsoleColorNormal);
		},
		"Toggle editor mode."
	);

	// コンソール変数を登録
	ConVarManager::RegisterConVar<int>("cl_showpos", 1, "Draw current position at top of screen");
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

	// デバッグ用にaxisを表示するためのコンソール変数
	ConVarManager::RegisterConVar<int>("ent_axis", 0, "Show entity axis");

	// デフォルトのバインド
	Console::SubmitCommand("bind ` toggleconsole");
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
	Console::SubmitCommand("bind c +changecamera");
	Console::SubmitCommand("bind f1 toggleeditor");
	Console::SubmitCommand("bind esc togglelockcursor");
}

void Engine::Quit([[maybe_unused]] const std::vector<std::string>& args) {
	bWishShutdown_ = true;
}

void Engine::CheckEditorMode() {
	if (bIsEditorMode_) {
		editor_ = std::make_unique<Editor>(currentScene_);
	} else {
		editor_.reset();
	}
}

bool Engine::bWishShutdown_ = false;

#ifdef _DEBUG
bool Engine::bIsEditorMode_ = true;
#else
bool Engine::bIsEditorMode_ = false;
#endif

