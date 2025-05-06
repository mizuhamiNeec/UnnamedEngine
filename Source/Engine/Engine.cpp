#include "Engine.h"

#ifdef _DEBUG
#include <imgui_internal.h>
#endif

#include <Camera/CameraManager.h>
#include <Debug/Debug.h>
#include <Input/InputSystem.h>
#include <Lib/DebugHud/DebugHud.h>
#include <Lib/Utils/ClientProperties.h>
#include <Renderer/D3D12.h>
#include <Renderer/AbstractionLayer/D3D12/D3D12Renderer.h>
#include <Renderer/AbstractionLayer/Vulkan/VulkanRenderer.h>
#include <Scene/GameScene.h>
#include <SubSystem/Console/ConCommand.h>
#include <SubSystem/Console/Console.h>
#include <SubSystem/Console/ConVarManager.h>
#include <Window/EditorWindow.h>
#include <Window/MainWindow.h>
#include <Window/WindowsUtils.h>

#include "SrvManager.h"

Engine::Engine() = default;

void Engine::Run() {
	Init();
	while (!bWishShutdown_) {
		time_->StartFrame();
		if (wm_->ProcessMessage()) {
			PostQuitMessage(ERROR_SUCCESS);
			break;
		}
		Update();
		time_->EndFrame();
	}
	Shutdown();
}

void Engine::Init() {
	// メインビューポート用ウィンドウの作成
	auto gameWindow = std::make_unique<MainWindow>();

	WindowInfo gameWindowInfo = {
		.title = "GameWindow",
		.width = kClientWidth,
		.height = kClientHeight,
		.style = WS_OVERLAPPEDWINDOW,
		.exStyle = 0,
		.hInstance = GetModuleHandle(nullptr),
		.className = "gameWindowClassName"
	};

	if (gameWindow->Create(gameWindowInfo)) {
		wm_->AddWindow(std::move(gameWindow));
	} else {
		Console::Print(
			"Failed to create main window.\n",
			kConTextColorError,
			Channel::Engine
		);
		return;
	}

	// auto secondWindow_ = std::make_unique<EditorWindow>();

	// WindowInfo editorWindowInfo = {
	// 	.title = "EditorWindow",
	// 	.width = 400,
	// 	.height = 300,
	// 	.style = WS_OVERLAPPEDWINDOW,
	// 	.exStyle = 0,
	// 	.hInstance = GetModuleHandle(nullptr),
	// 	.className = "editorWindowClassName"
	// };

	// if (secondWindow_->Create(editorWindowInfo)) {
	// 	wm_->AddWindow(std::move(secondWindow_));
	// } else {
	// 	Console::Print(
	// 		"Failed to create second window.\n",
	// 		kConTextColorError,
	// 		Channel::Engine
	// 	);
	// 	return;
	// }

	renderer_ = std::make_unique<D3D12>(wm_->GetMainWindow());
	renderer_->Init();

	// 入力システム
	InputSystem::Init();

	// コンソールコマンドと変数の登録
	RegisterConsoleCommandsAndVariables();

	// 抽象化レイヤーテスト
	rendererInitInfo_ = {};
	if (ConVarManager::GetConVar("r_vulkanenabled")->GetValueAsBool()) {
		rendererInitInfo_.api = API::Vulkan;
	} else {
		rendererInitInfo_.api = API::DX12;
	}
	//rendererInitInfo_.windowHandle = wm_->GetWindows()[1]->GetWindowHandle();
#ifdef _DEBUG
	rendererInitInfo_.enableDebugLayer = true;
#else
	rendererInitInfo_.enableDebugLayer = false;
#endif

	if (rendererInitInfo_.api == API::DX12) {
		testRenderer_ = std::make_unique<D3D12Renderer>();
	} else if (rendererInitInfo_.api == API::Vulkan) {
		testRenderer_ = std::make_unique<VulkanRenderer>();
	}

	if (!testRenderer_->Init(rendererInitInfo_)) {
		Console::Print("Failed to initialize renderer.\n", kConTextColorError, Channel::RenderSystem);
		assert(true && "Failed to initialize renderer.");
		throw std::runtime_error("Failed to initialize renderer.");
	}

	// コマンドライン引数をコンソールに送信
	Console::SubmitCommand(ConVarManager::GetConVar("launchargs")->GetValueAsString());

	resourceManager_ = std::make_unique<ResourceManager>(renderer_.get());

#ifdef _DEBUG
	imGuiManager_ = std::make_unique<ImGuiManager>(renderer_.get(), resourceManager_->GetShaderResourceViewManager());
#endif

	console_ = std::make_unique<Console>();

	resourceManager_->Init();

	//// モデル
	//modelCommon_ = std::make_unique<ModelCommon>();
	//modelCommon_->Init(renderer_.get());

	// オブジェクト3D
	/*object3DCommon_ = std::make_unique<Object3DCommon>();
	object3DCommon_->Init(renderer_.get());*/

	//// スプライト
	//spriteCommon_ = std::make_unique<SpriteCommon>();
	//spriteCommon_->Init(renderer_.get());

	srvManager_ = std::make_unique<SrvManager>();
	srvManager_->Init(renderer_.get());

	TexManager::GetInstance()->Init(renderer_.get(), srvManager_.get());

	// パーティクル
	particleManager_ = std::make_unique<ParticleManager>();
	particleManager_->Init(renderer_.get(), srvManager_.get());

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

	//-------------------------------------------------------------------------
	// すべての初期化が完了
	//-------------------------------------------------------------------------

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
#endif

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

	//ConVarManager::GetConVar("w_title")->SetValueFromString(std::to_string(EngineTimer::GetFrameCount()));

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

	//time_->EndFrame();
}

void Engine::Shutdown() const {
	Debug::Shutdown();


	renderer_->Shutdown();

	testRenderer_->Shutdown();


#ifdef _DEBUG
	if (imGuiManager_) {
		imGuiManager_->Shutdown();
	}
#endif
	resourceManager_->Shutdown();
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
	ConVarManager::RegisterConVar<bool>("r_vulkanenabled", false, "Enable Vulkan renderer", ConVarFlags::ConVarFlags_Notify);
	ConVarManager::RegisterConVar<int>(
		"cl_showpos", 1, "Draw current position at top of screen (1 = meter, 2 = hammer)"
	);
	ConVarManager::RegisterConVar<int>("cl_showfps", 2, "Draw fps meter (1 = fps, 2 = smooth)");
	ConVarManager::RegisterConVar<int>("cl_fpsmax", kMaxFps, "Frame rate limiter");
	ConVarManager::RegisterConVar<std::string>("name", "unnamed", "Current user name", ConVarFlags::ConVarFlags_Notify);
	Console::SubmitCommand("name " + WindowsUtils::GetWindowsUserName(), true);
	ConVarManager::RegisterConVar<float>("sensitivity", 2.0f, "Mouse sensitivity.");
	ConVarManager::RegisterConVar<float>("host_timescale", 1.0f, "Prescale the clock by this amount.");
	// World
	ConVarManager::RegisterConVar<float>("sv_gravity", 800.0f, "World gravity.");
	ConVarManager::RegisterConVar<float>(
		"sv_maxvelocity",
		3500.0f,
		"Maximum speed any ballistically moving object is allowed to attain per axis."
	);

	// Player
	ConVarManager::RegisterConVar<float>("sv_accelerate", 10.0f, "Linear acceleration amount (old value is 5.6)");
	ConVarManager::RegisterConVar<float>("sv_airaccelerate", 12.0f);
	ConVarManager::RegisterConVar<float>("sv_maxspeed", 800.0f, "Maximum speed a player can move.");
	ConVarManager::RegisterConVar<float>("sv_stopspeed", 100.0f, "Minimum stopping speed when on ground.");
	ConVarManager::RegisterConVar<float>("sv_friction", 4.0f, "World friction.");

	// デバッグ用にエンティティのaxisを表示するためのコンソール変数
	ConVarManager::RegisterConVar<int>("ent_axis", 0, "Show entity axis");

	// デフォルトのバインド
	Console::SubmitCommand("bind esc togglelockcursor", true);
	Console::SubmitCommand("bind w +forward", true);
	Console::SubmitCommand("bind s +back", true);
	Console::SubmitCommand("bind a +moveleft", true);
	Console::SubmitCommand("bind d +moveright", true);
	Console::SubmitCommand("bind e +moveup", true);
	Console::SubmitCommand("bind q +movedown", true);
	Console::SubmitCommand("bind c +crouch", true);
	Console::SubmitCommand("bind space +jump", true);
	Console::SubmitCommand("bind mouse1 +attack1", true);
	Console::SubmitCommand("bind mouse2 +attack2", true);
	Console::SubmitCommand("bind mousewheelup +invprev", true);
	Console::SubmitCommand("bind mousewheeldown +invnext", true);
	Console::SubmitCommand("bind f1 toggleeditor", true);
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
std::unique_ptr<D3D12> Engine::renderer_;
std::unique_ptr<ResourceManager> Engine::resourceManager_;
std::unique_ptr<ParticleManager> Engine::particleManager_;

#ifdef _DEBUG
bool Engine::bIsEditorMode_ = true;
#else
bool Engine::bIsEditorMode_ = false;
#endif
