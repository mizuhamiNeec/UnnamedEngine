#include <pch.h>

#ifdef _DEBUG
#include <imgui_internal.h>
#include <ImGuizmo.h>
#endif

#include <engine/public/Engine.h>
#include <engine/public/Camera/CameraManager.h>
#include <engine/public/Debug/Debug.h>
#include <engine/public/Debug/DebugHud.h>
#include <engine/public/ImGui/Icons.h>
#include <engine/public/ImGui/ImGuiWidgets.h>
#include <engine/public/Input/InputSystem.h>
#include <engine/public/OldConsole/ConVarManager.h>
#include <engine/public/renderer/SrvManager.h>
#include <engine/public/subsystem/console/ConsoleSystem.h>
#include <engine/public/subsystem/interface/ServiceLocator.h>
#include <engine/public/TextureManager/TexManager.h>
#include <engine/public/Timer/EngineTimer.h>
#include <engine/public/Window/MainWindow.h>

#include <game/public/scene/EmptyScene.h>
#include <game/public/scene/GameScene.h>

#include "engine/public/OldConsole/ConCommand.h"
#include "engine/public/Window/WindowsUtils.h"

constexpr Vec4 offscreenClearColor = Vec4(0.25f, 0.25f, 0.25f, 1.0f);

Engine::Engine() {
}

Engine::~Engine() {
}

bool Engine::Init() {
	//-----------------------------------------------------------------------------
	// Purpose: 旧エンジン
	//-----------------------------------------------------------------------------
#ifdef _DEBUG
	ConVarManager::RegisterConVar<bool>("verbose", true,
	                                    "Enable verbose logging");
#else
	ConVarManager::RegisterConVar<bool>("verbose", false, "Enable verbose logging");
#endif
	Console::Print("command line arguments:\n", kConTextColorGray,
	               Channel::CommandLine);
	Console::Print(StrUtil::ToString(GetCommandLineW()) + "\n",
	               kConTextColorGray,
	               Channel::CommandLine);
	ConVarManager::RegisterConVar<std::string>("launchargs",
	                                           StrUtil::ToString(
		                                           GetCommandLineW()),
	                                           "Command line arguments");
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
		mWindowManager->AddWindow(std::move(gameWindow));
	} else {
		Console::Print(
			"Failed to create main window.\n",
			kConTextColorError,
			Channel::Engine
		);
		return false;
	}

	mRenderer = std::make_unique<D3D12>(mWindowManager->GetMainWindow());

	mWindowManager->GetMainWindow()->SetResizeCallback(
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

	mResourceManager = std::make_unique<ResourceManager>(mRenderer.get());

	mSrvManager = std::make_unique<SrvManager>();
	mSrvManager->Init(mRenderer.get());

	mResourceManager->Init();

	mRenderer->SetShaderResourceViewManager(
		mSrvManager.get());
	mRenderer->Init();

	mSrvManager->Allocate();

#ifdef _DEBUG
	mImGuiManager = std::make_unique<ImGuiManager>(
		mRenderer.get(), mSrvManager.get());
#endif

	mConsole = std::make_unique<Console>();


	mOffscreenRtv = mRenderer->CreateRenderTargetTexture(
		mWindowManager->GetMainWindow()->GetClientWidth(),
		mWindowManager->GetMainWindow()->GetClientHeight(),
		offscreenClearColor,
		kBufferFormat
	);

	mOffscreenDsv = mRenderer->CreateDepthStencilTexture(
		mWindowManager->GetMainWindow()->GetClientWidth(),
		mWindowManager->GetMainWindow()->GetClientHeight(),
		DXGI_FORMAT_D32_FLOAT
	);

	mPostProcessedRtv = mRenderer->CreateRenderTargetTexture(
		mWindowManager->GetMainWindow()->GetClientWidth(),
		mWindowManager->GetMainWindow()->GetClientHeight(),
		offscreenClearColor,
		kBufferFormat
	);

	mPostProcessedDsv = mRenderer->CreateDepthStencilTexture(
		mWindowManager->GetMainWindow()->GetClientWidth(),
		mWindowManager->GetMainWindow()->GetClientHeight(),
		DXGI_FORMAT_D32_FLOAT
	);

	mOffscreenRenderPassTargets.pRTVs        = &mOffscreenRtv.rtvHandle;
	mOffscreenRenderPassTargets.numRTVs      = 1;
	mOffscreenRenderPassTargets.pDSV         = &mOffscreenDsv.dsvHandle;
	mOffscreenRenderPassTargets.clearColor   = offscreenClearColor;
	mOffscreenRenderPassTargets.clearDepth   = 1.0f;
	mOffscreenRenderPassTargets.clearStencil = 0;
	mOffscreenRenderPassTargets.bClearColor  = true;
	mOffscreenRenderPassTargets.bClearDepth  = true;

	mPostProcessedRenderPassTargets.pRTVs        = &mPostProcessedRtv.rtvHandle;
	mPostProcessedRenderPassTargets.numRTVs      = 1;
	mPostProcessedRenderPassTargets.pDSV         = &mPostProcessedDsv.dsvHandle;
	mPostProcessedRenderPassTargets.clearColor   = offscreenClearColor;
	mPostProcessedRenderPassTargets.clearDepth   = 1.0f;
	mPostProcessedRenderPassTargets.clearStencil = 0;
	mPostProcessedRenderPassTargets.bClearColor  = true;
	mPostProcessedRenderPassTargets.bClearDepth  = true;

	mCopyImagePass = std::make_unique<CopyImagePass>(
		mRenderer->GetDevice(), mSrvManager.get()
	);

	//// モデル
	//modelCommon_ = std::make_unique<ModelCommon>();
	//modelCommon_->Init(renderer_.get());

	// オブジェクト3D
	/*object3DCommon_ = std::make_unique<Object3DCommon>();
	object3DCommon_->Init(renderer_.get());*/

	//// スプライト
	//spriteCommon_ = std::make_unique<SpriteCommon>();
	//spriteCommon_->Init(renderer_.get());

	TexManager::GetInstance()->Init(mRenderer.get(), mSrvManager.get());

	// パーティクル
	mParticleManager = std::make_unique<ParticleManager>();
	mParticleManager->Init(mRenderer.get(), mSrvManager.get());

	// ライン
	mLineCommon = std::make_unique<LineCommon>();
	mLineCommon->Init(mRenderer.get());

	Debug::Init(mLineCommon.get());

	//-------------------------------------------------------------------------
	// コマンドのリセット
	//-------------------------------------------------------------------------
	HRESULT hr = mRenderer->GetCommandAllocator()->Reset();
	assert(SUCCEEDED(hr));
	hr = mRenderer->GetCommandList()->Reset(
		mRenderer->GetCommandAllocator(),
		nullptr
	);
	assert(SUCCEEDED(hr));

	mTime = std::make_unique<EngineTimer>();

	//-------------------------------------------------------------------------
	// すべての初期化が完了
	//-------------------------------------------------------------------------

	Console::SubmitCommand("neofetch");

	mEntityLoader = std::make_unique<EntityLoader>();

	// シーンマネージャ/ファクトリーの作成
	mSceneFactory = std::make_unique<SceneFactory>();
	mSceneManager = std::make_shared<SceneManager>(*mSceneFactory);
	// ゲームシーンを登録
	mSceneFactory->RegisterScene<GameScene>("GameScene");
	mSceneFactory->RegisterScene<EmptyScene>("EmptyScene");
	// シーンの初期化
	mSceneManager->ChangeScene("GameScene");

	// エディターの初期化
	CheckEditorMode();

	hr = mRenderer->GetCommandList()->Close();
	assert(SUCCEEDED(hr));

	//-----------------------------------------------------------------------------
	// Purpose: 新エンジン
	//-----------------------------------------------------------------------------
	mSubsystems.emplace_back(std::make_unique<ConsoleSystem>());

	for (auto& subsystem : mSubsystems) {
		if (!subsystem->Init()) {
			UASSERT(false && "Failed to initialize subsystem");
		}
	}

	// テスト: 成功
	if (auto* console = ServiceLocator::Get<IConsole>()) {
		console->Msg("Console\n\nHEELLLOOOOO!!!!!"); // コンソールの初期化後に更新を呼び出す
	}

	return true;
}

void Engine::Update() {
	//-----------------------------------------------------------------------------
	// Purpose: 旧エンジン
	//-----------------------------------------------------------------------------
	mTime->StartFrame();
#ifdef _DEBUG
	ImGuiManager::NewFrame();
	ImGuizmo::BeginFrame();
#endif

	// 前のフレームとeditorModeが違う場合はエディターモードを切り替える
	static bool bPrevEditorMode = mIsEditorMode;
	if (bPrevEditorMode != mIsEditorMode) {
		CheckEditorMode();
		bPrevEditorMode = mIsEditorMode;
	}

	/* ----------- 更新処理 ---------- */

	if (IsEditorMode()) {
#ifdef _DEBUG
		{
			// メニューバーを少し高くする
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
			                    ImVec2(
				                    0.0f, kTitleBarH * 0.5f -
				                    ImGui::GetFontSize() * 0.5f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
			                    ImVec2(0.0f, kTitleBarH));
			if (ImGui::BeginMainMenuBar()) {
				ImGui::PopStyleVar(2); // メニューバーのスタイルを元に戻す
				// アイコンメニュー
				ImGui::PushStyleColor(ImGuiCol_Text,
				                      ImVec4(0.13f, 0.5f, 1.0f, 1.0f));

				if (ImGuiWidgets::BeginMainMenu(
					StrUtil::ConvertToUtf8(kIconArrowForward).c_str())) {
					ImGui::PopStyleColor();
					if (ImGui::MenuItemEx(
						("About " + std::string(ENGINE_NAME)).c_str(),
						nullptr)) {
					}
					ImGui::EndMenu();
				} else {
					ImGui::PopStyleColor();
				}

				if (ImGuiWidgets::BeginMainMenu("File")) {
					if (ImGui::MenuItemEx(
						"Save", StrUtil::ConvertToUtf8(kIconSave).c_str())) {
					}

					if (ImGui::MenuItemEx("Save As",
					                      StrUtil::ConvertToUtf8(kIconSaveAs).
					                      c_str())) {
					}

					ImGui::Separator();

					if (ImGui::MenuItemEx("Import",
					                      StrUtil::ConvertToUtf8(kIconDownload)
					                      .
					                      c_str())) {
						BaseScene* currentScene = mSceneManager->
						                          GetCurrentScene().
						                          get();
						if (currentScene) {
							char szFile[MAX_PATH] = ""; // 初期ファイル名は空

							OPENFILENAMEA ofn;
							ZeroMemory(&ofn, sizeof(ofn)); // 構造体をゼロ初期化
							ofn.lStructSize = sizeof(OPENFILENAMEA);

							HWND hwndOwner = nullptr;
							if (OldWindowManager::GetMainWindow()) {
								hwndOwner = OldWindowManager::GetMainWindow()->
									GetWindowHandle();
							}
							ofn.hwndOwner   = hwndOwner;
							ofn.lpstrFilter =
								"Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
							ofn.lpstrFile = szFile;
							ofn.nMaxFile = MAX_PATH;
							ofn.lpstrTitle = "Import Scene From";
							ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST |
								OFN_NOCHANGEDIR; // ファイル/パス存在確認、カレントディレクトリ変更なし
							ofn.lpstrDefExt = "scene";

							if (GetOpenFileNameA(&ofn)) {
								mLoadFilePath = ofn.lpstrFile;
							}
						} else {
							Console::Print(
								"Import failed: No active scene found.");
						}
					}

					if (ImGui::MenuItemEx("Export",
					                      StrUtil::ConvertToUtf8(kIconUpload).
					                      c_str())) {
						BaseScene* currentScene = mSceneManager->
						                          GetCurrentScene().
						                          get();
						if (currentScene) {
							char szFile[MAX_PATH] = "scene.json"; // デフォルトのファイル名

							OPENFILENAMEA ofn;
							ZeroMemory(&ofn, sizeof(ofn)); // 構造体をゼロ初期化
							ofn.lStructSize = sizeof(OPENFILENAMEA);

							HWND hwndOwner = nullptr;
							if (OldWindowManager::GetMainWindow()) {
								hwndOwner = OldWindowManager::GetMainWindow()->
									GetWindowHandle();
							}
							ofn.hwndOwner   = hwndOwner;
							ofn.lpstrFilter =
								"Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
							ofn.lpstrFile = szFile;
							ofn.nMaxFile = MAX_PATH;
							ofn.lpstrTitle = "Export Scene As";
							ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
							// 上書き確認、カレントディレクトリ変更なし
							ofn.lpstrDefExt = "scene";

							if (GetSaveFileNameA(&ofn)) {
								std::string filePath = ofn.lpstrFile;
								mEntityLoader->
									SaveScene(filePath, currentScene);
								Console::Print(
									"Scene exported to: " + filePath);
							}
						} else {
							Console::Print(
								"Export failed: No active scene found.");
						}
					}

					ImGui::Separator();

					if (ImGui::MenuItemEx(
							"Exit",
							StrUtil::ConvertToUtf8(kIconPower).c_str())
					) {
						Console::SubmitCommand("quit");
					}
					ImGui::EndMenu();
				}

				if (ImGuiWidgets::BeginMainMenu("Edit")) {
					ImGui::Separator();
					if (ImGuiWidgets::MenuItemWithIcon(
						StrUtil::ConvertToUtf8(kIconSettings).c_str(),
						"Settings")) {
					}

					ImGui::EndMenu();
				}
				ImGui::EndMainMenuBar();
			}

			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
			                    ImVec2(0.0f, 0.0f));

			constexpr ImGuiDockNodeFlags dockSpaceFlags =
				ImGuiDockNodeFlags_PassthruCentralNode;
			constexpr ImGuiWindowFlags windowFlags =
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoCollapse |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoBringToFrontOnFocus |
				ImGuiWindowFlags_NoNavFocus |
				ImGuiWindowFlags_NoBackground;

			ImGui::Begin("DockSpace", nullptr, windowFlags);

			ImGui::PopStyleVar(3);

			const ImGuiIO& io = ImGui::GetIO();
			if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
				const ImGuiID dockSpaceId = ImGui::GetID("MyDockSpace");
				ImGui::DockSpace(dockSpaceId, ImVec2(0.0f, 0.0f),
				                 dockSpaceFlags);
			}

			ImGui::End();
		}

		static auto tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		static auto bg   = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

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
		const auto ptr   = mPostProcessedRtv.srvHandleGPU.ptr;

		static int prevW = 0, prevH = 0;
		int        w     = static_cast<int>(avail.x);
		int        h     = static_cast<int>(avail.y);
		if ((w != prevW || h != prevH) && w > 0 && h > 0) {
			prevW = w;
			prevH = h;
		}

		if (ptr) {
			// リソースからテクスチャの幅と高さを取得
			auto        desc      = mPostProcessedRtv.rtv->GetDesc();
			const float texWidth  = static_cast<float>(desc.Width);
			const float texHeight = static_cast<float>(desc.Height);

			const float availAspect = avail.x / avail.y;
			const float texAspect   = texWidth / texHeight;

			ImVec2 drawSize = avail;
			if (availAspect > texAspect) {
				drawSize.x = avail.y * texAspect;
			} else {
				drawSize.y = avail.x / texAspect;
			}

			float titleBarHeight = ImGui::GetCurrentWindow()->
				TitleBarHeight;

			ImVec2 offset = {
				(avail.x - drawSize.x) * 0.5f,
				(avail.y - drawSize.y) * 0.5f + titleBarHeight
			};
			ImGui::SetCursorPos(offset);

			ImVec2 viewportScreenPos = ImGui::GetCursorScreenPos();

			ImGui::ImageWithBg(
				ptr, // postProcessedRTV_のSRVを直接使用
				drawSize,
				ImVec2(0, 0), ImVec2(1, 1),
				bg, tint
			);

			mViewportLT   = {viewportScreenPos.x, viewportScreenPos.y};
			mViewportSize = {drawSize.x, drawSize.y};
		}
		ImGui::End();
		ImGui::PopStyleVar();
#endif

		if (mEditor) {
			mEditor->Update(EngineTimer::GetDeltaTime());
		}

		mCopyImagePass->Update(EngineTimer::GetDeltaTime());
	} else {
		mSceneManager->Update(EngineTimer::GetScaledDeltaTime());
		mViewportLT   = Vec2::zero;
		mViewportSize = {
			static_cast<float>(mWindowManager->GetMainWindow()->
			                                   GetClientWidth()),
			static_cast<float>(mWindowManager->GetMainWindow()->
			                                   GetClientHeight())
		};
	}

	InputSystem::Update();

#ifdef _DEBUG
	Console::Update();
	DebugHud::Update();
#endif

	mOffscreenRenderPassTargets.bClearColor =
		ConVarManager::GetConVar("r_clear")->GetValueAsBool();

#ifdef _DEBUG
	Debug::Update();
#endif
	CameraManager::Update(EngineTimer::GetDeltaTime());

	//-------------------------------------------------------------------------
	// --- PreRender↓ ---
	mRenderer->PreRender();
	//-------------------------------------------------------------------------

	mRenderer->SetViewportAndScissor(
		static_cast<uint32_t>(mOffscreenRtv.rtv->GetDesc().Width),
		mOffscreenRtv.rtv->GetDesc().Height
	);
	mRenderer->BeginRenderPass(mOffscreenRenderPassTargets);
	if (IsEditorMode()) {
		mLineCommon->Render();
		Debug::Draw();
		if (mEditor) {
			mEditor->Render();
		}
	} else {
		mSceneManager->Render();
	}

	// バリアを設定
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource   = mOffscreenRtv.rtv.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	mRenderer->GetCommandList()->ResourceBarrier(1, &barrier);

	if (IsEditorMode()) {
		// エディターモード時はポストプロセス用RTVに描画
		mRenderer->BeginRenderPass(mPostProcessedRenderPassTargets);
		mRenderer->SetViewportAndScissor(
			static_cast<uint32_t>(mOffscreenRtv.rtv->GetDesc().Width),
			mOffscreenRtv.rtv->GetDesc().Height
		);
		mCopyImagePass->Execute(
			mRenderer->GetCommandList(),
			mOffscreenRtv.rtv.Get(),
			mPostProcessedRtv.rtvHandle
		);

		// バリアを設定
		D3D12_RESOURCE_BARRIER postBarrier = {};
		postBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		postBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		postBarrier.Transition.pResource = mPostProcessedRtv.rtv.Get();
		postBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		postBarrier.Transition.StateAfter =
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		mRenderer->GetCommandList()->ResourceBarrier(1, &postBarrier);

		// CopyImagePass実行後にSRVを再作成して最新の内容を反映
		if (mSrvManager) {
			// SRVを再作成
			mSrvManager->CreateSRVForTexture2D(
				mPostProcessedRtv.srvIndex,
				mPostProcessedRtv.rtv.Get(),
				mPostProcessedRtv.rtv->GetDesc().Format,
				1
			);

			// GPUハンドルを再取得
			mPostProcessedRtv.srvHandleGPU = mSrvManager->
				GetGPUDescriptorHandle(
					mPostProcessedRtv.srvIndex);
		}
	} else {
		// ゲーム時はスワップチェーンに直接描画
		mRenderer->BeginSwapChainRenderPass();
		mRenderer->SetViewportAndScissor(
			OldWindowManager::GetMainWindow()->GetClientWidth(),
			OldWindowManager::GetMainWindow()->GetClientHeight()
		);
		mCopyImagePass->Execute(
			mRenderer->GetCommandList(),
			mOffscreenRtv.rtv.Get(),
			mRenderer->GetSwapChainRenderTargetView()
		);
	}

	//------------------------------------------------------------------------
	// --- PostRender↓ ---
	if (IsEditorMode()) {
		mRenderer->BeginSwapChainRenderPass();
	}

#ifdef _DEBUG
	mImGuiManager->EndFrame();
#endif

	// バリアを元に戻す
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
	mRenderer->GetCommandList()->ResourceBarrier(1, &barrier);

	if (IsEditorMode()) {
		// postProcessedRTV_ のバリアを戻す
		D3D12_RESOURCE_BARRIER postBarrier = {};
		postBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		postBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		postBarrier.Transition.pResource = mPostProcessedRtv.rtv.Get();
		postBarrier.Transition.StateBefore =
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		postBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		mRenderer->GetCommandList()->ResourceBarrier(1, &postBarrier);
	}

	if (mLoadFilePath) {
		BaseScene* currentScene = mSceneManager->
		                          GetCurrentScene().
		                          get();
		ResourceManager* resourceManager =
			GetResourceManager();
		if (resourceManager) {
			mEntityLoader->LoadScene(
				mLoadFilePath.value(), currentScene,
				resourceManager);
			Console::Print(
				"Scene imported from: " + mLoadFilePath.value());
		} else {
			Console::Print(
				"Import failed: ResourceManager not found.");
		}

		mLoadFilePath.reset(); // ロード後はリセット
	}

	mRenderer->PostRender();

	mTime->EndFrame();

	//-----------------------------------------------------------------------------
	// Purpose: 新エンジン
	//-----------------------------------------------------------------------------
}

void Engine::Shutdown() const {
	//-----------------------------------------------------------------------------
	// Purpose: 旧エンジン
	//-----------------------------------------------------------------------------
	mRenderer->WaitPreviousFrame();

	Debug::Shutdown();

	mCopyImagePass->Shutdown();

	TexManager::GetInstance()->Shutdown();

	mParticleManager->Shutdown();
	mParticleManager.reset();

	mRenderer->Shutdown();

#ifdef _DEBUG
	if (mImGuiManager) {
		mImGuiManager->Shutdown();
	}
#endif
	mResourceManager->Shutdown();
	mResourceManager.reset();

	//-----------------------------------------------------------------------------
	// Purpose: 新エンジン
	//-----------------------------------------------------------------------------
	// 登録されたサブシステムをシャットダウン
	for (auto& subsystem : mSubsystems) {
		if (subsystem) {
			subsystem->Shutdown();
		}
	}
}

void Engine::OnResize(const uint32_t width, const uint32_t height) {
	if (width == 0 || height == 0) {
		return;
	}

	// GPUの処理が終わるまで待つ
	mRenderer->Flush();

	mRenderer->Resize(width, height);

	mPostProcessedRtv.rtv.Reset();
	mPostProcessedDsv.dsv.Reset();

	mOffscreenRtv = {};
	mOffscreenDsv = {};

	mPostProcessedRtv = {};
	mPostProcessedDsv = {};

	mOffscreenRtv = mRenderer->CreateRenderTargetTexture(
		width, height,
		offscreenClearColor,
		kBufferFormat
	);
	mOffscreenDsv = mRenderer->CreateDepthStencilTexture(
		width, height,
		DXGI_FORMAT_D32_FLOAT
	);

	mPostProcessedRtv = mRenderer->CreateRenderTargetTexture(
		width, height,
		offscreenClearColor,
		kBufferFormat
	);

	mPostProcessedDsv = mRenderer->CreateDepthStencilTexture(
		width, height,
		DXGI_FORMAT_D32_FLOAT
	);

	mOffscreenRenderPassTargets.pRTVs   = &mOffscreenRtv.rtvHandle;
	mOffscreenRenderPassTargets.numRTVs = 1;
	mOffscreenRenderPassTargets.pDSV    = &mOffscreenDsv.dsvHandle;

	mPostProcessedRenderPassTargets.pRTVs   = &mPostProcessedRtv.rtvHandle;
	mPostProcessedRenderPassTargets.numRTVs = 1;
	mPostProcessedRenderPassTargets.pDSV    = &mPostProcessedDsv.dsvHandle;
}

void Engine::ResizeOffscreenRenderTextures(
	const uint32_t width,
	const uint32_t height
) {
	if (width == 0 || height == 0) {
		return;
	}

	// GPUの処理が終わるまで待つ
	mRenderer->Flush();

	mRenderer->ResetOffscreenRenderTextures();

	mOffscreenRtv.rtv.Reset();
	mOffscreenDsv.dsv.Reset();

	mPostProcessedRtv.rtv.Reset();
	mPostProcessedDsv.dsv.Reset();

	mOffscreenRtv = {};
	mOffscreenDsv = {};

	mPostProcessedRtv = {};
	mPostProcessedDsv = {};

	mOffscreenRtv = mRenderer->CreateRenderTargetTexture(
		width, height,
		offscreenClearColor,
		kBufferFormat
	);
	mOffscreenDsv = mRenderer->CreateDepthStencilTexture(
		width, height,
		DXGI_FORMAT_D32_FLOAT
	);

	mPostProcessedRtv = mRenderer->CreateRenderTargetTexture(
		width, height,
		offscreenClearColor,
		kBufferFormat
	);

	mPostProcessedDsv = mRenderer->CreateDepthStencilTexture(
		width, height,
		DXGI_FORMAT_D32_FLOAT
	);

	mOffscreenRenderPassTargets.pRTVs   = &mOffscreenRtv.rtvHandle;
	mOffscreenRenderPassTargets.numRTVs = 1;
	mOffscreenRenderPassTargets.pDSV    = &mOffscreenDsv.dsvHandle;

	mPostProcessedRenderPassTargets.pRTVs   = &mPostProcessedRtv.rtvHandle;
	mPostProcessedRenderPassTargets.numRTVs = 1;
	mPostProcessedRenderPassTargets.pDSV    = &mPostProcessedDsv.dsvHandle;
}

void Engine::RegisterConsoleCommandsAndVariables() {
	// コンソールコマンドを登録
	ConCommand::RegisterCommand("exit", Quit, "Exit the engine.");
	ConCommand::RegisterCommand("quit", Quit, "Exit the engine.");

	ConCommand::RegisterCommand(
		"toggleeditor",
		[]([[maybe_unused]] const std::vector<std::string>& args) {
			mIsEditorMode = !mIsEditorMode;
			Console::Print(
				"Editor mode is now " + std::to_string(mIsEditorMode) + "\n",
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
	ConVarManager::RegisterConVar<int>("cl_fpsmax", kFpsMax,
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
	Console::SubmitCommand("bind e +scale", true);
	Console::SubmitCommand("bind tab +toggleGizmo", true);
}

void Engine::Quit([[maybe_unused]] const std::vector<std::string>& args) {
	mWishShutdown = true;
}

void Engine::CheckEditorMode() {
	if (mIsEditorMode) {
		mEditor = std::make_unique<Editor>(mSceneManager.get());
	} else {
		mEditor.reset();
	}
}

bool                             Engine::mWishShutdown    = false;
std::unique_ptr<D3D12>           Engine::mRenderer        = nullptr;
std::unique_ptr<ResourceManager> Engine::mResourceManager = nullptr;
std::unique_ptr<ParticleManager> Engine::mParticleManager = nullptr;
std::unique_ptr<SrvManager>      Engine::mSrvManager      = nullptr;
std::shared_ptr<SceneManager>    Engine::mSceneManager    = nullptr;

Vec2 Engine::mViewportLT   = Vec2::zero;
Vec2 Engine::mViewportSize = Vec2::zero;

#ifdef _DEBUG
bool Engine::mIsEditorMode = true;
#else
bool Engine::mIsEditorMode = false;
#endif
