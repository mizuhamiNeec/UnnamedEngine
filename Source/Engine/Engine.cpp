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
#include <Scene/GameScene.h>
#include <SubSystem/Console/ConCommand.h>
#include <SubSystem/Console/Console.h>
#include <SubSystem/Console/ConVarManager.h>
#include <Window/EditorWindow.h>
#include <Window/MainWindow.h>
#include <Window/WindowsUtils.h>

#include "SrvManager.h"
#include "imgui_impl_dx12.h"

#include "CopyImagePass/CopyImagePass.h"
#include "ImGuiManager/Icons.h"
#include "ImGuiManager/ImGuiWidgets.h"
#include "ImGuizmo/ImGuizmo.h"
#include "Lib/Utils/StrUtil.h"

#include "TextureManager/TexManager.h"

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

constexpr Vec4 offscreenClearColor = {0.1f, 0.1f, 0.1f, 1.0f};

void Engine::OnResize(const uint32_t width, const uint32_t height) {
	if (width == 0 || height == 0) {
		return;
	}

	// GPUの処理が終わるまで待つ
	renderer_->Flush();
	//renderer_->WaitPreviousFrame();

	renderer_->Resize(width, height);

	const auto srvManager = resourceManager_->GetShaderResourceViewManager();

	srvManager->UnregisterResource(
		offscreenRTV_.rtv
	);
	srvManager->UnregisterResource(
		offscreenDSV_.dsv
	);

	srvManager->UnregisterResource(
		postProcessedRTV_.rtv
	);
	srvManager->UnregisterResource(
		postProcessedDSV_.dsv
	);

	offscreenRTV_.rtv.Reset();
	offscreenDSV_.dsv.Reset();

	postProcessedRTV_.rtv.Reset();
	postProcessedDSV_.dsv.Reset();

	offscreenRTV_ = {};
	offscreenDSV_ = {};

	postProcessedRTV_ = {};
	postProcessedDSV_ = {};

	offscreenRTV_ = renderer_->CreateRenderTargetTexture(
		width, height,
		offscreenClearColor,
		kBufferFormat
	);
	offscreenDSV_ = renderer_->CreateDepthStencilTexture(
		width, height,
		DXGI_FORMAT_D32_FLOAT
	);

	postProcessedRTV_ = renderer_->CreateRenderTargetTexture(
		width, height,
		offscreenClearColor,
		kBufferFormat
	);

	postProcessedDSV_ = renderer_->CreateDepthStencilTexture(
		width, height,
		DXGI_FORMAT_D32_FLOAT
	);

	offscreenRenderPassTargets_.pRTVs   = &offscreenRTV_.rtvHandle;
	offscreenRenderPassTargets_.numRTVs = 1;
	offscreenRenderPassTargets_.pDSV    = &offscreenDSV_.handles.cpuHandle;

	postProcessedRenderPassTargets_.pRTVs = &postProcessedRTV_.rtvHandle;
	postProcessedRenderPassTargets_.numRTVs = 1;
	postProcessedRenderPassTargets_.pDSV = &postProcessedDSV_.handles.cpuHandle;
}

void Engine::ResizeOffscreenRenderTextures(
	const uint32_t width,
	const uint32_t height
) {
	if (width == 0 || height == 0) {
		return;
	}

	// GPUの処理が終わるまで待つ
	renderer_->Flush();
	//renderer_->WaitPreviousFrame();

	// renderer_->Resize(width, height);

	renderer_->ResetOffscreenRenderTextures();

	const auto srvManager = resourceManager_->GetShaderResourceViewManager();

	srvManager->UnregisterResource(
		offscreenRTV_.rtv
	);
	srvManager->UnregisterResource(
		offscreenDSV_.dsv
	);

	srvManager->UnregisterResource(
		postProcessedRTV_.rtv
	);
	srvManager->UnregisterResource(
		postProcessedDSV_.dsv
	);

	offscreenRTV_.rtv.Reset();
	offscreenDSV_.dsv.Reset();

	postProcessedRTV_.rtv.Reset();
	postProcessedDSV_.dsv.Reset();

	offscreenRTV_ = {};
	offscreenDSV_ = {};

	postProcessedRTV_ = {};
	postProcessedDSV_ = {};

	offscreenRTV_ = renderer_->CreateRenderTargetTexture(
		width, height,
		offscreenClearColor,
		kBufferFormat
	);
	offscreenDSV_ = renderer_->CreateDepthStencilTexture(
		width, height,
		DXGI_FORMAT_D32_FLOAT
	);

	postProcessedRTV_ = renderer_->CreateRenderTargetTexture(
		width, height,
		offscreenClearColor,
		kBufferFormat
	);

	postProcessedDSV_ = renderer_->CreateDepthStencilTexture(
		width, height,
		DXGI_FORMAT_D32_FLOAT
	);

	offscreenRenderPassTargets_.pRTVs   = &offscreenRTV_.rtvHandle;
	offscreenRenderPassTargets_.numRTVs = 1;
	offscreenRenderPassTargets_.pDSV    = &offscreenDSV_.handles.cpuHandle;

	postProcessedRenderPassTargets_.pRTVs = &postProcessedRTV_.rtvHandle;
	postProcessedRenderPassTargets_.numRTVs = 1;
	postProcessedRenderPassTargets_.pDSV = &postProcessedDSV_.handles.cpuHandle;

	//renderer_->SetViewportAndScissor(width, height);
}

Vec2 Engine::GetViewportLT() {
	return viewportLT;
}

Vec2 Engine::GetViewportSize() {
	return viewportSize;
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

	renderer_ = std::make_unique<D3D12>(wm_->GetMainWindow());

	wm_->GetMainWindow()->SetResizeCallback(
		[this]([[maybe_unused]] const uint32_t width,
		       [[maybe_unused]] const uint32_t height) {
			OnResize(width, height);
		}
	);

	// 入力システム
	InputSystem::Init();

	// コンソールコマンドと変数の登録
	RegisterConsoleCommandsAndVariables();

	// コマンドライン引数をコンソールに送信
	Console::SubmitCommand(
		ConVarManager::GetConVar("launchargs")->GetValueAsString());

	resourceManager_ = std::make_unique<ResourceManager>(renderer_.get());

	#ifdef _DEBUG
	imGuiManager_ = std::make_unique<ImGuiManager>(
		renderer_.get(), resourceManager_->GetShaderResourceViewManager());
	#endif

	console_ = std::make_unique<Console>();

	resourceManager_->Init();

	renderer_->SetShaderResourceViewManager(
		resourceManager_->GetShaderResourceViewManager());
	renderer_->Init();

	offscreenRTV_ = renderer_->CreateRenderTargetTexture(
		wm_->GetMainWindow()->GetClientWidth(),
		wm_->GetMainWindow()->GetClientHeight(),
		offscreenClearColor,
		kBufferFormat
	);

	offscreenDSV_ = renderer_->CreateDepthStencilTexture(
		wm_->GetMainWindow()->GetClientWidth(),
		wm_->GetMainWindow()->GetClientHeight(),
		DXGI_FORMAT_D32_FLOAT
	);

	postProcessedRTV_ = renderer_->CreateRenderTargetTexture(
		wm_->GetMainWindow()->GetClientWidth(),
		wm_->GetMainWindow()->GetClientHeight(),
		offscreenClearColor,
		kBufferFormat
	);

	postProcessedDSV_ = renderer_->CreateDepthStencilTexture(
		wm_->GetMainWindow()->GetClientWidth(),
		wm_->GetMainWindow()->GetClientHeight(),
		DXGI_FORMAT_D32_FLOAT
	);

	offscreenRenderPassTargets_.pRTVs        = &offscreenRTV_.rtvHandle;
	offscreenRenderPassTargets_.numRTVs      = 1;
	offscreenRenderPassTargets_.pDSV         = &offscreenDSV_.handles.cpuHandle;
	offscreenRenderPassTargets_.clearColor   = offscreenClearColor;
	offscreenRenderPassTargets_.clearDepth   = 1.0f;
	offscreenRenderPassTargets_.clearStencil = 0;
	offscreenRenderPassTargets_.bClearColor  = true;
	offscreenRenderPassTargets_.bClearDepth  = true;

	postProcessedRenderPassTargets_.pRTVs = &postProcessedRTV_.rtvHandle;
	postProcessedRenderPassTargets_.numRTVs = 1;
	postProcessedRenderPassTargets_.pDSV = &postProcessedDSV_.handles.cpuHandle;
	postProcessedRenderPassTargets_.clearColor = offscreenClearColor;
	postProcessedRenderPassTargets_.clearDepth = 1.0f;
	postProcessedRenderPassTargets_.clearStencil = 0;
	postProcessedRenderPassTargets_.bClearColor = true;
	postProcessedRenderPassTargets_.bClearDepth = true;

	copyImagePass_ = std::make_unique<CopyImagePass>(renderer_->GetDevice());

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
	ImGuizmo::BeginFrame();
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
		{
			static ImVec4 tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
			static ImVec4 bg   = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

			ImGuiWindowFlags windowFlags =
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoScrollWithMouse;

			if (ImGuizmo::IsUsing()) {
				windowFlags |= ImGuiWindowFlags_NoMove;
			}

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::Begin(
				"ViewPort",
				nullptr,
				windowFlags
			);
			ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

			ImVec2     avail = ImGui::GetContentRegionAvail();
			const auto ptr   = postProcessedRTV_.srvHandles.gpuHandle.ptr;

			static int prevW = 0, prevH = 0;
			int        w     = static_cast<int>(avail.x);
			int        h     = static_cast<int>(avail.y);
			if ((w != prevW || h != prevH) && w > 0 && h > 0) {
				// ビューポートサイズがおかしくなるので一旦廃止
				//ResizeOffscreenRenderTextures(w, h);
				/*CameraManager::GetActiveCamera()->SetAspectRatio(
					static_cast<float>(w) / static_cast<float>(h)
				);*/
				prevW = w;
				prevH = h;
			}

			if (ptr) {
				const ImTextureID texId =
					postProcessedRTV_.srvHandles.gpuHandle.ptr;

				// リソースからテクスチャの幅と高さを取得
				auto        desc      = postProcessedRTV_.rtv->GetDesc();
				const float texWidth  = static_cast<float>(desc.Width);
				const float texHeight = static_cast<float>(desc.Height);

				const float availAspect = avail.x / avail.y;
				const float texAspect   = texWidth / texHeight;

				ImVec2 drawSize = avail;
				if (availAspect > texAspect) {
					// 横が余る
					drawSize.x = avail.y * texAspect;
				} else {
					// 縦が余る
					drawSize.y = avail.x / texAspect;
				}

				float titleBarHeight = ImGui::GetCurrentWindow()->
					TitleBarHeight;

				// ここで中央に配置するためのオフセットを計算
				ImVec2 offset = {
					(avail.x - drawSize.x) * 0.5f,
					(avail.y - drawSize.y) * 0.5f + titleBarHeight
				};
				ImGui::SetCursorPos(offset);

				ImVec2 viewportScreenPos = ImGui::GetCursorScreenPos();

				ImGui::ImageWithBg(
					texId,
					drawSize,
					ImVec2(0, 0), ImVec2(1, 1),
					bg, tint
				);

				viewportLT   = {viewportScreenPos.x, viewportScreenPos.y};
				viewportSize = {drawSize.x, drawSize.y};
			}
			ImGui::End();
			ImGui::PopStyleVar();
		}

		copyImagePass_->Update(EngineTimer::GetDeltaTime());
	} else {
		sceneManager_->Update(EngineTimer::GetScaledDeltaTime());
		viewportLT   = Vec2::zero;
		viewportSize = {
			static_cast<float>(wm_->GetMainWindow()->GetClientWidth()),
			static_cast<float>(wm_->GetMainWindow()->GetClientHeight())
		};
	}

	InputSystem::Update();

	#ifdef _DEBUG
	Console::Update();
	DebugHud::Update();
	#endif

	offscreenRenderPassTargets_.bClearColor =
		ConVarManager::GetConVar("r_clear")->GetValueAsBool();


	//ConVarManager::GetConVar("w_title")->SetValueFromString(std::to_string(EngineTimer::GetFrameCount()));

	#ifdef _DEBUG
	Debug::Update();
	#endif
	CameraManager::Update(EngineTimer::GetDeltaTime());

	//-------------------------------------------------------------------------
	// --- PreRender↓ ---
	renderer_->PreRender();
	//-------------------------------------------------------------------------

	// renderer_->SetViewportAndScissor(
	// 	WindowManager::GetMainWindow()->GetClientWidth(),
	// 	WindowManager::GetMainWindow()->GetClientHeight()
	// );
	renderer_->SetViewportAndScissor(
		static_cast<uint32_t>(offscreenRTV_.rtv->GetDesc().Width),
		static_cast<uint32_t>(offscreenRTV_.rtv->GetDesc().Height)
	);
	renderer_->BeginRenderPass(offscreenRenderPassTargets_);
	if (IsEditorMode()) {
		lineCommon_->Render();
		Debug::Draw();
		if (editor_) {
			editor_->Render();
		}
	} else {
		sceneManager_->Render();
	}

	// バリアを設定
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource   = offscreenRTV_.rtv.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	renderer_->GetCommandList()->ResourceBarrier(1, &barrier);

	if (IsEditorMode()) {
		// エディターモード時はポストプロセス用RTVに描画
		renderer_->BeginRenderPass(postProcessedRenderPassTargets_);
		renderer_->SetViewportAndScissor(
			static_cast<uint32_t>(offscreenRTV_.rtv->GetDesc().Width),
			static_cast<uint32_t>(offscreenRTV_.rtv->GetDesc().Height)
		);
		copyImagePass_->Execute(
			renderer_->GetCommandList(),
			offscreenRTV_.rtv.Get(),
			postProcessedRTV_.rtvHandle,
			resourceManager_->GetShaderResourceViewManager()
		);

		// バリアを設定
		D3D12_RESOURCE_BARRIER postBarrier = {};
		postBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		postBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		postBarrier.Transition.pResource = postProcessedRTV_.rtv.Get();
		postBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		postBarrier.Transition.StateAfter =
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		renderer_->GetCommandList()->ResourceBarrier(1, &postBarrier);
	} else {
		// ゲーム時はスワップチェーンに直接描画
		renderer_->BeginSwapChainRenderPass();
		renderer_->SetViewportAndScissor(
			WindowManager::GetMainWindow()->GetClientWidth(),
			WindowManager::GetMainWindow()->GetClientHeight()
		);
		copyImagePass_->Execute(
			renderer_->GetCommandList(),
			offscreenRTV_.rtv.Get(),
			renderer_->GetSwapChainRenderTargetView(),
			resourceManager_->GetShaderResourceViewManager()
		);
	}

	//------------------------------------------------------------------------
	// --- PostRender↓ ---
	if (IsEditorMode()) {
		renderer_->BeginSwapChainRenderPass();
	}

	#ifdef _DEBUG
	imGuiManager_->EndFrame();
	#endif

	// バリアを元に戻す
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
	renderer_->GetCommandList()->ResourceBarrier(1, &barrier);

	if (IsEditorMode()) {
		// postProcessedRTV_ のバリアを戻す
		D3D12_RESOURCE_BARRIER postBarrier = {};
		postBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		postBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		postBarrier.Transition.pResource = postProcessedRTV_.rtv.Get();
		postBarrier.Transition.StateBefore =
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		postBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		renderer_->GetCommandList()->ResourceBarrier(1, &postBarrier);
	}

	renderer_->PostRender();

	//-------------------------------------------------------------------------

	//time_->EndFrame();
}

void Engine::Shutdown() const {
	renderer_->WaitPreviousFrame();

	Debug::Shutdown();

	copyImagePass_->Shutdown();

	TexManager::GetInstance()->Shutdown();

	particleManager_->Shutdown();
	particleManager_.reset();

	renderer_->Shutdown();

	#ifdef _DEBUG
	if (imGuiManager_) {
		imGuiManager_->Shutdown();
	}
	#endif
	resourceManager_->Shutdown();
	resourceManager_.reset();
}

void Engine::RegisterConsoleCommandsAndVariables() {
	// コンソールコマンドを登録
	ConCommand::RegisterCommand("exit", Quit, "Exit the engine.");
	ConCommand::RegisterCommand("quit", Quit, "Exit the engine.");

	ConCommand::RegisterCommand(
		"toggleeditor",
		[]([[maybe_unused]] const std::vector<std::string>& args) {
			bIsEditorMode_ = !bIsEditorMode_;
			Console::Print(
				"Editor mode is now " + std::to_string(bIsEditorMode_) + "\n",
				kConFgColorDark);
		},
		"Toggle editor mode."
	);

	// コンソール変数を登録
	ConVarManager::RegisterConVar<bool>("r_vulkanenabled", false,
	                                    "Enable Vulkan renderer",
	                                    ConVarFlags::ConVarFlags_Notify);
	ConVarManager::RegisterConVar<int>(
		"cl_showpos", 1,
		"Draw current position at top of screen (1 = meter, 2 = hammer)"
	);
	ConVarManager::RegisterConVar<int>("cl_showfps", 2,
	                                   "Draw fps meter (1 = fps, 2 = smooth)");
	ConVarManager::RegisterConVar<int>("cl_fpsmax", kMaxFps,
	                                   "Frame rate limiter");
	ConVarManager::RegisterConVar<std::string>("name", "unnamed",
	                                           "Current user name",
	                                           ConVarFlags::ConVarFlags_Notify);
	Console::SubmitCommand("name " + WindowsUtils::GetWindowsUserName(), true);
	ConVarManager::RegisterConVar<float>("sensitivity", 2.0f,
	                                     "Mouse sensitivity.");
	ConVarManager::RegisterConVar<float>("host_timescale", 1.0f,
	                                     "Prescale the clock by this amount.");
	// World
	ConVarManager::RegisterConVar<
		float>("sv_gravity", 800.0f, "World gravity.");
	ConVarManager::RegisterConVar<float>(
		"sv_maxvelocity",
		3500.0f,
		"Maximum speed any ballistically moving object is allowed to attain per axis."
	);

	// Player
	ConVarManager::RegisterConVar<float>("sv_accelerate", 10.0f,
	                                     "Linear acceleration amount (old value is 5.6)");
	ConVarManager::RegisterConVar<float>("sv_airaccelerate", 12.0f);
	ConVarManager::RegisterConVar<float>("sv_maxspeed", 800.0f,
	                                     "Maximum speed a player can move.");
	ConVarManager::RegisterConVar<float>("sv_stopspeed", 100.0f,
	                                     "Minimum stopping speed when on ground.");
	ConVarManager::RegisterConVar<
		float>("sv_friction", 4.0f, "World friction.");

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
	Console::SubmitCommand("bind r +reload", true);
	Console::SubmitCommand("bind mousewheelup +invprev", true);
	Console::SubmitCommand("bind mousewheeldown +invnext", true);
	Console::SubmitCommand("bind f1 toggleeditor", true);

	Console::SubmitCommand("bind q +bounds", true);
	Console::SubmitCommand("bind t +translate", true);
	Console::SubmitCommand("bind r +rotate", true);
	Console::SubmitCommand("bind e +scale", true);
	Console::SubmitCommand("bind tab +toggleGizmo", true);
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

bool                             Engine::bWishShutdown_ = false;
std::unique_ptr<D3D12>           Engine::renderer_;
std::unique_ptr<ResourceManager> Engine::resourceManager_;
std::unique_ptr<ParticleManager> Engine::particleManager_;

Vec2 Engine::viewportLT   = Vec2::zero;
Vec2 Engine::viewportSize = Vec2::zero;

#ifdef _DEBUG
bool Engine::bIsEditorMode_ = true;
#else
bool Engine::bIsEditorMode_ = false;
#endif
