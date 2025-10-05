#include <pch.h>

#ifdef _DEBUG
#include <imgui_internal.h>
// ImGuizmoのインクルードはImGuiより後
#include <ImGuizmo.h>
#endif

#include <engine/Engine.h>
#include <engine/Camera/CameraManager.h>
#include <engine/Debug/Debug.h>
#include <engine/Debug/DebugHud.h>
#include <engine/ImGui/Icons.h>
#include <engine/ImGui/ImGuiWidgets.h>
#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/ConCommand.h>
#include <engine/OldConsole/ConVarManager.h>
#include <engine/postprocess/PPBloom.h>
#include <engine/postprocess/PPChromaticAberration.h>
#include <engine/postprocess/PPRadialBlur.h>
#include <engine/postprocess/PPVignette.h>
#include <engine/renderer/SrvManager.h>
#include <engine/subsystem/console/ConsoleSystem.h>
#include <engine/subsystem/interface/ServiceLocator.h>
#include <engine/subsystem/time/TimeSystem.h>
#include <engine/TextureManager/TexManager.h>
#include <engine/Window/MainWindow.h>
#include <engine/Window/WindowsUtils.h>

#include "game/scene/EmptyScene.h"
#include "game/scene/GameScene.h"

#include "engine/subsystem/console/concommand/UnnamedConCommand.h"
#include "engine/subsystem/console/concommand/UnnamedConVar.h"
#include "engine/subsystem/console/concommand/base/UnnamedConCommandBase.h"

constexpr Vec4 offscreenClearColor = Vec4(0.025f, 0.025f, 0.025f, 1.0f);

namespace Unnamed {
	Engine::Engine() = default;

	Engine::~Engine() = default;

	bool Engine::Init() {
		//---------------------------------------------------------------------
		// Purpose: 新エンジン
		//---------------------------------------------------------------------
		mSubsystems.emplace_back(std::make_unique<ConsoleSystem>());
		mSubsystems.emplace_back(std::make_unique<TimeSystem>());

		for (auto& subsystem : mSubsystems) {
			if (subsystem->Init()) {
				std::string name = std::string(subsystem->GetName());
				SpecialMsg(
					LogLevel::Success,
					"Engine",
					"Subsystem initialized: {}",
					subsystem->GetName()
				);
			} else {
				UASSERT(false && "Failed to initialize subsystem");
			}
		}

		mConsoleSystem = ServiceLocator::Get<ConsoleSystem>();
		mTimeSystem    = ServiceLocator::Get<TimeSystem>();

		static UnnamedConVar cv_test(
			"test",
			Math::pi,
			FCVAR::NONE
		);

		static UnnamedConVar<std::string> cv_test1(
			"test1",
			"くぁｗせｄｒｆｔｇｙふじこｌｐ；＠：「」",
			FCVAR::NONE
		);

		static UnnamedConVar cv_test2(
			"test2",
			false,
			FCVAR::NONE
		);
		static UnnamedConVar cv_test3(
			"test3",
			65535,
			FCVAR::NONE
		);
		static UnnamedConVar cv_test4(
			"test4",
			DBL_MAX,
			FCVAR::NONE
		);

		static UnnamedConCommand con_test(
			"なんかするコマンド",
			[]([[maybe_unused]] std::vector<std::string> args) {
				Msg("Test", "へっへっへっへっへっへ");
				return true;
			},
			"なんかします。",
			FCVAR::NONE
		);

		mConsoleSystem->Test();
		mConsoleSystem->ExecuteCommand(
			"なんかするコマンド;なんかするコマンド;なんかするコマンド;なんかするコマンド;なんかするコマンド;なんかするコマンド;なんかするコマンド;なんかするコマンド;;;;;;;;;;;;;;;;;;"
		);

		//---------------------------------------------------------------------
		// Purpose: 旧エンジン
		//---------------------------------------------------------------------
#ifdef _DEBUG
		ConVarManager::RegisterConVar<bool>("verbose", true,
		                                    "Enable verbose logging");
#else
		ConVarManager::RegisterConVar<bool>("verbose", false,
		                                    "Enable verbose logging");
#endif
		Msg("CommandLine", "command line arguments:\n{}",
		    StrUtil::ToString(GetCommandLineW()));
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
			Fatal("Engine", "Failed to create main window.");
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
			ConVarManager::GetConVar("launchargs")->GetValueAsString()
		);

		mResourceManager = std::make_unique<ResourceManager>(mRenderer.get());

		mSrvManager = std::make_unique<SrvManager>();
		mSrvManager->Init(mRenderer.get());

		mResourceManager->Init();

		mRenderer->SetShaderResourceViewManager(
			mSrvManager.get());
		mRenderer->Init();

		mSrvManager->AllocateForTexture2D();

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

		mPingRtv[0] = mRenderer->CreateRenderTargetTexture(
			mWindowManager->GetMainWindow()->GetClientWidth(),
			mWindowManager->GetMainWindow()->GetClientHeight(),
			offscreenClearColor,
			kBufferFormat
		);

		mPingRtv[1] = mRenderer->CreateRenderTargetTexture(
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

		mPostProcessedRenderPassTargets.pRTVs = &mPostProcessedRtv.rtvHandle;
		mPostProcessedRenderPassTargets.numRTVs = 1;
		mPostProcessedRenderPassTargets.pDSV = &mPostProcessedDsv.dsvHandle;
		mPostProcessedRenderPassTargets.clearColor = offscreenClearColor;
		mPostProcessedRenderPassTargets.clearDepth = 1.0f;
		mPostProcessedRenderPassTargets.clearStencil = 0;
		mPostProcessedRenderPassTargets.bClearColor = true;
		mPostProcessedRenderPassTargets.bClearDepth = true;

		mPostChain.emplace_back(
			std::make_unique<CopyImagePass>(
				mRenderer->GetDevice(),
				mSrvManager.get()
			)
		);

		mPostChain.emplace_back(
			std::make_unique<PPBloom>(
				mRenderer->GetDevice(),
				mSrvManager.get()
			)
		);

		mPostChain.emplace_back(
			std::make_unique<PPBloom>(
				mRenderer->GetDevice(),
				mSrvManager.get()
			)
		);

		reinterpret_cast<PPBloom*>(
			mPostChain.back().get()
		)->SetStrength(0.0f); // 奇数個のポストプロセスは適応されない不具合があるのでダミー

		mPostChain.emplace_back(
			std::make_unique<PPVignette>(
				mRenderer->GetDevice(),
				mSrvManager.get()
			)
		);

		mPostChain.emplace_back(
			std::make_unique<PPChromaticAberration>(
				mRenderer->GetDevice(),
				mSrvManager.get()
			)
		);

		mPostChain.emplace_back(
			std::make_unique<PPRadialBlur>(
				mRenderer->GetDevice(),
				mSrvManager.get()
			)
		);

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

		//-----------------------------------------------------------------------------
		// エディターの初期化
		CheckEditorMode();

		hr = mRenderer->GetCommandList()->Close();
		assert(SUCCEEDED(hr));

		return true;
	}

	void Engine::Update() {
		//-----------------------------------------------------------------------------
		// Purpose: 旧エンジン
		//-----------------------------------------------------------------------------

		// ウィンドウが閉じられた場合は終了 TODO: きたないので直そう
		if (mWishShutdown) {
			DestroyWindow(mWindowManager->GetMainWindow()->GetWindowHandle());
		}

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
						ImGui::BeginDisabled();
						if (ImGui::MenuItemEx(
							"Save",
							StrUtil::ConvertToUtf8(kIconSave).c_str())) {
						}

						if (ImGui::MenuItemEx("Save As",
						                      StrUtil::ConvertToUtf8(
							                      kIconSaveAs).
						                      c_str())) {
						}
						ImGui::EndDisabled();

						ImGui::Separator();

						if (ImGui::MenuItemEx("Import",
						                      StrUtil::ConvertToUtf8(
							                      kIconDownload)
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
									hwndOwner =
										OldWindowManager::GetMainWindow()->
										GetWindowHandle();
								}
								ofn.hwndOwner   = hwndOwner;
								ofn.lpstrFilter =
									"Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
								ofn.lpstrFile  = szFile;
								ofn.nMaxFile   = MAX_PATH;
								ofn.lpstrTitle = "Import Scene From";
								ofn.Flags      = OFN_PATHMUSTEXIST |
									OFN_FILEMUSTEXIST |
									OFN_NOCHANGEDIR;
								// ファイル/パス存在確認、カレントディレクトリ変更なし
								ofn.lpstrDefExt = "scene";

								if (GetOpenFileNameA(&ofn)) {
									mLoadFilePath = ofn.lpstrFile;
								}
							} else {
								Error(
									"ImportScene",
									"Import failed: No active scene found."
								);
							}
						}

						if (ImGui::MenuItemEx("Export",
						                      StrUtil::ConvertToUtf8(
							                      kIconUpload).
						                      c_str())) {
							BaseScene* currentScene = mSceneManager->
								GetCurrentScene().
								get();
							if (currentScene) {
								char szFile[MAX_PATH] = "scene.json";
								// デフォルトのファイル名

								OPENFILENAMEA ofn;
								ZeroMemory(&ofn, sizeof(ofn)); // 構造体をゼロ初期化
								ofn.lStructSize = sizeof(OPENFILENAMEA);

								HWND hwndOwner = nullptr;
								if (OldWindowManager::GetMainWindow()) {
									hwndOwner =
										OldWindowManager::GetMainWindow()->
										GetWindowHandle();
								}
								ofn.hwndOwner   = hwndOwner;
								ofn.lpstrFilter =
									"Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
								ofn.lpstrFile  = szFile;
								ofn.nMaxFile   = MAX_PATH;
								ofn.lpstrTitle = "Export Scene As";
								ofn.Flags      = OFN_OVERWRITEPROMPT |
									OFN_NOCHANGEDIR;
								// 上書き確認、カレントディレクトリ変更なし
								ofn.lpstrDefExt = "scene";

								if (GetSaveFileNameA(&ofn)) {
									std::string filePath = ofn.lpstrFile;
									mEntityLoader->
										SaveScene(filePath, currentScene);
									SpecialMsg(
										LogLevel::Success,
										"SceneExport",
										"Scene exported to: {}",
										filePath
									);
								}
							} else {
								Error(
									"SceneExport",
									"Export failed: No active scene found."
								);
							}
						}

						ImGui::Separator();

						ImGui::BeginDisabled();
						if (ImGui::MenuItemEx(
								"Exit",
								StrUtil::ConvertToUtf8(kIconPower).c_str())
						) {
							Console::SubmitCommand("quit");
						}
						ImGui::EndDisabled();
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
			const auto ptr   = mPingRtv[mPingIndex].srvHandleGPU.ptr;

			static int prevW = 0, prevH = 0;
			int        w     = static_cast<int>(avail.x);
			int        h     = static_cast<int>(avail.y);
			if ((w != prevW || h != prevH) && w > 0 && h > 0) {
				prevW = w;
				prevH = h;
			}

			if (ptr) {
				// リソースからテクスチャの幅と高さを取得
				auto        desc      = mPingRtv[mPingIndex].rtv->GetDesc();
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
				mEditor->Update(mTimeSystem->GetGameTime()->DeltaTime<float>());
			}

#ifdef _DEBUG
			ImGui::Begin("Post Process");
			for (int i = 0; i < mPostChain.size(); ++i) {
				if (i == 2) {
					continue;
				}
				auto& postProcess = mPostChain[i];
				if (postProcess) {
					postProcess->Update(
						mTimeSystem->GetGameTime()->DeltaTime<float>());
				}
			}
			ImGui::End();
#endif
		} else {
			mSceneManager->Update(
				mTimeSystem->GetGameTime()->ScaledDeltaTime<float>());
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
		DebugHud::Update(mTimeSystem->GetGameTime()->ScaledDeltaTime<float>());
#endif


#ifdef _DEBUG
		Debug::Update();
#endif
		CameraManager::Update(
			mTimeSystem->GetGameTime()->ScaledDeltaTime<float>());

		mOffscreenRenderPassTargets.bClearColor =
			ConVarManager::GetConVar("r_clear")->GetValueAsBool();
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
			if (mEditor) {
				mEditor->Render();
			}
		} else {
			mSceneManager->Render();
		}

#ifdef _DEBUG
		mLineCommon->Render();
		Debug::Draw();
#endif

		// 先にバリアを設定
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource   = mOffscreenRtv.rtv.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter  =
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		mRenderer->GetCommandList()->ResourceBarrier(1, &barrier);

		auto* radialBlur = dynamic_cast<PPRadialBlur*>(mPostChain[5].get());
		if (radialBlur) {
			radialBlur->SetBlurStrength(blurStrength);
		}

		// ポストプロセスを適用するRTV
		ID3D12Resource* postProcessTarget = mOffscreenRtv.rtv.Get();
		for (auto& pass : mPostChain) {
			const uint32_t w = static_cast<uint32_t>(
				postProcessTarget->GetDesc().
				                   Width
			);
			const uint32_t h = static_cast<uint32_t>(
				postProcessTarget->GetDesc().Height
			);

			const uint32_t next = mPingIndex ^ 1; // 次のインデックス
			auto&          dest = mPingRtv[next];

			if (IsEditorMode()) {
				mRenderer->SetViewportAndScissor(
					static_cast<uint32_t>(mOffscreenRtv.rtv->GetDesc().Width),
					mOffscreenRtv.rtv->GetDesc().Height
				);
			} else {
				//mRenderer->BeginSwapChainRenderPass();
				mRenderer->SetViewportAndScissor(
					OldWindowManager::GetMainWindow()->GetClientWidth(),
					OldWindowManager::GetMainWindow()->GetClientHeight()
				);
			}

			const bool isLastPass = (&pass == &mPostChain.back());

			D3D12_CPU_DESCRIPTOR_HANDLE outRtvHandle{};

			if (isLastPass && !IsEditorMode()) {
				// ゲーム
				if (!bSwapchainPassBegun) {
					mRenderer->BeginSwapChainRenderPass();
					mRenderer->SetViewportAndScissor(
						OldWindowManager::GetMainWindow()->GetClientWidth(),
						OldWindowManager::GetMainWindow()->GetClientHeight());
					bSwapchainPassBegun = true;
				}

				outRtvHandle = mRenderer->GetSwapChainRenderTargetView();
			} else {
				mRenderer->BeginRenderPass(
					{
						&dest.rtvHandle,
						1,
						&mPostProcessedDsv.dsvHandle,
						offscreenClearColor,
						1.0f,
						0,
						true,
						true
					}
				);
				mRenderer->SetViewportAndScissor(
					static_cast<uint32_t>(mOffscreenRtv.rtv->GetDesc().Width),
					mOffscreenRtv.rtv->GetDesc().Height
				);
				outRtvHandle = dest.rtvHandle;
			}


			if (IsEditorMode()) {
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

				PostProcessContext context = {};
				context.commandList        = mRenderer->GetCommandList();
				context.inputTexture       = postProcessTarget;
				context.outRtv             = dest.rtvHandle;
				context.width              = w;
				context.height             = h;

				pass->Execute(context);
			} else {
				PostProcessContext context = {};
				context.commandList        = mRenderer->GetCommandList();
				context.inputTexture       = postProcessTarget;
				context.outRtv             = outRtvHandle;
				context.width              = OldWindowManager::GetMainWindow()->
					GetClientWidth();
				context.height = OldWindowManager::GetMainWindow()->
					GetClientHeight();

				pass->Execute(context);
			}

			postProcessTarget = dest.rtv.Get();
			mPingIndex        = next;
		}

		//------------------------------------------------------------------------
		// --- PostRender↓ ---
		if (IsEditorMode()) {
			mRenderer->BeginSwapChainRenderPass();
		}

		//-----------------------------------------------------------------------------
		// Purpose: 新エンジン
		//-----------------------------------------------------------------------------

		for (auto& subsystem : mSubsystems) {
			subsystem->Update(mTimeSystem->GetGameTime()->DeltaTime<float>());
		}

		for (auto& subsystem : mSubsystems) {
			subsystem->Render();
		}

#ifdef _DEBUG
		mImGuiManager->EndFrame();
#endif

		// バリアを元に戻す
		barrier.Transition.StateBefore =
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		mRenderer->GetCommandList()->ResourceBarrier(1, &barrier);

		if (IsEditorMode()) {
			// postProcessedRTV_ のバリアを戻す
			D3D12_RESOURCE_BARRIER postBarrier = {};
			postBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			postBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			postBarrier.Transition.pResource = mPostProcessedRtv.rtv.Get();
			postBarrier.Transition.StateBefore =
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			postBarrier.Transition.StateAfter =
				D3D12_RESOURCE_STATE_RENDER_TARGET;
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
				Msg(
					"Engine",
					"Scene imported from: {}",
					mLoadFilePath.value()
				);
			}

			mLoadFilePath.reset(); // ロード後はリセット
		}

		mRenderer->PostRender();

		mTimeSystem->EndFrame();
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

		SpecialMsg(
			LogLevel::Success,
			"Engine",
			"アリーヴェ帰ルチ! (さよナランチャ"
		);

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

	std::shared_ptr<BaseScene> Engine::GetCurrentScene() {
		if (GetSceneManager()) {
			return GetSceneManager()->GetCurrentScene();
		}
		UASSERT(false && "SceneManager is not initialized.");
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

		mOffscreenRtv = mRenderer->CreateRenderTargetTexture(
			width, height,
			offscreenClearColor,
			mOffscreenRtv.srvIndex,
			kBufferFormat
		);
		mOffscreenDsv = mRenderer->CreateDepthStencilTexture(
			width, height,
			mOffscreenDsv.srvIndex,
			DXGI_FORMAT_D32_FLOAT
		);

		mPingRtv[0] = mRenderer->CreateRenderTargetTexture(
			width, height,
			offscreenClearColor,
			mPingRtv[0].srvIndex,
			kBufferFormat
		);

		mPingRtv[1] = mRenderer->CreateRenderTargetTexture(
			width, height,
			offscreenClearColor,
			mPingRtv[1].srvIndex,
			kBufferFormat
		);

		mPostProcessedRtv = mRenderer->CreateRenderTargetTexture(
			width, height,
			offscreenClearColor,
			mPostProcessedRtv.srvIndex,
			kBufferFormat
		);

		mPostProcessedDsv = mRenderer->CreateDepthStencilTexture(
			width, height,
			mPostProcessedDsv.srvIndex,
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

		mPingRtv[0] = mRenderer->CreateRenderTargetTexture(
			width, height,
			offscreenClearColor,
			kBufferFormat
		);

		mPingRtv[1] = mRenderer->CreateRenderTargetTexture(
			width, height,
			offscreenClearColor,
			kBufferFormat
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
				Warning(
					"Engine",
					"Editor mode is now {}",
					std::to_string(mIsEditorMode)
				);
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
		ConVarManager::RegisterConVar<int>("fps_max", kDefaultFpsMax,
		                                   "Frame rate limiter");
		ConVarManager::RegisterConVar<std::string>("name", "unnamed",
		                                           "Current user name",
		                                           ConVarFlags::ConVarFlags_Notify);
		Console::SubmitCommand("name " + WindowsUtils::GetWindowsUserName(),
		                       true);
		ConVarManager::RegisterConVar<float>("sensitivity", 2.0f,
		                                     "Mouse sensitivity.");
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
		ConVarManager::RegisterConVar<float>("sv_maxspeed", 320.0f,
		                                     "Maximum speed a player can move.");
		ConVarManager::RegisterConVar<float>("sv_stopspeed", 100.0f,
		                                     "Minimum stopping speed when on ground.");
		ConVarManager::RegisterConVar<
			float>("sv_friction", 4.0f, "World friction.");
		ConVarManager::RegisterConVar<float>("sv_stepsize", 18.0f,
		                                     "Maximum step height.");
		ConVarManager::RegisterConVar<float>("sv_overbounce", 1.05f,
		                                     "Overbounce factor.");

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

		Console::SubmitCommand("bind 4 +bounds", true);
		Console::SubmitCommand("bind 1 +translate", true);
		Console::SubmitCommand("bind 2 +rotate", true);
		Console::SubmitCommand("bind 3 +scale", true);
		Console::SubmitCommand("bind tab +toggleGizmo", true);
	}

	void Engine::Quit([[maybe_unused]] const std::vector<std::string>& args) {
		mWishShutdown = true;
	}

	void Engine::CheckEditorMode() {
		if (mIsEditorMode) {
			mEditor = std::make_unique<Editor>(
				mSceneManager.get(),
				mTimeSystem->GetGameTime()
			);
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
	float                            Engine::blurStrength     = 0.0f;

	Vec2 Engine::mViewportLT   = Vec2::zero;
	Vec2 Engine::mViewportSize = Vec2::zero;

#ifdef _DEBUG
	bool Engine::mIsEditorMode = true;
#else
	bool Engine::mIsEditorMode = false;
#endif
}
