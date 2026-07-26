#ifdef _DEBUG
#include "EditorToolHost.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <core/string/StrUtil.h>

#include "engine/editor/EditorGuiScriptPanel.h"
#include "engine/editor/EditorNotification.h"
#include "engine/editor/GuiEditorTool.h"
#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/render/RenderModule.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/editor/lua/EditorLuaSystem.h"

namespace Unnamed {
	static constexpr std::string_view kChannel        = "EdTlHost";
	static constexpr float            kTitleBarHeight = 34.0f;
	static constexpr float            kWindowPadding  = 8.0f;

	EditorToolHost::EditorToolHost(
		ConsoleSystem*        console,
		InputSystem*          inputSystem,
		AssetManager*         assetManager,
		IDemoService*         demoService,
		IGameWorldFactory&    gameWorldFactory,
		Profiler*             profiler,
		WindowManager&        windowManager,
		Render::RenderModule& renderModule,
		ImGuiLayer&           imGuiLayer
	) : mWindowManager(windowManager),
	    mRenderModule(renderModule),
	    mImGuiLayer(imGuiLayer),
	    mConsole(console),
	    mInputSystem(inputSystem),
	    mAssetManager(assetManager),
	    mDemoService(demoService),
	    mGameWorldFactory(gameWorldFactory),
	    mProfiler(profiler) {
		if (!mConsole) {
			Error(
				kChannel, "ConsoleSystemが見つかりませんでした。エディタツールホストは正しく動作しません。"
			);
		}

		RegisterTool(
			std::make_unique<LevelEditorTool>(mWindowManager, mImGuiLayer)
		);
		RegisterTool(std::make_unique<GuiEditorTool>(mImGuiLayer));
	}

	EditorToolHost::~EditorToolHost() {
		Shutdown();
	}

	void EditorToolHost::Initialize() {
		const EditorToolServices services = {
			.windowManager    = &mWindowManager,
			.renderModule     = &mRenderModule,
			.imGuiLayer       = &mImGuiLayer,
			.console          = mConsole,
			.inputSystem      = mInputSystem,
			.assetManager     = mAssetManager,
			.demoService      = mDemoService,
			.gameWorldFactory = &mGameWorldFactory,
			.profiler         = mProfiler,
		};
		for (const auto& tool : mOwnedTools) {
			if (!tool) {
				continue;
			}
			tool->Initialize(services);
		}

		mEditorLuaSystem = std::make_unique<EditorLuaSystem>();
		mEditorLuaSystem->Init();

		mEditorGuiScriptPanel = std::make_unique<EditorGuiScriptPanel>();
		if (mEditorGuiScriptPanel) {
			mEditorGuiScriptPanel->Initialize(
				mAssetManager, mEditorLuaSystem.get()
			);

			mEditorGuiScriptPanel->SetScriptPath(
				VirtualPath::ParseOrThrow("editorgui/test.edgui.lua")
			);
		}

		mAssetManager->RegisterReload(
			[this](AssetID) {
				mEditorGuiScriptPanel->Reload();
			}
		);

		mNotification = std::make_unique<EditorNotification>();
	}

	void EditorToolHost::Shutdown() {
		for (const auto& tool : mOwnedTools) {
			if (!tool) {
				continue;
			}
			tool->Shutdown();
		}

		mEditorLuaSystem->Shutdown();

		mMainDockInitialized = false;
	}

	void EditorToolHost::BeginUI() const {
		for (auto& tool : mOwnedTools) {
			if (!tool || !tool->IsOpen()) {
				continue;
			}
			tool->BeginUI();
		}
	}

	void EditorToolHost::Tick(
		const EditorToolFrameContext& frameContext
	) const {
		for (auto& tool : mOwnedTools) {
			if (!tool || !tool->IsOpen()) {
				continue;
			}
			tool->Tick(frameContext);
		}
	}

	void EditorToolHost::BuildUi(const EditorToolFrameContext& frameContext) {
		constexpr float framePadding = 12.0f; // メニューバーを高くする
		constexpr auto  logoColor    = ImVec4(0.13f, 0.5f, 1.0f, 1.0f);

		// FramePadding
		ImGui::PushStyleVar(
			ImGuiStyleVar_FramePadding, ImVec2(framePadding, framePadding)
		);

		if (ImGui::BeginMainMenuBar()) {
			// WindowPadding
			ImGui::PushStyleVar(
				ImGuiStyleVar_WindowPadding,
				ImVec2(kWindowPadding, kWindowPadding)
			);

			//-----------------------------------------------------------------

			// ロゴメニュー
			{
				// Text
				ImGui::PushStyleColor(
					ImGuiCol_Text, logoColor
				);

				// 前後にスペースを入れてアイコンをテキストとして表示
				const std::string iconText =
					" " + StrUtil::ConvertToUtf8(kIconArrowBack) + " ";

				if (ImGuiWidgets::BeginMainMenu(iconText.c_str(),
				                                kTitleBarHeight)) {
					ImGui::PopStyleColor(); // Text
					if (
						ImGuiWidgets::MenuItemWithIcon(
							"About Unnamed", kIconInfo
						)
					) {
						mShowAbout = true;
					}
					ImGui::EndMenu();
				} else {
					ImGui::PopStyleColor();
				}
			}

			// File メニュー
			if (ImGuiWidgets::BeginMainMenu("File", kTitleBarHeight)) {
				if (ImGuiWidgets::MenuItemWithIcon("Exit", kIconPower)) {
					if (mConsole) {
						mConsole->ExecuteCommand("quit");
					}
				}
				ImGui::EndMenu();
			}

			// Edit メニュー
			{
				if (ImGuiWidgets::BeginMainMenu("Edit", kTitleBarHeight)) {
					(void)ImGuiWidgets::MenuItemWithIcon(
						"Settings", kIconSettings
					);
					ImGui::EndMenu();
				}
			}

			// Window メニュー
			{
				if (ImGuiWidgets::BeginMainMenu("Window", kTitleBarHeight)) {
					for (const auto& tool : mOwnedTools) {
						if (!tool) {
							continue;
						}
						const bool open = tool->IsOpen();
						if (
							ImGuiWidgets::MenuItemWithIcon(
								std::string(tool->GetDisplayName()).c_str(),
								tool->GetIcon(),
								nullptr,
								open
							)
						) {
							tool->SetOpen(!open);
						}
					}

					if (mLevelTool) {
						const bool profilerOpen = mLevelTool->
							IsProfilerWindowOpen();
						if (ImGuiWidgets::MenuItemWithIcon(
							"Profiler",
							kIconAvgTime,
							nullptr,
							profilerOpen
						)) {
							mLevelTool->SetProfilerWindowOpen(!profilerOpen);
						}
					}

					ImGui::EndMenu();
				}
			}

			if (mLevelTool) {
				constexpr auto playColor = ImVec4(
					0.34f, 0.59f, 0.36f, 1.0f
				);
				constexpr auto stopColor = ImVec4(
					0.79f, 0.31f, 0.31f, 1.0f
				);
				ImVec2       available     = ImGui::GetContentRegionAvail();
				const ImVec2 windowPadding = ImGui::GetStyle().WindowPadding;
				ImGui::SameLine();

				ImGui::SetCursorPosY(
					ImGui::GetCursorPosY() + windowPadding.y * 0.25f
				);

				if (!mLevelTool->IsPlaying()) {
					ImGui::PushStyleColor(ImGuiCol_Text, playColor);
					if (ImGuiWidgets::IconButton(
						kIconPlay,
						nullptr,
						{available.y, available.y},
						3.0f
					)) {
						mLevelTool->StartPlayInEditor();
					}
					ImGui::PopStyleColor();
				} else {
					ImGui::PushStyleColor(ImGuiCol_Text, stopColor);
					if (ImGuiWidgets::IconButton(
						kIconStop,
						nullptr,
						{available.y, available.y},
						3.0f
					)) {
						mLevelTool->StopPlayInEditor();
					}
					ImGui::PopStyleColor();
				}
			}

			//-----------------------------------------------------------------

			ImGui::PopStyleVar(); // WindowPadding

			ImGui::EndMainMenuBar();
		}

		ImGui::PopStyleVar(); // FramePadding

		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);

			// DockSpaceには角丸も境界線もパディングもつけない
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(
				ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f)
			);

			constexpr ImGuiWindowFlags hostFlags =
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoCollapse |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoBringToFrontOnFocus |
				ImGuiWindowFlags_NoNavFocus |
				ImGuiWindowFlags_NoDocking;

			ImGui::Begin("DockSpace", nullptr, hostFlags);

			ImGui::PopStyleVar(3); // WinRound, WinBorder, WinPadding

			const ImGuiID dockSpaceId  = ImGui::GetID("MainEditorDockSpace");
			const ImVec2  dockNodeSize = ImGui::GetContentRegionAvail();
			ImGui::DockSpace(
				dockSpaceId,
				ImVec2(0.0f, 0.0f),
				ImGuiDockNodeFlags_None
			);

			if (!mMainDockInitialized) {
				if (dockNodeSize.x > 1.0f && dockNodeSize.y > 1.0f) {
					ImGui::DockBuilderRemoveNode(dockSpaceId);
					ImGui::DockBuilderAddNode(
						dockSpaceId,
						ImGuiDockNodeFlags_DockSpace
					);
					ImGui::DockBuilderSetNodeSize(dockSpaceId, dockNodeSize);

					ImGuiID       dockMain  = dockSpaceId;
					const ImGuiID dockRight = ImGui::DockBuilderSplitNode(
						dockMain,
						ImGuiDir_Right,
						0.4f,
						nullptr,
						&dockMain
					);
					ImGui::DockBuilderDockWindow("Level Editor", dockMain);
					ImGui::DockBuilderDockWindow("GUI Editor", dockRight);
					ImGui::DockBuilderFinish(dockSpaceId);
					mMainDockInitialized = true;
				}
			}

			ImGui::End();
		}

		for (const auto& tool : mOwnedTools) {
			if (!tool || !tool->IsOpen()) {
				continue;
			}
			tool->BuildUi(frameContext);
		}

		if (mShowAbout) {
			ImGuiWidgets::ShowAboutWindow(
				"Unnamed Engine",
				std::string(ENGINE_VERSION),
				kIconArrowBack,
				mShowAbout
			);
		}

		mEditorGuiScriptPanel->Draw();
		mNotification->Update(frameContext.deltaTime);

		ImGui::ShowDemoWindow();
	}

	void EditorToolHost::CollectRenderViews(
		Render::RenderFrameInputs& inputs
	) const {
		for (auto& tool : mOwnedTools) {
			if (!tool || !tool->IsOpen()) {
				continue;
			}
			tool->CollectRenderViews(inputs);
		}
	}

	void EditorToolHost::SyncViewOutputs() const {
		for (auto& tool : mOwnedTools) {
			if (!tool || !tool->IsOpen()) {
				continue;
			}

			std::vector<std::string> viewKeys;
			tool->EnumerateViewKeys(viewKeys);
			for (const std::string& key : viewKeys) {
				tool->SetViewOutput(
					key,
					mRenderModule.GetViewOutputView(key),
					mRenderModule.GetViewOutputSize(key)
				);
			}
		}
	}

	void EditorToolHost::SyncPresentationState() const {
		if (!mLevelTool) {
			return;
		}
		mLevelTool->SyncPresentationState();
	}

	World* EditorToolHost::GetRuntimeWorld() const {
		return mLevelTool ? mLevelTool->GetRuntimeWorld() : nullptr;
	}

	void EditorToolHost::TogglePresentMode() const {
		if (!mLevelTool) {
			return;
		}
		mLevelTool->TogglePresentMode();
	}

	EDITOR_PRESENT_MODE EditorToolHost::GetPresentMode() const {
		if (!mLevelTool) {
			return EDITOR_PRESENT_MODE::VIEWPORT_PANEL;
		}
		return mLevelTool->GetPresentMode();
	}

	void EditorToolHost::RegisterTool(std::unique_ptr<IEditorTool> tool) {
		if (!tool) {
			return;
		}

		if (auto* levelTool = dynamic_cast<LevelEditorTool*>(tool.get())) {
			mLevelTool = levelTool;
		}

		mOwnedTools.emplace_back(std::move(tool));
	}
}

#endif
