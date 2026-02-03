#include <pch.h>

#ifdef _DEBUG
#include <imgui_internal.h>
// ImGuizmoのインクルードはImGuiより後! いいね!?
#include <ImGuizmo.h>
#endif

#include <engine/Engine.h>
#include <engine/Camera/CameraManager.h>
#include <engine/Debug/DebugDraw.h>
#include <engine/Debug/DebugHud.h>
#include <engine/ImGui/ImGuiWidgets.h>
#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/ConCommand.h>
#include <engine/OldConsole/ConVarManager.h>
#include <engine/Platform/Window.h>
#include <engine/postprocess/PostProcessPipeline.h>
#include <engine/postprocess/PPBloom.h>
#include <engine/postprocess/PPChromaticAberration.h>
#include <engine/postprocess/PPRadialBlur.h>
#include <engine/postprocess/PPVignette.h>
#include <engine/ResourceSystem/Audio/AudioManager.h>
#include <engine/state/EditorModeState.h>
#include <engine/state/GameModeState.h>
#include <engine/unnamed/subsystem/console/ConsoleScriptParser.h>
#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/console/concommand/UnnamedConVar.h>
#include <engine/unnamed/subsystem/input/device/keyboard/KeyboardDevice.h>
#include <engine/unnamed/subsystem/input/device/mouse/MouseDevice.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>
#include <engine/unnamed/subsystem/time/TimeSystem.h>
#include <engine/Window/WindowsUtils.h>
#include <game/scene/GameScene.h>

#include "Platform/Win32App.h"

#include "Window/MainWindow.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "Engine";

	/// @brief コンストラクタ
	Engine::Engine() = default;

	/// @brief デストラクタ
	Engine::~Engine() = default;


	int Engine::Run() {
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); // リークチェック
		[[maybe_unused]] HRESULT hr = CoInitializeEx(
			nullptr, COINIT_MULTITHREADED
		);
		if (!Init()) {
			UASSERT(false && "Failed to initialize Engine");
			return EXIT_FAILURE;
		}
		while (!Win32App::PollEvents()) {
			if (OldWindowManager::ProcessMessage()) { break; }
			Tick();
		}
		Shutdown();
		CoUninitialize();
		return EXIT_SUCCESS;
	}

	/// @brief 初期化
	/// @return 成功したらtrueを返す
	bool Engine::Init() {
		ServiceLocator::Register<Engine>(this);
#ifdef _DEBUG
		ConVarManager::RegisterConVar<bool>(
			"verbose", true, "Enable verbose logging"
		);
#else
		ConVarManager::RegisterConVar<bool>(
			"verbose", false, "Enable verbose logging"
		);
#endif

		// ConsoleSystemの初期化
		mConsoleSystem = std::make_unique<ConsoleSystem>();
		if (!mConsoleSystem->Init()) { return false; }

		// TimeSystemの初期化
		mTimeSystem = std::make_unique<TimeSystem>();
		if (!mTimeSystem->Init()) { return false; }

		// メインウィンドウの作成
		auto gameWindow = std::make_unique<MainWindow>();

		// ウィンドウ情報の設定
		WindowInfo gameWindowInfo = {
			.title     = "GameWindow",
			.width     = kClientWidth,
			.height    = kClientHeight,
			.style     = WS_OVERLAPPEDWINDOW,
			.exStyle   = 0,
			.hInstance = GetModuleHandle(nullptr),
			.className = "gameWindowClassName"
		};

		// ウィンドウの作成
		if (gameWindow->Create(gameWindowInfo)) {
			mWindowManager->AddWindow(std::move(gameWindow));
		} else {
			Fatal("Engine", "Failed to create main window.");
			throw std::runtime_error("Failed to create main window.");
		}

		// InputSystemの初期化
		mInputSystem = std::make_unique<UInputSystem>();
		if (!mInputSystem->Init()) { return false; }
		// デバイス登録
		const auto keyboardDevice = std::make_shared<KeyboardDevice>(
			mWindowManager->GetMainWindow()->GetWindowHandle()
		);
		const auto mouseDevice = std::make_shared<MouseDevice>(
			mWindowManager->GetMainWindow()->GetWindowHandle()
		);
		mInputSystem->RegisterDevice(keyboardDevice);
		mInputSystem->RegisterDevice(mouseDevice);

		mRenderer = std::make_unique<D3D12>(mWindowManager->GetMainWindow());

		mWindowManager->GetMainWindow()->SetResizeCallback(
			[this](
			[[maybe_unused]] const uint32_t width,
			[[maybe_unused]] const uint32_t height
		) {
				OnResize(width, height);
			}
		);

		InputSystem::Init();

		RegisterConsoleCommandsAndVariables();

		// 各マネージャーの初期化
		mResourceManager = std::make_unique<ResourceManager>(mRenderer.get());
		mSrvManager      = std::make_unique<SrvManager>();
		mSrvManager->Init(mRenderer.get());
		mResourceManager->Init();
		mRenderer->SetShaderResourceViewManager(mSrvManager.get());
		mRenderer->Init();

		mAudioManager = std::make_unique<AudioManager>();
		mAudioManager->Init();

#ifdef _DEBUG
		// ImGuiFontテクスチャ用にSRVを確保
		mSrvManager->AllocateForTexture2D();
		// ImGuiManagerの初期化
		mImGuiManager = std::make_unique<ImGuiManager>(
			mRenderer.get(), mSrvManager.get()
		);
#endif

		// コンソールを作成
		mConsole = std::make_unique<Console>();

#pragma region PostProcessInit
		auto* wndMgr = mWindowManager->GetMainWindow();

		const uint32_t kClWidth  = wndMgr->GetClientWidth();
		const uint32_t kClHeight = wndMgr->GetClientHeight();

		mRenderTargets.Init(
			mRenderer.get(),
			kClWidth,
			kClHeight,
			kOffscreenClearColor,
			kBufferFormat,
			DXGI_FORMAT_D32_FLOAT
		);

		mPostProcessPipeline.Init(
			mRenderer.get(),
			mSrvManager.get(),
			kClWidth,
			kClHeight,
			reinterpret_cast<const float*>(&kOffscreenClearColor),
			kBufferFormat,
			DXGI_FORMAT_D32_FLOAT
		);

		mPostProcessPipeline.AddPass(
			std::make_unique<PPBloom>(
				mRenderer->GetDevice(), mSrvManager.get()
			)
		);

		mPostProcessPipeline.AddPass(
			std::make_unique<PPVignette>(
				mRenderer->GetDevice(), mSrvManager.get()
			)
		);

		mPostProcessPipeline.AddPass(
			std::make_unique<PPChromaticAberration>(
				mRenderer->GetDevice(), mSrvManager.get()
			)
		);

		mPostProcessPipeline.AddPass(
			std::make_unique<PPRadialBlur>(
				mRenderer->GetDevice(), mSrvManager.get()
			)
		);
#pragma endregion

		// スプライト
		mSpriteCommon = std::make_unique<SpriteCommon>();
		mSpriteCommon->Init(mRenderer.get());

		// パーティクル
		mParticleManager = std::make_unique<ParticleManager>();
		mParticleManager->Init(mRenderer.get(), mSrvManager.get());

		// ライン
		mLineCommon = std::make_unique<LineCommon>();
		mLineCommon->Init(mRenderer.get());

		DebugDraw::Init(mLineCommon.get());

		//---------------------------------------------------------------------
		// すべての初期化が完了
		//---------------------------------------------------------------------

		Console::SubmitCommand("neofetch");

		// エンティティローダーの作成
		mEntityLoader = std::make_unique<EntityLoader>();

		// シーンマネージャ/ファクトリーの作成
		mSceneFactory = std::make_unique<SceneFactory>();
		mSceneManager = std::make_shared<SceneManager>(*mSceneFactory);
		// ゲームシーンを登録
		mSceneFactory->RegisterScene<GameScene>("GameScene");
		// シーンの初期化
		mSceneManager->ChangeScene("GameScene");

		//---------------------------------------------------------------------
		// エディターの初期化
		//---------------------------------------------------------------------
		CheckEditorMode();

		assert(SUCCEEDED(mRenderer->GetCommandList()->Close()));

		mConsoleSystem->ExecuteCommand(
			"exec ./content/core/cfg/config_default.cfg"
		);

		return true;
	}

	/// @brief 更新
	void Engine::Tick() {
		mTimeSystem->BeginFrame();
		float deltaTime = mTimeSystem->GetGameTime()->DeltaTime<float>();

		mInputSystem->Update(deltaTime);

#ifdef _DEBUG
		ImGuiManager::NewFrame();
		ImGuizmo::BeginFrame();
#endif

		// 前のフレームとeditorModeが違う場合はStateを切り替える
		static bool bPrevEditorMode = mIsEditorMode;
		if (bPrevEditorMode != mIsEditorMode) {
			CheckEditorMode();
			bPrevEditorMode = mIsEditorMode;
		}

		/* ----------- 更新処理 ---------- */
		if (mModeState) { mModeState->Update(*this, deltaTime); }

		mConsoleSystem->Update(deltaTime);

		auto viewportCenter =
			GetViewportLTInstance() + GetViewportSizeInstance() * 0.5f;
		InputSystem::CheckMouseCursorLock(
			mWindowManager->GetMainWindow()->GetWindowHandle(),
			static_cast<int32_t>(viewportCenter.x),
			static_cast<int32_t>(viewportCenter.y)
		);
		InputSystem::Update();

#ifdef _DEBUG
		Console::Update();
		DebugHud::Update(mTimeSystem->GetGameTime()->ScaledDeltaTime<float>());
#endif

#ifdef _DEBUG
		DebugDraw::Update();
#endif
		CameraManager::Update(
			mTimeSystem->GetGameTime()->ScaledDeltaTime<float>()
		);

		auto offscreenTargets        = mRenderTargets.GetOffscreenPassTargets();
		offscreenTargets.bClearColor =
			ConVarManager::GetConVar("r_clear")->GetValueAsBool();
		//---------------------------------------------------------------------
		// --- PreRender↓ ---
		mRenderer->PreRender();
		//---------------------------------------------------------------------

		mRenderer->SetViewportAndScissor(
			static_cast<uint32_t>(mRenderTargets.GetOffscreenRtv().rtv->
			                                     GetDesc().Width),
			mRenderTargets.GetOffscreenRtv().rtv->GetDesc().Height
		);
		mRenderer->BeginRenderPass(offscreenTargets);

#ifdef _DEBUG
		if (mIsEditorMode) {
			mLineCommon->Render();
			DebugDraw::Draw();
		}
#endif

		if (mModeState) { mModeState->Render(*this); }

		// 先にバリアを設定
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource   = mRenderTargets.GetOffscreenRtv().rtv.
			Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter  =
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		mRenderer->GetCommandList()->ResourceBarrier(1, &barrier);

		auto* radialBlur = mPostProcessPipeline.FindPass<PPRadialBlur>();
		if (radialBlur) { radialBlur->SetBlurStrength(mBlurStrength); }

		// 最終出力先はモードで切り替え
		D3D12_CPU_DESCRIPTOR_HANDLE finalOutRtv;
		uint32_t                    finalW;
		uint32_t                    finalH;
		if (!mIsEditorMode) {
			if (!bSwapchainPassBegun) {
				mRenderer->BeginSwapChainRenderPass();
				bSwapchainPassBegun = true;
			}
			finalOutRtv = mRenderer->GetSwapChainRenderTargetView();
			finalW      = OldWindowManager::GetMainWindow()->GetClientWidth();
			finalH      = OldWindowManager::GetMainWindow()->GetClientHeight();
		} else {
			finalOutRtv = mRenderTargets.GetPostProcessedRtv().rtvHandle;
			finalW = static_cast<uint32_t>(mRenderTargets.GetOffscreenRtv().rtv
				->GetDesc().Width);
			finalH = mRenderTargets.GetOffscreenRtv().rtv->GetDesc().Height;
		}

		mPostProcessPipeline.Execute(
			mRenderer->GetCommandList(),
			mRenderTargets.GetOffscreenRtv().rtv.Get(),
			finalOutRtv,
			finalW,
			finalH
		);

		if (mIsEditorMode && mSrvManager) {
			auto& postRtv = mRenderTargets.GetPostProcessedRtv();
			mSrvManager->CreateSRVForTexture2D(
				postRtv.srvIndex,
				postRtv.rtv.Get(),
				postRtv.rtv->GetDesc().Format,
				1
			);
			postRtv.srvHandleGPU = mSrvManager->GetGPUDescriptorHandle(
				postRtv.srvIndex
			);
		}

		//---------------------------------------------------------------------
		// --- PostRender↓ ---
		if (mIsEditorMode) { mRenderer->BeginSwapChainRenderPass(); }

#ifdef _DEBUG
		mImGuiManager->EndFrame();
#endif

		// バリアを元に戻す
		barrier.Transition.StateBefore =
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		mRenderer->GetCommandList()->ResourceBarrier(1, &barrier);

		if (mIsEditorMode) {
			// mPostProcessedRtv のバリアを戻す
			D3D12_RESOURCE_BARRIER postBarrier = {};
			postBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			postBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			postBarrier.Transition.pResource = mRenderTargets.
			                                   GetPostProcessedRtv().rtv.Get();
			postBarrier.Transition.StateBefore =
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			postBarrier.Transition.StateAfter =
				D3D12_RESOURCE_STATE_RENDER_TARGET;
			mRenderer->GetCommandList()->ResourceBarrier(1, &postBarrier);
		}

		mRenderer->PostRender();

		mTimeSystem->EndFrame();
	}

	/// @brief シャットダウン
	void Engine::Shutdown() const {
		mRenderer->WaitPreviousFrame();

		DebugDraw::Shutdown();

		// TexManager は ResourceManager が破棄します

		mParticleManager->Shutdown();
		mParticleManager.reset();

		mRenderer->Shutdown();

#ifdef _DEBUG
		if (mImGuiManager) { mImGuiManager->Shutdown(); }
#endif

		mResourceManager->Shutdown();
		mResourceManager.reset();

		mInputSystem->Shutdown();
		mConsoleSystem->Shutdown();
		mTimeSystem->Shutdown();

		SpecialMsg(
			LogLevel::Success,
			"Engine",
			"アリーヴェ帰ルチ! (さよナランチャ"
		);
	}

	/// @brief ウィンドウリサイズ時の処理
	/// @param width 幅
	/// @param height 高さ
	void Engine::OnResize(const uint32_t width, const uint32_t height) {
		if (width == 0 || height == 0) { return; }

		mRenderTargets.OnResize(width, height);
		mPostProcessPipeline.OnResize(width, height);
	}

	/// @brief オフスクリーンレンダーテクスチャのリサイズ
	/// @param width 幅
	/// @param height 高さ
	void Engine::ResizeOffscreenRenderTextures(
		const uint32_t width,
		const uint32_t height
	) {
		if (width == 0 || height == 0) { return; }

		mRenderTargets.OnResize(width, height);
		mPostProcessPipeline.OnResize(width, height);
	}

	/// @brief コンソールコマンドと変数の登録
	void Engine::RegisterConsoleCommandsAndVariables() {
		// コンソールコマンドを登録
		ConCommand::RegisterCommand("exit", Quit, "Exit the engine.");
		ConCommand::RegisterCommand("quit", Quit, "Exit the engine.");

#ifdef _DEBUG
		ConCommand::RegisterCommand(
			"toggleeditor",
			[]([[maybe_unused]] const std::vector<std::string>& args) {
				Engine::mIsEditorMode = !Engine::mIsEditorMode;
				Warning(
					"Engine",
					"Editor mode is now {}",
					std::to_string(Engine::mIsEditorMode)
				);
			},
			"Toggle editor mode."
		);
#endif

		// コンソール変数を登録
		ConVarManager::RegisterConVar(
			"cl_showpos", 2,
			"Draw current position at top of screen (1 = meter, 2 = hammer)"
		);
		ConVarManager::RegisterConVar(
			"cl_showfps", 2,
			"Draw fps meter (1 = fps, 2 = smooth)"
		);
		ConVarManager::RegisterConVar(
			"fps_max", kDefaultFpsMax,
			"Frame rate limiter"
		);
		ConVarManager::RegisterConVar<std::string>(
			"name", "unnamed",
			"Current user name",
			ConVarFlags::ConVarFlags_Notify
		);
		Console::SubmitCommand(
			"name " + WindowsUtils::GetWindowsUserName(), true
		);
		ConVarManager::RegisterConVar(
			"sensitivity", 2.0f,
			"Mouse sensitivity."
		);
		// World
		ConVarManager::RegisterConVar("sv_gravity", 1000.0f, "World gravity.");
		ConVarManager::RegisterConVar(
			"sv_maxvelocity", 3500.0f,
			"Maximum speed any ballistically moving object is allowed to attain per axis."
		);

		// Player
		ConVarManager::RegisterConVar(
			"sv_accelerate", 20.0f,
			"Linear acceleration amount (old value is 5.6)"
		);
		ConVarManager::RegisterConVar("sv_airaccelerate", 12.0f);
		ConVarManager::RegisterConVar(
			"sv_maxspeed", 320.0f,
			"Maximum speed a player can move."
		);
		ConVarManager::RegisterConVar(
			"sv_stopspeed", 100.0f,
			"Minimum stopping speed when on ground."
		);
		ConVarManager::RegisterConVar("sv_friction", 8.0f, "World friction.");
		ConVarManager::RegisterConVar(
			"sv_stepsize", 18.0f,
			"Maximum step height."
		);

		// デバッグ用にエンティティのaxisを表示するかのコンソール変数
		ConVarManager::RegisterConVar("ent_axis", 0, "Show entity axis");
	}

	/// @brief エンジン終了コマンド
	/// @param args 引数
	void Engine::Quit([[maybe_unused]] const std::vector<std::string>& args) {
		mWishShutdown = true;
	}

	/// @brief エディターモードの確認と切り替え
	void Engine::CheckEditorMode() {
		// フラグに応じて State を差し替える
		if (mIsEditorMode) {
			SetModeState(std::make_unique<EditorModeState>());
		} else { SetModeState(std::make_unique<GameModeState>()); }
	}

	void Engine::SetModeState(std::unique_ptr<IEngineModeState> state) {
		// 既存 State を終了し、新 State を開始
		if (mModeState) { mModeState->OnExit(*this); }
		mModeState = std::move(state);
		if (mModeState) { mModeState->OnEnter(*this); }
	}

	void Engine::CreateEditor() {
		if (mEditor) { return; }
		mEditor = std::make_unique<Editor>(
			mSceneManager.get(), mTimeSystem->GetGameTime()
		);
		mEditor->SetEntityLoader(mEntityLoader.get());
	}

	void Engine::DestroyEditor() { mEditor.reset(); }

	void Engine::SetViewportToMainWindow() {
		mViewportLT   = Vec2::zero;
		mViewportSize = {
			static_cast<float>(mWindowManager->GetMainWindow()->
			                                   GetClientWidth()),
			static_cast<float>(mWindowManager->GetMainWindow()->
			                                   GetClientHeight())
		};
	}

	void Engine::SetViewportFromEditor(float x, float y, float w, float h) {
		mViewportLT   = {x, y};
		mViewportSize = {w, h};
	}

	bool                             Engine::mWishShutdown    = false;
	std::unique_ptr<AudioManager>    Engine::mAudioManager    = nullptr;
	std::unique_ptr<D3D12>           Engine::mRenderer        = nullptr;
	std::unique_ptr<ResourceManager> Engine::mResourceManager = nullptr;
	std::unique_ptr<ParticleManager> Engine::mParticleManager = nullptr;
	std::unique_ptr<SrvManager>      Engine::mSrvManager      = nullptr;
	std::unique_ptr<SpriteCommon>    Engine::mSpriteCommon    = nullptr;
	std::shared_ptr<SceneManager>    Engine::mSceneManager    = nullptr;

	Vec2 Engine::mViewportLT   = Vec2::zero;
	Vec2 Engine::mViewportSize = Vec2::zero;

	std::optional<std::string> Engine::mPendingSceneChange = std::nullopt;

#ifdef _DEBUG
	bool Engine::mIsEditorMode = true;
#else
	bool Engine::mIsEditorMode = false;
#endif
}
